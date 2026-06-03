# thomaz-api CD Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Automate build, migrate, and deploy of `thomaz-api` to the Lightsail VM `services` on every push to `main` touching `api/**`, running under PM2 behind Apache TLS, reusing the existing PostgreSQL container.

**Architecture:** GitHub Actions runs tests + type-check on a runner, then (on `main` only) SSHes into the VM with the `baseup` key, `git reset`s a sparse checkout of the monorepo, and runs a committed deploy script: `npm ci` → Prisma generate/migrate → SWC build → `pm2 reload`. A one-time bootstrap (swap, DB/user, clone, `.env`, first start, reverse proxy) is done over SSH.

**Tech Stack:** Node 24, Fastify 5, Prisma 6, PostgreSQL 18, SWC, PM2, GitHub Actions, Apache (Bitnami) + Let's Encrypt.

**Reference spec:** `docs/superpowers/specs/2026-06-03-api-cd-design.md`

**Conventions / facts (do not re-derive):**
- VM host `3.209.35.78`, user `bitnami`, SSH key at `~/.ssh/baseup.pem` (local, perms 600).
- Apps live in `/home/bitnami/apps/<name>`; PM2 6.0.14; Node v24 via login shell; `gh` authed on VM.
- Postgres container name `postgres` (image `postgres:18`), superuser `postgres`, listening `localhost:5432`. Existing DBs: `postgres`, `zapi`.
- Ports taken: 5678 (n8n), 3331 (zapi), 4444 (loop). **thomaz-api uses 3000.**
- Apache vhost helper: `~/apps/apache.sh <domain> <port>` (needs LE cert pre-existing).
- API has `GET /health` → `{ "status": "ok" }`. Prisma migrations exist under `api/prisma/migrations/`.
- Subdomain: `api.thomaz.baseup.cc`. DB user `thomaz` password: random, generated in bootstrap.
- Commit identity: `luizfbalves <luizzbanndera@gmail.com>`.
- **Production SSH actions require explicit user confirmation** (security classifier). Tasks 5–8 act on the live VM.

---

## File Structure

Repo changes (all under the monorepo `thomaz`):
- `api/.swcrc` — SWC transpile config (create)
- `api/package.json` — scripts + devDeps (modify)
- `api/ecosystem.config.cjs` — PM2 app definition (create)
- `api/scripts/deploy.sh` — VM-side build+migrate+restart, version-controlled (create)
- `.github/workflows/api.yml` — add typecheck step + deploy job (modify)

VM-only (not committed): swapfile, Postgres `thomaz` DB/user, sparse clone at `~/apps/thomaz`, `~/apps/thomaz/api/.env`, Apache vhosts, PM2 process registration.

---

## Task 1: SWC build setup in `api/`

**Files:**
- Create: `api/.swcrc`
- Modify: `api/package.json`

- [ ] **Step 1: Add SWC dev dependencies**

```bash
cd api
npm install -D @swc/core@^1 @swc/cli@^0.4
```
Expected: `@swc/core` and `@swc/cli` added to `devDependencies`, lockfile updated.

- [ ] **Step 2: Create `api/.swcrc`**

```json
{
  "$schema": "https://swc.rs/schema.json",
  "jsc": {
    "parser": { "syntax": "typescript", "decorators": false },
    "target": "es2022",
    "loose": false
  },
  "module": { "type": "es6" },
  "sourceMaps": true
}
```

- [ ] **Step 3: Update `api/package.json` scripts**

Replace the `scripts` block so `build` uses SWC and a `typecheck` script is added:
```json
  "scripts": {
    "dev": "tsx watch src/index.ts",
    "build": "swc src -d dist --strip-leading-paths",
    "typecheck": "tsc --noEmit",
    "start": "node --env-file=.env dist/index.js",
    "db:migrate": "prisma migrate dev",
    "db:deploy": "prisma migrate deploy",
    "db:generate": "prisma generate",
    "test": "vitest run",
    "test:watch": "vitest"
  },
```
Note: `tsc` stays a devDependency (now used only by `typecheck` and editor tooling); SWC no longer emits `.d.ts`, which is fine for an application (not a library).

- [ ] **Step 4: Verify build emits a runnable ESM tree**

```bash
cd api
rm -rf dist
npm run build
ls dist/index.js dist/app.js dist/routes/auth.js
```
Expected: all three files exist (flat layout mirroring `src/`, not `dist/src/...`).

- [ ] **Step 5: Verify type-check passes**

Run: `cd api && npm run typecheck`
Expected: no output, exit 0.

- [ ] **Step 6: Verify the built server boots**

```bash
cd api
node --env-file=.env -e "import('./dist/app.js').then(m=>m.buildApp()).then(({app})=>app.inject({method:'GET',url:'/health'})).then(r=>{console.log(r.statusCode, r.body); process.exit(r.statusCode===200?0:1)})"
```
Expected: `200 {"status":"ok"}` (requires local `.env` + local Postgres on 5433 from `docker-compose.yml`; if no local DB, start it first with `docker compose up -d`).

- [ ] **Step 7: Verify tests still pass**

Run: `cd api && npm test`
Expected: vitest suite passes.

- [ ] **Step 8: Commit**

```bash
git add api/.swcrc api/package.json api/package-lock.json
git -c user.name="luizfbalves" -c user.email="luizzbanndera@gmail.com" \
  commit -m "build(api): use SWC for emit, tsc for typecheck only"
```

---

## Task 2: PM2 ecosystem + version-controlled deploy script

**Files:**
- Create: `api/ecosystem.config.cjs`
- Create: `api/scripts/deploy.sh`

- [ ] **Step 1: Create `api/ecosystem.config.cjs`**

```js
// PM2 process definition for thomaz-api on the Lightsail VM.
// cwd is the deployed path; env is loaded by Node via --env-file=.env.
module.exports = {
  apps: [
    {
      name: 'thomaz-api',
      script: 'dist/index.js',
      cwd: '/home/bitnami/apps/thomaz/api',
      node_args: '--env-file=.env',
      instances: 1,
      exec_mode: 'fork',
      max_memory_restart: '300M',
      autorestart: true,
    },
  ],
}
```

- [ ] **Step 2: Create `api/scripts/deploy.sh`**

This runs ON the VM, with cwd already at the freshly-pulled `api/` (the workflow does the
`git reset` before invoking it, so the script itself is the up-to-date version).
```bash
#!/usr/bin/env bash
# Build + migrate + (re)start thomaz-api under PM2. Run from the api/ directory on the VM.
# Non-disruptive: does NOT touch n8n / zapi / loop / postgres.
set -euo pipefail

# Node/PM2 are not on the non-interactive PATH; make them available.
export PATH="/opt/bitnami/node/bin:$HOME/.npm-global/bin:$PATH"
[ -s "$HOME/.nvm/nvm.sh" ] && . "$HOME/.nvm/nvm.sh"

log() { printf '\n[%s] %s\n' "$(date -Iseconds)" "$*"; }

log "Installing dependencies (npm ci)..."
npm ci

log "Generating Prisma client..."
npx prisma generate

log "Applying database migrations..."
npx prisma migrate deploy

log "Building with SWC..."
npm run build

log "Reloading PM2 process..."
if pm2 describe thomaz-api > /dev/null 2>&1; then
  pm2 reload thomaz-api --update-env
else
  pm2 start ecosystem.config.cjs
fi
pm2 save

log "Done."
pm2 describe thomaz-api | grep -E "status|restarts" || true
```

- [ ] **Step 3: Make the script executable**

```bash
chmod +x api/scripts/deploy.sh
```

- [ ] **Step 4: Verify shell syntax**

Run: `bash -n api/scripts/deploy.sh && echo "syntax ok"`
Expected: `syntax ok`.

- [ ] **Step 5: Verify ecosystem config parses**

Run: `node -e "const c=require('./api/ecosystem.config.cjs'); if(c.apps[0].name!=='thomaz-api') process.exit(1); console.log('ecosystem ok')"`
Expected: `ecosystem ok`.

- [ ] **Step 6: Commit**

```bash
git add api/ecosystem.config.cjs api/scripts/deploy.sh
git -c user.name="luizfbalves" -c user.email="luizzbanndera@gmail.com" \
  commit -m "feat(api): add PM2 ecosystem and VM deploy script"
```

---

## Task 3: GitHub Actions — typecheck step + deploy job

**Files:**
- Modify: `.github/workflows/api.yml`

- [ ] **Step 1: Add a typecheck step to the existing `test` job**

After the "Apply migrations" step and before "Run tests", insert:
```yaml
      - name: Type-check
        run: npm run typecheck
```

- [ ] **Step 2: Add the `deploy` job at the end of the file**

```yaml
  deploy:
    name: Deploy to Lightsail
    needs: test
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    runs-on: ubuntu-latest
    steps:
      - name: Deploy over SSH
        uses: appleboy/ssh-action@v1.2.0
        with:
          host: ${{ secrets.LIGHTSAIL_HOST }}
          username: ${{ secrets.LIGHTSAIL_USER }}
          key: ${{ secrets.LIGHTSAIL_SSH_KEY }}
          script_stop: true
          command_timeout: "20m"
          script: |
            set -euo pipefail
            cd ~/apps/thomaz
            git fetch origin main
            git reset --hard origin/main
            bash api/scripts/deploy.sh
```
Note: the `git reset` runs before `deploy.sh`, so the freshly-pulled script executes.

- [ ] **Step 3: Validate the workflow YAML**

Run (if `actionlint` available): `actionlint .github/workflows/api.yml`
Otherwise: `python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/api.yml')); print('yaml ok')"`
Expected: no errors / `yaml ok`.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/api.yml
git -c user.name="luizfbalves" -c user.email="luizzbanndera@gmail.com" \
  commit -m "ci(api): typecheck step + deploy job to Lightsail on main"
```

---

## Task 4: Configure GitHub repository secrets

**Files:** none (operational, via `gh` CLI authenticated locally as `luizfbalves`).

- [ ] **Step 1: Set host and user secrets**

```bash
gh secret set LIGHTSAIL_HOST  --repo luizfbalves/thomaz --body "3.209.35.78"
gh secret set LIGHTSAIL_USER  --repo luizfbalves/thomaz --body "bitnami"
```
Expected: `✓ Set secret LIGHTSAIL_HOST` / `LIGHTSAIL_USER`.

- [ ] **Step 2: Set the SSH private key secret**

```bash
gh secret set LIGHTSAIL_SSH_KEY --repo luizfbalves/thomaz < ~/.ssh/baseup.pem
```
Expected: `✓ Set secret LIGHTSAIL_SSH_KEY`.

- [ ] **Step 3: Verify all three exist**

Run: `gh secret list --repo luizfbalves/thomaz`
Expected: `LIGHTSAIL_HOST`, `LIGHTSAIL_USER`, `LIGHTSAIL_SSH_KEY` listed.

---

## Task 5: VM bootstrap — swapfile + Postgres database/user

**Files:** none (operational, over SSH — requires user confirmation).

SSH prefix for all steps: `ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 '<cmd>'`

- [ ] **Step 1: Create a 2 GB swapfile (idempotent)**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 '
  if ! sudo swapon --show | grep -q /swapfile; then
    sudo fallocate -l 2G /swapfile && sudo chmod 600 /swapfile &&
    sudo mkswap /swapfile && sudo swapon /swapfile &&
    grep -q "/swapfile" /etc/fstab || echo "/swapfile none swap sw 0 0" | sudo tee -a /etc/fstab
  fi
  free -m | grep -i swap'
```
Expected: Swap total ≈ 2048 MB.

- [ ] **Step 2: Generate a random DB password and create the `thomaz` DB + user (idempotent)**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 '
  PW=$(openssl rand -hex 24)
  docker exec postgres psql -U postgres -v ON_ERROR_STOP=1 <<SQL
DO \$\$ BEGIN
  IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '"'"'thomaz'"'"') THEN
    CREATE ROLE thomaz LOGIN PASSWORD '"'"'$PW'"'"';
  ELSE
    ALTER ROLE thomaz WITH PASSWORD '"'"'$PW'"'"';
  END IF;
END \$\$;
SELECT '"'"'CREATE DATABASE thomaz OWNER thomaz'"'"'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '"'"'thomaz'"'"')\gexec
SQL
  echo "GENERATED_DB_PASSWORD=$PW"'
```
Expected: prints `GENERATED_DB_PASSWORD=<hex>`. **Record this value — it goes into `.env` in Task 6.**

- [ ] **Step 3: Verify the database exists and the user can connect**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 \
  'docker exec postgres psql -U postgres -tc "SELECT datname FROM pg_database WHERE datname='"'"'thomaz'"'"';"'
```
Expected: a row `thomaz`.

---

## Task 6: VM bootstrap — sparse clone, `.env`, first deploy, PM2 start

**Files:** creates `~/apps/thomaz/` (sparse) and `~/apps/thomaz/api/.env` on the VM.

- [ ] **Step 1: Sparse-clone the monorepo (only `api/`)**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 '
  cd ~/apps
  if [ ! -d thomaz/.git ]; then
    git clone --filter=blob:none --no-checkout https://github.com/luizfbalves/thomaz.git thomaz
    cd thomaz
    git sparse-checkout init --cone
    git sparse-checkout set api
    git checkout main
  fi
  ls ~/apps/thomaz/api/package.json && echo "clone ok"'
```
Expected: `clone ok` (the `api/` tree present; C++ homebrew tree excluded).

- [ ] **Step 2: Create `~/apps/thomaz/api/.env`** (substitute `<DB_PW>` from Task 5 Step 2, and generate a JWT secret)

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 '
  JWT=$(openssl rand -hex 32)
  cat > ~/apps/thomaz/api/.env <<ENV
DATABASE_URL="postgresql://thomaz:<DB_PW>@localhost:5432/thomaz?schema=public"
JWT_SECRET="$JWT"
PORT=3000
HOST=127.0.0.1
PUBLIC_BASE_URL="https://api.thomaz.baseup.cc"
UPLOAD_DIR="./uploads"
CORS_ORIGINS="https://api.thomaz.baseup.cc"
JWT_ACCESS_EXPIRES="15m"
REFRESH_TOKEN_TTL_DAYS=7
NODE_ENV="production"
ENV
  chmod 600 ~/apps/thomaz/api/.env
  echo "env written"'
```
Expected: `env written`. (`HOST=127.0.0.1` — only Apache reaches it. `PUBLIC_BASE_URL` set ahead of the proxy in Task 7; the API does not need TLS live to boot.)

- [ ] **Step 3: Run the first deploy (build + migrate + PM2 start)**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 \
  'cd ~/apps/thomaz/api && bash scripts/deploy.sh'
```
Expected: ends with PM2 showing `thomaz-api` `online`.

- [ ] **Step 4: Enable PM2 on boot (idempotent)**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 'bash -lc "pm2 save && pm2 startup | tail -1"'
```
If `pm2 startup` prints a `sudo env ... pm2 startup systemd -u bitnami ...` command, run it once (it requires sudo). Expected: PM2 startup hook installed (may already be from existing apps).

- [ ] **Step 5: Verify the API answers locally on the VM**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 'curl -fsS http://127.0.0.1:3000/health'
```
Expected: `{"status":"ok"}`.

---

## Task 7: Reverse proxy + TLS for `api.thomaz.baseup.cc`

**Files:** Apache vhosts on the VM (created by `~/apps/apache.sh`).

- [ ] **Step 1: USER ACTION — create DNS record**

In the `baseup.cc` DNS provider, add an `A` record: `api.thomaz` → `3.209.35.78`.
Wait for propagation before continuing.

- [ ] **Step 2: Verify DNS resolves to the VM**

```bash
dig +short api.thomaz.baseup.cc
```
Expected: `3.209.35.78`. Do not proceed until this matches (Let's Encrypt validation needs it).

- [ ] **Step 3: Obtain the Let's Encrypt certificate**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 \
  'sudo /opt/bitnami/bncert-tool --target-domains api.thomaz.baseup.cc --non-interactive 2>/dev/null || echo "use interactive bncert if this fails"'
```
Expected: cert files at `/opt/bitnami/letsencrypt/certificates/api.thomaz.baseup.cc.{crt,key}`.
If `bncert-tool` is unavailable/interactive-only, run `sudo /opt/bitnami/bncert-tool` interactively and add the domain.

- [ ] **Step 4: Generate the Apache vhosts and restart**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 \
  '~/apps/apache.sh api.thomaz.baseup.cc 3000'
```
Expected: `Concluído: api.thomaz.baseup.cc -> 127.0.0.1:3000` and Apache restarts cleanly.

- [ ] **Step 5: Verify public HTTPS access**

```bash
curl -fsS https://api.thomaz.baseup.cc/health
```
Expected: `{"status":"ok"}` over TLS.

---

## Task 8: End-to-end CD verification

**Files:** none (validates the whole pipeline with a real push).

- [ ] **Step 1: Merge the repo changes to `main`**

Merge the branch carrying Tasks 1–3 into `main` (PR or fast-forward). The path filter
`api/**` + the new commits will trigger the `api` workflow.

- [ ] **Step 2: Watch the workflow run**

```bash
gh run watch --repo luizfbalves/thomaz $(gh run list --repo luizfbalves/thomaz --workflow api.yml --limit 1 --json databaseId -q '.[0].databaseId')
```
Expected: both `test` and `deploy` jobs succeed.

- [ ] **Step 3: Confirm the deploy reached the VM**

```bash
ssh -i ~/.ssh/baseup.pem bitnami@3.209.35.78 \
  'cd ~/apps/thomaz && git rev-parse --short HEAD; bash -lc "pm2 describe thomaz-api | grep -E \"status|uptime\""'
```
Expected: VM HEAD matches the latest `main` commit; `thomaz-api` `online` with a fresh uptime.

- [ ] **Step 4: Confirm the live endpoint still serves**

```bash
curl -fsS https://api.thomaz.baseup.cc/health
```
Expected: `{"status":"ok"}`.

---

## Notes & rollback

- **Migration failure** during deploy: `set -euo pipefail` aborts `deploy.sh` before `pm2 reload`, so the previous build keeps serving. Inspect with `npx prisma migrate status` on the VM.
- **Prisma 6 vs PostgreSQL 18**: first `migrate deploy` (Task 6 Step 3) is the real compatibility test. If it errors on an unsupported feature, fall back to a `postgres:17` container for the `thomaz` DB (out-of-band) — but this is not expected, as the migrations use standard SQL.
- **Secret rotation**: `baseup` is a personal key reused as a deploy secret. Follow-up: generate a dedicated deploy keypair, add the public half to the VM's `~/.ssh/authorized_keys`, and replace `LIGHTSAIL_SSH_KEY`.
- **Memory**: watch `pm2 describe thomaz-api` mem and `free -m` after first real traffic; `max_memory_restart: 300M` + 2 GB swap are the guards.
