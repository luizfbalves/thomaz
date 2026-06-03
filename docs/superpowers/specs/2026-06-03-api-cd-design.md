# CD Pipeline: thomaz-api → AWS Lightsail

**Date:** 2026-06-03
**Status:** Approved — pending implementation plan
**Author:** luizfbalves (with Claude)

## Goal

Automate deployment of `thomaz-api` (Fastify + Prisma + PostgreSQL) to the existing
AWS Lightsail VM `services` on every push to `main` that touches `api/**`. Reuse the
PostgreSQL container already running on the VM, run the API under PM2, and expose it
publicly through Apache reverse proxy with TLS.

CI (test + typecheck) already partially exists in `.github/workflows/api.yml`; this
work adds the CD half and the build/runtime changes needed to support it.

## Context: target VM (`services`)

- **Bundle:** `small_3_0` — 2 vCPU, 2 GB RAM, 60 GB SSD, static IP `3.209.35.78` (`us-east-1`).
- **Already running (Docker):** `n8n` (5678), `redis` (6379), `postgres:18` (5432).
- **Already running (PM2):** `loop-api` (~199 MB), `zapi` (~192 MB).
- **Runtime:** Node v24.13.1 (login-shell PATH only), PM2 6.0.14, Bun, git, `gh`.
- **Memory:** ~696 MB available, **swap = 0** → OOM risk; mitigated by adding swap.
- **CPU:** ~1% average — abundant headroom.
- **Established app convention:** apps live in `~/apps/<name>`, managed by PM2 via
  per-app `ecosystem.config.cjs` aggregated in `~/apps/ecosystem.config.js`; exposed as
  `<name>.baseup.cc` Apache vhosts (http+https) proxying to `127.0.0.1:<port>`; GitHub
  auth on the VM via `gh auth git-credential`. Helper `~/apps/apache.sh <domain> <port>`
  generates both vhosts (requires the Let's Encrypt cert to already exist).
- **PostgreSQL:** superuser `postgres` (password held on the VM, not recorded here); existing databases `postgres`,
  `zapi`. The API will get a dedicated `thomaz` database + `thomaz` user in this same
  container (same pattern `zapi` already uses).

## Key decisions

| Decision | Choice | Rationale |
|---|---|---|
| Database | New DB + user inside the **existing** `postgres:18` container | Saves ~150–250 MB RAM vs a second Postgres; matches `zapi` pattern |
| Deploy trigger | Push to `main`, path filter `api/**` | Continuous, simple |
| Process manager | PM2 (`thomaz-api`) | Matches existing apps; auto-restart, boot persistence |
| Build location | **On the VM** (git pull → build) | Consistent with `zapi`; proven on this box |
| Transpiler | **SWC** (not `tsc`) for emit; `tsc --noEmit` for type-check in CI | Light/fast build keeps RAM spike off the 2 GB VM |
| CI SSH access | Reuse `baseup` key as a GitHub secret | User preference |
| Public exposure | Apache reverse proxy + Let's Encrypt, subdomain of `baseup.cc` | Matches n8n/zapi/loop convention |
| App port | `3000` | Free (5678/3331/4444 taken) |

## Architecture

```
push to main (api/**)
        │
        ▼
GitHub Actions  ── job: test ──> postgres service container
   (api.yml)         • npm ci • prisma generate • migrate deploy
                     • npm test • npm run typecheck (tsc --noEmit)
        │ needs: test, if push on main
        ▼
   ── job: deploy ──> ssh (baseup key) ──> VM ~/apps/deploy-thomaz-api.sh
                                                │
        ┌───────────────────────────────────────┘
        ▼
  VM: ~/apps/thomaz (sparse checkout: api/ only)
        • git fetch + reset --hard origin/main
        • cd api && npm ci
        • npx prisma generate && npx prisma migrate deploy  ──> postgres:18 (db: thomaz)
        • npm run build  (swc src -d dist)
        • pm2 reload thomaz-api  (node --env-file=.env dist/index.js, :3000)
        │
        ▼
  Apache vhost <subdomain>.baseup.cc :80/:443  ──proxy──> 127.0.0.1:3000
```

## Components

### 1. Build with SWC (`api/`)
- Add devDeps `@swc/core`, `@swc/cli`.
- `.swcrc`: TypeScript parser, `target: es2022`, `module.type: es6` (ESM). Source already
  uses explicit `.js` import specifiers (NodeNext), so SWC output runs under Node ESM as-is.
- `package.json` scripts:
  - `build`: `swc src -d dist --strip-leading-paths`
  - `typecheck`: `tsc --noEmit`
  - `start`: `node --env-file=.env dist/index.js` (Node 24 native env loading; `config.ts`
    reads `process.env` only — no dotenv dependency)
- `tsc` stays a devDep for type-checking and `prisma generate` only — no longer emits.

### 2. GitHub Actions (`.github/workflows/api.yml`)
- **`test` job** (extend existing): add a `npm run typecheck` step after install.
- **`deploy` job** (new):
  - `needs: test`
  - `if: github.event_name == 'push' && github.ref == 'refs/heads/main'`
  - SSH into the VM using `LIGHTSAIL_SSH_KEY` and run `~/apps/deploy-thomaz-api.sh`.
- **Secrets** (set via `gh secret set`): `LIGHTSAIL_SSH_KEY` (baseup private key),
  `LIGHTSAIL_HOST` (`3.209.35.78`), `LIGHTSAIL_USER` (`bitnami`).

### 3. VM deploy script (`~/apps/deploy-thomaz-api.sh`)
Non-disruptive (does **not** touch n8n / zapi / loop / postgres):
```bash
set -euo pipefail
export PATH="/opt/bitnami/node/bin:$HOME/.npm-global/bin:$PATH"
[ -s "$HOME/.nvm/nvm.sh" ] && . "$HOME/.nvm/nvm.sh"
cd ~/apps/thomaz
git fetch origin main && git reset --hard origin/main
cd api
npm ci
npx prisma generate
npx prisma migrate deploy
npm run build
pm2 reload thomaz-api --update-env || pm2 start ecosystem.config.cjs
pm2 save
```

### 4. PostgreSQL provisioning (one-time)
```sql
CREATE USER thomaz WITH PASSWORD '<strong-password>';
CREATE DATABASE thomaz OWNER thomaz;
```
Run via `docker exec postgres psql -U postgres -c "..."`.
Connection string (API → localhost): `postgresql://thomaz:<pw>@localhost:5432/thomaz?schema=public`.

### 5. PM2 ecosystem (`api/ecosystem.config.cjs`, committed)
```js
module.exports = {
  apps: [{
    name: 'thomaz-api',
    script: 'dist/index.js',
    cwd: '/home/bitnami/apps/thomaz/api',
    node_args: '--env-file=.env',
    instances: 1,
    exec_mode: 'fork',
    max_memory_restart: '300M',
  }],
}
```
The VM-only `api/.env` (not committed) holds `DATABASE_URL`, `JWT_SECRET` (≥16 chars),
`PORT=3000`, `HOST=127.0.0.1`, `PUBLIC_BASE_URL`, `CORS_ORIGINS`, `NODE_ENV=production`,
`UPLOAD_DIR`, etc. Optionally register in `~/apps/ecosystem.config.js` aggregator for
consistency.

### 6. Swapfile (one-time)
2 GB swapfile to eliminate OOM risk (current swap = 0):
```bash
sudo fallocate -l 2G /swapfile && sudo chmod 600 /swapfile
sudo mkswap /swapfile && sudo swapon /swapfile
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

### 7. Reverse proxy + TLS (one-time)
- Subdomain: **`api.thomaz.baseup.cc`**.
- DNS A record `api.thomaz.baseup.cc → 3.209.35.78` (manual, in DNS provider; wait for propagation).
- Obtain Let's Encrypt cert for the subdomain (bncert / certbot) — required before `apache.sh`.
- Run `~/apps/apache.sh api.thomaz.baseup.cc 3000` to generate http+https vhosts.
- Set `PUBLIC_BASE_URL=https://api.thomaz.baseup.cc` and `CORS_ORIGINS` accordingly in `.env`.

## Bootstrap vs recurring

**One-time bootstrap** (executed via SSH during implementation):
swapfile → create DB/user → sparse-clone monorepo to `~/apps/thomaz` (only `api/`, excludes
the C++ homebrew tree) → create `api/.env` → first build → `pm2 start` + `pm2 save` →
DNS + cert + `apache.sh`.

**Recurring CD** (automatic): the `deploy` job on every push to `main` touching `api/**`.

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| OOM on 2 GB box (swap=0) during build/run | 2 GB swapfile + PM2 `max_memory_restart: 300M` |
| Prisma 6.8 vs PostgreSQL **18** (very new) | Migrations are standard SQL; validate on first `migrate deploy`; rollback plan = pin to pg 17 image if incompatible |
| Node not in non-interactive PATH | Deploy script sets PATH / sources nvm explicitly |
| Monorepo clone pulls unrelated homebrew tree | `git sparse-checkout set api` |
| `migrate deploy` failure mid-deploy | `set -euo pipefail` aborts before `pm2 reload`; previous version keeps running |
| baseup key exposure in CI | Stored as encrypted GitHub secret; consider rotating to a dedicated deploy key later |

## Out of scope
- Migrating to a dedicated/managed database.
- Dockerizing the API itself.
- Blue/green or zero-downtime multi-instance deploys.
- Automated DB backups (recommended follow-up).

## Confirmed
1. Subdomain: **`api.thomaz.baseup.cc`**.
2. `thomaz` Postgres user password: **randomly generated during bootstrap**, stored only in
   VM `api/.env` (never committed).
