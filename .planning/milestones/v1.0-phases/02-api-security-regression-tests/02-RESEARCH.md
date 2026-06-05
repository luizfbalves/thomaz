# Phase 2: API Security + Regression Tests - Research

**Researched:** 2026-06-04
**Domain:** Fastify 5 API hardening — JWT revocation (jti blocklist), Prisma/Postgres migration, pino logging, Vitest `app.inject()` regression tests
**Confidence:** HIGH (all four mechanisms verified against the actual repo code and official Fastify/@fastify/jwt docs)

## Summary

This phase is almost entirely about HOW to wire four well-understood mechanisms into an existing, already-working Fastify 5 + @fastify/jwt + Prisma/Postgres + Vitest codebase — not about choosing between approaches (every WHETHER decision is locked in CONTEXT.md). The codebase is small, internally consistent, and already demonstrates every pattern the phase needs: a single `app.authenticate` decorator enforcement point, a DB-backed `RefreshToken` model with a hand-numbered migration to mirror, an `app.inject()` + `form-data` multipart test that already exercises three of the four TEST-02 branches, and a `NODE_ENV` zod enum ready to key an `envToLogger` map.

The only genuinely new knowledge is the exact `@fastify/jwt` `jti` mechanism. It is built on `jsonwebtoken`, so `reply.jwtSign(payload, { jwtid: randomUUID(), expiresIn })` mints the `jti`, and the verified `request.user` exposes both `jti` and `exp` (unix seconds) — which is exactly what D-08 needs for `expiresAt = new Date(payload.exp * 1000)`. The blocklist check is a single conditional `findUnique` added inside the existing `authenticate` decorator after `jwtVerify()` succeeds.

**Primary recommendation:** Add `jti` via the `jwtid` sign option in both signing sites; add a `RevokedToken` model with a hand-numbered SQL migration mirroring `20250603150000_refresh_tokens`; gate the blocklist lookup on `payload.jti` being present (L-02/D-05) and wrap it in try/catch that fails open (D-06); replace `Fastify({ logger: false })` with an `envToLogger[NODE_ENV]` map carrying a shared `redact` array; extend `api/test/api.test.ts` in place using the existing `app.inject()` + `form-data` pattern.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Mint `jti` on access tokens | API (token signers) | — | `jti` is a server-issued claim; both `signAuthResponse` and `/auth/refresh` sign here |
| Enforce token revocation | API (`authenticate` decorator) | Database (`RevokedToken` lookup) | Single decorator is the existing enforcement point for every protected route (D-04) |
| Store revocation list | Database (Postgres) | — | L-01: Postgres `findUnique` on PK `jti` is sub-ms; no Redis |
| Best-effort logout revoke | API (`/auth/logout`) | Database (insert + lazy sweep) | D-01/D-09: read bearer, insert `jti`, `deleteMany` expired |
| Request logging + redaction | API (Fastify logger / pino) | — | DEBT-04: `envToLogger` map + `redact` paths, app-level concern |
| Regression verification | Test harness (Vitest + `app.inject()`) | — | TEST-01/TEST-02: in-process injection, no network |

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Locked upstream (ROADMAP planning flags — do not re-litigate):**
- **L-01:** SEC-02 uses a `jti` claim on access tokens + a Postgres `RevokedToken` table. **No Redis** — `findUnique` on the `jti` primary key is sub-millisecond.
- **L-02:** Tokens minted before deploy (without a `jti`) MUST pass the blocklist check unblocked — never rejected for lacking a `jti`.
- **L-03:** Refresh tokens are already DB-backed via `revokeRefreshToken`. **No change** to refresh-token handling.
- **L-04:** SEC-01 is verify-only. The fix (removing `@fastify/static`) landed in Phase 1; blobs stay in `uploads/saves/` (Phase 1 D-10). Phase 2 delivers TEST-01 as the regression guard.

**Logout contract (SEC-02):**
- **D-01:** `/auth/logout` reads the access token from the `Authorization: Bearer` header **best-effort** — if present and valid with a `jti`, insert that `jti` into `RevokedToken`. The endpoint stays effectively unauthenticated (no `authenticate` preHandler): a missing/expired/invalid bearer does **not** 401.
- **D-02:** Logout still revokes the `refreshToken` from the request body (unchanged behavior), and **always** returns `200 { ok: true }`.
- **D-03:** A blocklisted (logged-out) access token, when reused, returns the **generic** `401 { ok: false, error: "unauthorized" }` — the same shape as any invalid/expired token. No distinct `token_revoked` code.

**Blocklist enforcement:**
- **D-04:** The revocation check lives in the `authenticate` decorator (`api/src/plugins/auth.ts`), after `jwtVerify()` succeeds.
- **D-05:** **Skip the lookup when the verified token has no `jti`** — allow with no DB hit. Only query `RevokedToken` when a `jti` is present.
- **D-06:** **Fail-open on DB error** — if the lookup throws, log a warning and treat the token as not-revoked.

**RevokedToken lifecycle:**
- **D-07:** New Prisma model — `jti` (String `@id`), `userId` (String), `expiresAt` (DateTime), `createdAt` (DateTime `@default(now())`), with `@@index([expiresAt])`.
- **D-08:** `expiresAt` derived from the access token's `exp` claim (`new Date(payload.exp * 1000)`).
- **D-09:** **Lazy sweep on logout** — before inserting, `deleteMany({ where: { expiresAt: { lt: now } } })`. No scheduler/cron.

**Logging policy (DEBT-04):**
- **D-10:** `envToLogger` map keyed by `NODE_ENV`: `production` → pino JSON (`true`); `development` → pino-pretty; `test` → `false`. Adds `pino-pretty` as a **dev dependency**.
- **D-11:** Redact `req.headers.authorization` and `req.headers.cookie` via pino `redact`. Bodies are not serialized by default — keep it that way.

### Claude's Discretion
- **`jti` generation:** mint a unique `jti` (e.g. `randomUUID()`) on every access token, in **both** signing sites (`signAuthResponse` and the inline `reply.jwtSign` in `/auth/refresh`). Use `@fastify/jwt`'s `jti`/`jwtid` mechanism or include `jti` in the payload.
- **TEST-01 approach:** assert a direct path to a save blob returns 404. Exact fixture shape is the executor's call.
- **TEST-02:** the four revision branches already exist in `saves.ts` PUT; tests only. Use the existing `app.inject()` + multipart pattern.
- Commit granularity, migration naming, and test-file organization — planner/executor decide.

### Deferred Ideas (OUT OF SCOPE)
- **"Revoke all my sessions"** (bulk revocation by `userId`) — `userId` column added now to enable it, but the feature is out of scope.
- **Scheduled/cron purge of `RevokedToken`** — rejected in favor of lazy sweep on logout.
- **Distinct `token_revoked` error code** — deferred; would need C++ client handling first.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SEC-01 | Save blobs not downloadable without auth / by non-owner — verify-only | `@fastify/static` already absent from `app.ts` (verified: no `static` register call). Verification delivered as TEST-01. **Landmine:** `@fastify/static` is still a `package.json` dependency (see Pitfall 6) — TEST-01 guards behavior regardless. |
| SEC-02 | Revoked (logged-out) access token can no longer authenticate | `jwtid` sign option (jsonwebtoken passthrough); blocklist `findUnique` in `authenticate`; `RevokedToken` Prisma model + migration; logout best-effort bearer read. All mechanisms verified below. |
| DEBT-04 | Production logging via `envToLogger`; test stays silent; no secrets logged | Official Fastify `envToLogger` pattern + `redact` paths; pino-pretty dev dep; bodies not serialized by default. Verified against Fastify Logging docs. |
| TEST-01 | Regression test: no save-blob URL publicly accessible | `app.inject({ method:"GET", url:"/uploads/saves/..." })` → expect 404 (no route serves it). Pattern in existing test file. |
| TEST-02 | API tests cover four saves `PUT` revision branches | Three branches already covered in existing test (`revision_conflict`, new-slot 200, matching-revision 200); only `revision_required` (400) branch is missing. Existing `form-data` multipart pattern is the template. |
</phase_requirements>

## Standard Stack

Everything needed is already installed. The phase adds exactly one new package.

### Core (already present — verified in `api/package.json`)
| Library | Version (installed) | Purpose | Why Standard |
|---------|---------------------|---------|--------------|
| `fastify` | ^5.3.3 | HTTP framework + built-in pino logger | Already the app foundation |
| `@fastify/jwt` | ^9.1.0 | `reply.jwtSign` / `request.jwtVerify`; built on `jsonwebtoken` | Provides `jwtid` sign option + verified payload claims |
| `@prisma/client` / `prisma` | ^6.8.2 | ORM + migration tooling | `RevokedToken` model + `findUnique` on PK `jti` |
| `zod` | ^3.25.28 | Env + body validation | `NODE_ENV` enum already keys the logger map |
| `vitest` | ^3.1.4 | Test runner | `app.inject()` scaffold already in `api/test/api.test.ts` |
| `form-data` | ^4.0.5 (devDep) | Multipart bodies in tests | Already used for save-upload tests |

### Supporting (the one addition)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `pino-pretty` | ^13.1.3 | Dev-mode human-readable logs | **devDependency only** (D-10); referenced by the `development` branch of `envToLogger` |

**Note:** `pino` itself is NOT a direct dependency to add — Fastify 5 bundles `pino ^9` transitively `[VERIFIED: npm view fastify@5.3.3 dependencies.pino → ^9.0.0]`. The `redact` option is configured through Fastify's `logger` object, not by importing pino directly. `randomUUID` comes from the Node built-in `node:crypto` (already imported in `refresh-tokens.ts`) — no package needed.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `jwtid` sign option | `jti` inline in payload object | Both produce an identical token; `jwtid` is the documented jsonwebtoken option and keeps the payload object clean. Either satisfies Claude's Discretion. |
| `randomUUID()` | `randomBytes(16).toString("hex")` | Equivalent uniqueness; `randomUUID()` is more readable and already a Node built-in. |

**Installation:**
```bash
cd api && npm install --save-dev pino-pretty
```

**Version verification:**
- `pino-pretty` latest `13.1.3` `[VERIFIED: npm view pino-pretty version → 13.1.3, created 2018-04-04, repo github.com/pinojs/pino-pretty]`
- `fastify` bundles `pino ^9.0.0` `[VERIFIED: npm view fastify@5.3.3 dependencies.pino]`

## Package Legitimacy Audit

| Package | Registry | Age | Downloads | Source Repo | slopcheck | Disposition |
|---------|----------|-----|-----------|-------------|-----------|-------------|
| `pino-pretty` | npm | ~8 yrs (created 2018-04-04) | very high (Fastify-recommended, pinojs org) | github.com/pinojs/pino-pretty | unavailable | Approved — cited by official Fastify Logging docs |

**Packages removed due to slopcheck [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

slopcheck was not installable in this session. However, `pino-pretty` is not a discovered/assumed name — it is the package named in the **official Fastify Logging documentation** `[CITED: fastify.dev/docs/latest/Reference/Logging/]`, published by the same org (pinojs) that publishes pino (Fastify's own logger), and confirmed on the npm registry with an 8-year history and the canonical pinojs repo. It is tagged `[VERIFIED]` on the strength of the official-docs citation, not registry existence alone. No `checkpoint:human-verify` gate is required for this single, canonical package, but the planner may add one for defense-in-depth.

## Architecture Patterns

### System Architecture Diagram

```
                         ┌──────────────────────────────────────┐
   POST /auth/login      │  signAuthResponse (auth-tokens.ts)   │
   POST /auth/register ─▶│  reply.jwtSign({sub,username},       │──┐
                         │    { jwtid: randomUUID(), expiresIn })│  │  access token
                         └──────────────────────────────────────┘  │  (now carries jti)
   POST /auth/refresh ──▶  reply.jwtSign({sub,username},           │
                           { jwtid: randomUUID(), expiresIn }) ─────┤
                                                                    ▼
   Any protected route                              ┌─────────────────────────────┐
   (GET/PUT/DELETE /saves) ─ preHandler ───────────▶│ app.authenticate (auth.ts)  │
                                                     │ 1. await jwtVerify()        │
                                                     │ 2. if (!user.jti) → allow   │ (D-05/L-02)
                                                     │ 3. findUnique(RevokedToken) │──▶ Postgres
                                                     │    on jti                   │    RevokedToken
                                                     │ 4. found → 401 unauthorized │    (PK = jti)
                                                     │    err  → log warn + allow  │ (D-06 fail-open)
                                                     └─────────────────────────────┘
                         ┌──────────────────────────────────────────────┐
   POST /auth/logout ──▶ │ best-effort: read Bearer header              │
   (no authenticate     │  verify → if jti+exp:                         │
    preHandler, D-01)   │    deleteMany(expiresAt < now)  (D-09 sweep)  │──▶ Postgres
                         │    create({ jti, userId, expiresAt:exp*1000})│ (D-07/D-08)
                         │  revokeRefreshToken(body.refreshToken)       │ (D-02, unchanged)
                         │  ALWAYS return 200 { ok: true }              │
                         └──────────────────────────────────────────────┘

   Logging: Fastify({ logger: envToLogger[NODE_ENV] }) with shared redact:
            [req.headers.authorization, req.headers.cookie]  (DEBT-04)
```

### Recommended Project Structure (no new files required for src; all edits in-place)
```
api/
├── prisma/
│   ├── schema.prisma                  # + RevokedToken model
│   └── migrations/
│       └── 2026XXXXXXXXXX_revoked_tokens/migration.sql   # new, hand-numbered
├── src/
│   ├── app.ts                         # logger:false → envToLogger map
│   ├── plugins/auth.ts                # JwtPayload += jti?/exp?; blocklist check in authenticate
│   ├── lib/auth-tokens.ts             # signAuthResponse: add jwtid
│   └── routes/auth.ts                 # /auth/refresh: add jwtid; /auth/logout: best-effort revoke + sweep
└── test/api.test.ts                   # + TEST-01, TEST-02 (revision_required), revoked-token coverage
```
The planner MAY extract a small `lib/revoked-tokens.ts` helper (mirroring `refresh-tokens.ts`) for `revokeAccessJti(prisma, jti, userId, exp)` and `isJtiRevoked(prisma, jti)` — discretionary, keeps `auth.ts`/`authenticate` thin.

### Pattern 1: Mint `jti` via the `jwtid` sign option
**What:** `@fastify/jwt`'s `reply.jwtSign` / `app.jwt.sign` proxy to `jsonwebtoken.sign()`, which accepts a `jwtid` option that becomes the `jti` claim.
**When to use:** Both access-token signing sites (`signAuthResponse`, `/auth/refresh`).
```typescript
// Source: github.com/fastify/fastify-jwt (sign proxies to jsonwebtoken.sign)
//         + jsonwebtoken docs (jwtid option → jti claim)
import { randomUUID } from "node:crypto";

const token = await reply.jwtSign(
  { sub: user.id, username: user.username },
  { jwtid: randomUUID(), expiresIn: env.JWT_ACCESS_EXPIRES },
);
// Verified payload then exposes: request.user.jti AND request.user.exp (unix seconds)
```

### Pattern 2: Blocklist check inside `authenticate` (fail-open, jti-gated)
**What:** After `jwtVerify()`, if the payload has a `jti`, look it up in `RevokedToken`. Found → 401. DB error → warn + allow.
**When to use:** Once, in the `authenticate` decorator — covers every `preHandler:[app.authenticate]` route.
```typescript
// Source: derived from repo auth.ts + Prisma findUnique; honors D-04/D-05/D-06/L-02
app.decorate("authenticate", async function (request, reply) {
  try {
    await request.jwtVerify();
  } catch {
    return reply.status(401).send({ ok: false, error: "unauthorized" });
  }
  const { jti } = request.user as JwtPayload;
  if (!jti) return; // D-05/L-02: pre-deploy token, allow with no DB hit
  try {
    const revoked = await app.prisma.revokedToken.findUnique({ where: { jti } });
    if (revoked) {
      return reply.status(401).send({ ok: false, error: "unauthorized" }); // D-03 generic
    }
  } catch (err) {
    request.log.warn({ err }, "revocation lookup failed; allowing (fail-open)"); // D-06
  }
});
```

### Pattern 3: Best-effort logout revoke + lazy sweep
**What:** `/auth/logout` reads the bearer best-effort, sweeps expired rows, inserts the current `jti`, then does the existing refresh revoke. Always 200.
```typescript
// Source: derived from repo auth.ts + @fastify/jwt jwtVerify; honors D-01/D-02/D-08/D-09
app.post("/auth/logout", async (request, reply) => {
  const parsed = refreshBodySchema.safeParse(request.body);
  if (!parsed.success) {
    return reply.status(400).send(authError("invalid_body"));
  }
  // best-effort access-token revocation (D-01) — never throws to the client
  try {
    const decoded = await request.jwtVerify<JwtPayload>(); // reads Authorization header
    if (decoded.jti && decoded.exp) {
      const now = new Date();
      await app.prisma.revokedToken.deleteMany({ where: { expiresAt: { lt: now } } }); // D-09
      await app.prisma.revokedToken.create({
        data: {
          jti: decoded.jti,
          userId: decoded.sub,
          expiresAt: new Date(decoded.exp * 1000), // D-08
        },
      });
    }
  } catch {
    // missing/expired/invalid/no-jti bearer → do nothing, still 200 (D-01)
  }
  await revokeRefreshToken(app.prisma, parsed.data.refreshToken); // D-02 unchanged
  return { ok: true };
});
```
**Note on `request.jwtVerify()` in logout:** `jwtVerify` reads the `Authorization: Bearer` header itself; no `authenticate` preHandler is added (D-01). Use a duplicate-insert guard if a token is logged out twice — `create` on an existing PK throws P2002; either `upsert` or swallow it inside the try/catch (it's already wrapped). Prefer `upsert` (`where:{jti}, create:{...}, update:{}`) to be explicit.

### Pattern 4: `envToLogger` map with shared redaction
**What:** Replace `Fastify({ logger: false })` with a per-`NODE_ENV` map. `redact` must be attached to the object-valued branches; `production: true` becomes an object so redaction applies there too.
```typescript
// Source: fastify.dev/docs/latest/Reference/Logging/ (envToLogger + redact)
const redact = { paths: ["req.headers.authorization", "req.headers.cookie"], remove: true };
const envToLogger = {
  development: { redact, transport: { target: "pino-pretty", options: { translateTime: "HH:MM:ss Z", ignore: "pid,hostname" } } },
  production: { redact },   // object (not `true`) so redact applies; still JSON output
  test: false,
} as const;
const app = Fastify({ logger: envToLogger[env.NODE_ENV] });
```
**Why `production: {redact}` not `true`:** the bare `true` value enables logging with no redaction. To honor D-11 in production you must pass an object carrying `redact`. An object with only `{redact}` still produces standard pino JSON (no transport) — equivalent to `true` plus redaction.

### Anti-Patterns to Avoid
- **Adding `authenticate` preHandler to `/auth/logout`:** breaks D-01 (a missing/expired bearer would 401 instead of best-effort revoking). Logout must stay open.
- **Returning a `token_revoked` error code:** violates D-03; the C++ client only branches on the generic `unauthorized` 401.
- **Rejecting tokens that lack `jti`:** violates L-02/D-05. Always `return` (allow) before any DB hit when `jti` is absent.
- **Throwing on revocation DB error:** violates D-06; must fail open (warn + allow).
- **`prisma db push` for the new model:** this repo uses versioned migrations (`migrate deploy` in `deploy.sh`); `db push` would desync the migration history. See Pitfall 1.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Unique token ID | Custom counter / timestamp | `randomUUID()` (`node:crypto`) | Collision-free, no state, already in repo |
| `jti` claim injection | Manual payload mutation + manual `exp` math | `jwtid` + `expiresIn` sign options | jsonwebtoken sets `jti`/`exp` correctly incl. encoding |
| Reading `exp` for `expiresAt` | Decode/parse the token string | `request.user.exp` (verified payload) | `jwtVerify` already decoded it to unix seconds |
| Expired-row cleanup | Cron / scheduled job | Lazy `deleteMany` on logout (D-09) | No scheduler infra; amortized, locked decision |
| Header redaction | Custom serializer stripping headers | pino `redact: { paths, remove }` | Built into Fastify's logger; D-11 |
| Multipart test bodies | Hand-built boundary strings | `form-data` + `form.getHeaders()` | Already the test pattern (`api.test.ts`) |

**Key insight:** Every "new" capability in this phase already has a first-class mechanism in a library the repo already depends on. The work is wiring, not building.

## Runtime State Inventory

This phase is **additive** (new model, new claim, new logging) rather than a rename/migration, but the token-format change has real runtime-state implications, so the inventory is completed explicitly.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | **Live access tokens already issued to consoles have NO `jti`.** They are stored client-side (`platform/feed/auth_store`), 365-day lifetime. | None (code edit only) — D-05/L-02 guarantees they pass the blocklist unblocked. No data migration; they age out via relogin. |
| Stored data | New `RevokedToken` Postgres table — empty at deploy; populated lazily on logout. | Migration creates table; no backfill. |
| Live service config | API runs under **PM2 on Lightsail**, auto-deploys on push to `api/**` via `deploy.sh` which runs `npx prisma migrate deploy`. | New migration must be committed; `migrate deploy` applies it non-interactively in CI (no TTY). Verified in `api/scripts/deploy.sh`. |
| OS-registered state | None — PM2 process name unchanged (`thomaz-api`); no task scheduler/systemd unit names reference token internals. | None — verified by reading `deploy.sh` (`pm2 reload thomaz-api`). |
| Secrets/env vars | `JWT_SECRET` unchanged (signing key untouched — old tokens stay verifiable). New optional env? **No** — `JWT_ACCESS_EXPIRES` already exists; no new secret. | None. |
| Build artifacts | Prisma client must be regenerated after schema change (`prisma generate`, already in `deploy.sh`). | `npm run db:generate` locally; CI runs it. |

**Critical sequencing note:** Because old tokens lack `jti` and new code allows them (D-05), the deploy is backward-compatible — there is **no flag-day**. A user only becomes revocable after their next login/refresh issues a `jti`-bearing token. This is the intended, locked behavior (L-02), not a gap.

## Common Pitfalls

### Pitfall 1: Running the migration interactively (`migrate dev`) on the deploy path
**What goes wrong:** `prisma migrate dev` is interactive and may prompt; CI/PM2 deploy has no TTY.
**Why it happens:** `db:migrate` script is `prisma migrate dev` (for local authoring); deploy uses `prisma migrate deploy`.
**How to avoid:** Author the migration locally with `npx prisma migrate dev --name revoked_tokens` (or hand-write the SQL to match the repo's hand-numbered style — see Code Examples). The deploy script already runs `npx prisma migrate deploy` non-interactively. Mirror the existing `20250603150000_refresh_tokens` directory naming/format. **Warning sign:** a migration created by `migrate dev` gets a timestamp prefix automatically — that is fine and consistent with `20260604233032_remove_community_models`.

### Pitfall 2: `production: true` silently skips redaction
**What goes wrong:** Following the vanilla Fastify `envToLogger` example with `production: true` logs Authorization headers in production, violating DEBT-04.
**How to avoid:** Make the production branch an object `{ redact }` (Pattern 4). **Warning sign:** grepping production logs for `authorization` returns header values.

### Pitfall 3: Double-logout P2002 unique-constraint crash
**What goes wrong:** Logging out the same token twice calls `create` with a duplicate `jti` PK → Prisma P2002. If unguarded, logout 500s instead of returning 200 (violates D-02 "always 200").
**How to avoid:** Use `upsert` (`update: {}`) or keep the `create` inside the best-effort try/catch (Pattern 3 wraps it). Prefer `upsert` for clarity.

### Pitfall 4: Rate limit on `/auth/*` accidentally applied to logout changes
**What goes wrong:** `/auth/register` and `/auth/login` have an `authRateLimit` config object; `/auth/logout` and `/auth/refresh` do **not**. Adding the bearer-read logic must not introduce the rate-limit config to logout.
**How to avoid:** Edit only the handler body of the existing `/auth/logout` route (currently has no rate-limit config). Do not touch the `authRateLimit` object. **Warning sign:** the rate-limit tests (`rate-limits repeated /auth/login`, `/auth/register`) must stay green and unchanged.

### Pitfall 5: `JwtPayload` type not updated → `request.user.jti` is a type error
**What goes wrong:** The `JwtPayload` type in `auth.ts` is `{ sub, username }`. Accessing `.jti`/`.exp` fails `tsc --noEmit`.
**How to avoid:** Extend the type to `{ sub: string; username: string; jti?: string; exp?: number }`. `jti`/`exp` are optional because pre-deploy tokens lack `jti` and `exp` is added by `expiresIn`. The `@fastify/jwt` module augmentation (`FastifyJWT.user`) references this same type, so one edit covers both `request.user` and `jwtVerify` generics.

### Pitfall 6: `@fastify/static` still in `package.json` (SEC-01 residue)
**What goes wrong:** SEC-01's root fix (removing static serving) landed in Phase 1 by removing the `app.register(static)` call — but `@fastify/static` (`^8.1.1`) is **still listed in `api/package.json` dependencies** (verified). It is dead weight, not a live exposure (no code registers it). TEST-01 asserts behavior (404), which holds regardless.
**How to avoid:** TEST-01 is the binding guard — it passes because no route serves `/uploads/...`. Removing the unused dependency from `package.json` is optional cleanup the planner may include, but it is NOT required for SEC-01/TEST-01 and is not in the locked scope. Flag it; don't silently expand scope.

### Pitfall 7: `request.jwtVerify()` in logout fails the build if not given a return type
**What goes wrong:** `jwtVerify<JwtPayload>()` — the generic must match the augmented `FastifyJWT.user` or TS complains.
**How to avoid:** Since the module augmentation already sets `user: JwtPayload`, plain `await request.jwtVerify()` returns `JwtPayload`. The explicit generic in Pattern 3 is belt-and-suspenders; either compiles.

## Code Examples

### Prisma model (schema.prisma) — mirrors RefreshToken style, D-07
```prisma
// Source: derived from repo schema.prisma RefreshToken; honors D-07
model RevokedToken {
  jti       String   @id
  userId    String
  expiresAt DateTime
  createdAt DateTime @default(now())

  @@index([expiresAt])
}
```
Note: unlike `RefreshToken`, D-07 does **not** specify a `User` relation/FK (just a bare `userId` string for audit). Keeping it relationless avoids a cascade-delete dependency and matches the decision text exactly. (The planner may add `@@index([userId])` for the future "revoke all sessions" feature, but D-07 only mandates `@@index([expiresAt])`.)

### Hand-numbered migration SQL — mirrors `20250603150000_refresh_tokens/migration.sql`
```sql
-- Source: derived from repo migrations/20250603150000_refresh_tokens/migration.sql
-- CreateTable
CREATE TABLE "RevokedToken" (
    "jti" TEXT NOT NULL,
    "userId" TEXT NOT NULL,
    "expiresAt" TIMESTAMP(3) NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "RevokedToken_pkey" PRIMARY KEY ("jti")
);

-- CreateIndex
CREATE INDEX "RevokedToken_expiresAt_idx" ON "RevokedToken"("expiresAt");
```
Recommended: let `prisma migrate dev --name revoked_tokens` generate this (it will produce equivalent SQL with a timestamp-prefixed dir like the `20260604233032_remove_community_models` migration). Hand-writing is shown for parity reference.

### TEST-01: save-blob path is not publicly served (→ 404)
```typescript
// Source: derived from api/test/api.test.ts app.inject() pattern
it("TEST-01: direct save-blob path is not publicly accessible", async () => {
  const res = await app.inject({
    method: "GET",
    url: "/uploads/saves/some-user-id/01008BB901469000.bin",
  });
  expect(res.statusCode).toBe(404); // no static route serves uploads/
});
```

### TEST-02: the only missing branch — `revision_required` (400)
The existing `saves list, upload, download with revision` test already covers: new-slot create (200, revision 1), matching-revision update (200, revision 2), and `revision_conflict` (409). The **only uncovered branch** is updating an existing slot with **no** `revision` field → 400 `revision_required` (`saves.ts` line 146–148).
```typescript
// Source: derived from api/test/api.test.ts form-data pattern + saves.ts L146
it("TEST-02: PUT to existing slot without revision → 400 revision_required", async () => {
  // ... register user, PUT once to create the slot (revision 1) ...
  const form = new FormData();
  form.append("data", Buffer.from("v2"), { filename: "save.bin", contentType: "application/octet-stream" });
  // intentionally omit form.append("revision", ...)
  const res = await app.inject({
    method: "PUT", url: `/saves/${titleId}`,
    headers: { authorization: `Bearer ${token}`, ...form.getHeaders() },
    payload: form,
  });
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ ok: false, error: "revision_required" });
});
```
**TEST-02 completeness check for the planner:** ensure all four are asserted — (a) new slot 200, (b) matching revision 200, (c) `revision_conflict` 409, (d) `revision_required` 400. (a)–(c) exist; only (d) is new. The planner may consolidate into one dedicated test or extend the existing one.

### Revoked-token regression coverage (SEC-02)
```typescript
// Source: derived from api/test/api.test.ts auth + saves patterns
it("SEC-02: logged-out access token is rejected; other valid tokens still work", async () => {
  // login user A twice → tokenA1 (logged out) and tokenA2 (kept)
  // logout with tokenA1 in Authorization header + its refreshToken in body
  // GET /saves with tokenA1 → 401 { ok:false, error:"unauthorized" }
  // GET /saves with tokenA2 → 200
});
```
**Test-env caveat:** With `NODE_ENV=test` the logger is `false` (D-10) — the revoked-token `findUnique` and the fail-open warn path both run, but produce no log output, keeping the suite silent (Success Criterion 4). No test setup change needed for logging.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Fastify({ logger: false })` everywhere | `envToLogger[NODE_ENV]` map | This phase | Production gets pino JSON; test stays silent |
| Stateless 365-day access tokens (irrevocable) | `jti` + Postgres blocklist | This phase | Logout becomes effective for new tokens |

**Deprecated/outdated:** none relevant. `@fastify/jwt` 9.x sign API is current; jsonwebtoken `jwtid` option is long-standing and stable.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `request.jwtVerify()` inside `/auth/logout` reads the `Authorization: Bearer` header without a preHandler (so logout can be best-effort) | Pattern 3 | LOW — this is `@fastify/jwt`'s documented default token-source behavior; if a custom extractor were configured it would differ, but the repo uses defaults (verified in `auth.ts` register call, no `formatUser`/`getToken` override). Executor should smoke-test logout-with-bearer once. |
| A2 | `prisma migrate dev --name revoked_tokens` produces SQL equivalent to the hand-written CREATE TABLE | Code Examples | LOW — standard Prisma codegen; verified pattern against three existing migrations. |

All other claims are `[VERIFIED]` against repo code or `[CITED]` to official Fastify/@fastify/jwt docs. The two assumptions above are low-risk and self-verifying at implementation time.

## Open Questions

1. **Should the unused `@fastify/static` dependency be removed from `package.json`?**
   - What we know: It is no longer registered in `app.ts` (Phase 1); TEST-01 passes regardless.
   - What's unclear: Whether the planner wants to expand scope to dependency cleanup.
   - Recommendation: Leave out of Phase 2 scope (not in locked decisions); note as optional follow-up. TEST-01 is the binding guard.

2. **`upsert` vs guarded `create` for double-logout.**
   - What we know: Both prevent the P2002 crash; the logout try/catch already swallows it.
   - Recommendation: Use `upsert` for explicitness — but executor's call (commit/test organization is Claude's Discretion).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Node | runtime/tests | ✓ | engines `>=20` (repo); host has Node | — |
| Postgres (test) | Vitest suite (`app.inject` hits real DB) | assumed ✓ | `localhost:5433/thomaz` (test default in `api.test.ts`) | none — tests require a live test Postgres; this already gated Phase 1's suite |
| `prisma` CLI | migration authoring + `generate` | ✓ | ^6.8.2 (devDep) | — |
| `pino-pretty` | `development` log transport | ✗ (to be added) | ^13.1.3 | dev-only; absence affects only local log formatting, not prod/test |

**Missing dependencies with no fallback:** none blocking. The test Postgres on `localhost:5433` must be up to run Vitest — this is pre-existing infrastructure (the Phase 1 suite already depended on it), not new to this phase. The planner should include a "test DB reachable + migrations applied" precondition in the verification steps.

**Missing dependencies with fallback:** `pino-pretty` is added as a devDependency; if unavailable in CI, only `development` logging is affected (test=`false`, production=JSON need no transport).

## Validation Architecture

`.planning/config.json` was not present/readable in this session under the expected path; `nyquist_validation` not explicitly `false`, so this section is included.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Vitest ^3.1.4 |
| Config file | `api/vitest.config.ts` (`fileParallelism: false`, 30s timeouts, node env) |
| Quick run command | `cd api && npx vitest run test/api.test.ts` |
| Full suite command | `cd api && npm test` (`vitest run`) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| SEC-01 | `GET /uploads/saves/...` → 404 | integration (`app.inject`) | `npm test` | ✅ extend `api/test/api.test.ts` |
| SEC-02 | logged-out token → 401; other token → 200; no-jti token → allowed | integration | `npm test` | ✅ extend |
| DEBT-04 | test env stays silent; prod emits JSON; headers redacted | integration (silent-suite assertion) + manual prod check | `npm test` | ✅ silent suite is the automated guard |
| TEST-01 | save-blob path not served | integration | `npm test` | ✅ extend |
| TEST-02 | four `PUT` revision branches | integration | `npm test` | ✅ 3 exist; add `revision_required` (400) |

### Sampling Rate
- **Per task commit:** `cd api && npx vitest run test/api.test.ts`
- **Per wave merge:** `cd api && npm test`
- **Phase gate:** full suite green + `tsc --noEmit` clean (`npm run typecheck`) before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] No new test files strictly required — all new cases extend `api/test/api.test.ts`.
- [ ] No framework install needed (Vitest + form-data present).
- [ ] Precondition: test Postgres reachable at `DATABASE_URL` (default `localhost:5433`) with the **new** `RevokedToken` migration applied (`prisma migrate deploy` against the test DB) before the SEC-02 tests can pass.

*(The migration-applied precondition is the single most likely cause of a red suite if forgotten — surface it as an explicit verification step.)*

## Security Domain

`security_enforcement` not explicitly `false`; included.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Session Management (token revocation) | **yes** | `jti` claim + Postgres `RevokedToken` blocklist; logout invalidates (SEC-02) |
| V3 Authentication | partial (verify-only) | existing argon2 + JWT; no change this phase |
| V4 Access Control | yes | owner-scoped `userId_titleId` lookups in `saves.ts` (unchanged); SEC-01/TEST-01 guards cross-user blob access |
| V5 Input Validation | yes | zod schemas on bodies (`refreshBodySchema`, `savePutFieldsSchema`) — unchanged |
| V6 Cryptography | yes | `JWT_SECRET` signing + `randomUUID()` for `jti`; **never hand-roll** token IDs |
| V7 Error/Logging | **yes** | DEBT-04: enable logs in prod, redact Authorization/cookie headers; bodies not serialized |

### Known Threat Patterns for Fastify + JWT + Postgres
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Stolen long-lived (365d) token replay | Spoofing | `jti` blocklist on logout (SEC-02) — narrows the window for tokens issued post-deploy |
| Predictable save-blob URL enumeration | Information Disclosure | No static serving of `uploads/` (SEC-01); TEST-01 regression guard |
| Secret/PII in request logs | Information Disclosure | pino `redact` on `req.headers.authorization`/`cookie`; default serializer omits bodies (D-11) |
| Revocation DB outage blocks all auth | Denial of Service | Fail-open on lookup error (D-06) — availability over strict revocation |
| Revocation-table unbounded growth | Denial of Service (storage) | Lazy `deleteMany(expiresAt < now)` sweep on logout (D-09) |
| Double-logout crash (P2002) | DoS (request-level) | `upsert` / guarded `create` (Pitfall 3) |

## Sources

### Primary (HIGH confidence)
- Repo source (read this session): `api/src/plugins/auth.ts`, `api/src/routes/auth.ts`, `api/src/lib/auth-tokens.ts`, `api/src/lib/refresh-tokens.ts`, `api/src/routes/saves.ts`, `api/src/lib/save-storage.ts`, `api/src/app.ts`, `api/src/config.ts`, `api/prisma/schema.prisma`, `api/prisma/migrations/*`, `api/test/api.test.ts`, `api/vitest.config.ts`, `api/scripts/deploy.sh`, `api/package.json`, `api/src/plugins/db.ts`, `api/src/lib/errors.ts`
- `fastify.dev/docs/latest/Reference/Logging/` — `envToLogger` map, `redact` paths, "body is not serialized by default"
- `github.com/fastify/fastify-jwt` — sign proxies to `jsonwebtoken.sign`; `jti` via payload or `jwtid` option; verified payload exposes claims
- `npm view fastify@5.3.3 dependencies.pino → ^9.0.0`; `npm view pino-pretty version → 13.1.3` (repo pinojs/pino-pretty, created 2018)

### Secondary (MEDIUM confidence)
- WebSearch (cross-verified with GitHub/npm): `@fastify/jwt` `sign` is `jsonwebtoken.sign()`; `jwtid` is a supported sign option; `expiresIn` → `exp` seconds

### Tertiary (LOW confidence)
- none required — all load-bearing claims verified against repo or official docs

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — verified against `package.json` + registry; one additive devDep cited from official docs
- Architecture: HIGH — every pattern derived from existing repo code + official @fastify/jwt/Fastify docs
- Pitfalls: HIGH — each grounded in a concrete repo line (rate-limit config, `JwtPayload` type, `migrate deploy` in `deploy.sh`, stale `@fastify/static` dep)

**Research date:** 2026-06-04
**Valid until:** 2026-07-04 (stable stack; @fastify/jwt 9.x and Fastify 5.x sign/log APIs are not fast-moving)
