# Phase 2: API Security + Regression Tests - Context

**Gathered:** 2026-06-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Close the remaining HIGH-severity API security holes and lock every fix behind regression tests. Concretely:
- **SEC-02** — access tokens become revocable on logout via a `jti` claim + a Postgres `RevokedToken` blocklist.
- **DEBT-04** — production request logging is enabled via an `envToLogger` map, with secrets/PII redacted.
- **SEC-01** — verify-only (the `@fastify/static` removal already landed in Phase 1); TEST-01 is the regression guard.
- **TEST-01 / TEST-02** — regression tests for the static-blob exposure guard and the saves `PUT` revision branches.

**Not in scope:** Redis or any new infra; redesigning auth; refresh-token changes (already DB-backed); the C++ platform/activity fixes (Phases 3–4); relocating preserved auth files.

</domain>

<decisions>
## Implementation Decisions

### Locked upstream (from ROADMAP.md planning flags — do not re-litigate)
- **L-01:** SEC-02 uses a `jti` claim on access tokens + a Postgres `RevokedToken` table. **No Redis** — `findUnique` on the `jti` primary key is sub-millisecond.
- **L-02:** Tokens minted before deploy (without a `jti`) MUST pass the blocklist check unblocked — never rejected for lacking a `jti`.
- **L-03:** Refresh tokens are already DB-backed via `revokeRefreshToken`. **No change** to refresh-token handling.
- **L-04:** SEC-01 is verify-only. The fix (removing `@fastify/static`) landed in Phase 1; blobs stay in `uploads/saves/` (Phase 1 D-10). Phase 2 delivers TEST-01 as the regression guard.

### Logout contract (SEC-02)
- **D-01:** `/auth/logout` reads the access token from the `Authorization: Bearer` header **best-effort** — if present and valid with a `jti`, insert that `jti` into `RevokedToken`. The endpoint stays effectively unauthenticated (no `authenticate` preHandler): a missing/expired/invalid bearer does **not** 401.
- **D-02:** Logout still revokes the `refreshToken` from the request body (unchanged behavior), and **always** returns `200 { ok: true }`.
- **D-03:** A blocklisted (logged-out) access token, when reused, returns the **generic** `401 { ok: false, error: "unauthorized" }` — the same shape as any invalid/expired token, so the existing C++ client re-login path handles it. No distinct `token_revoked` code.

### Blocklist enforcement
- **D-04:** The revocation check lives in the `authenticate` decorator (`api/src/plugins/auth.ts`), after `jwtVerify()` succeeds, so it covers every authenticated route via the existing `preHandler: [app.authenticate]` wiring.
- **D-05:** **Skip the lookup when the verified token has no `jti`** (pre-deploy tokens) — allow with no DB hit. Only query `RevokedToken` when a `jti` is present. (Satisfies L-02 and avoids a wasted round-trip per legacy-token request.)
- **D-06:** **Fail-open on DB error** — if the `RevokedToken` lookup throws (transient Postgres error), log a warning and treat the token as not-revoked (allow the request). Prioritizes availability; the revocation gap is acceptable for a save-sync hobby API.

### RevokedToken lifecycle
- **D-07:** New Prisma model — `jti` (String `@id`), `userId` (String), `expiresAt` (DateTime), `createdAt` (DateTime `@default(now())`), with `@@index([expiresAt])`. `userId` is kept for audit / a future "revoke all my sessions".
- **D-08:** `expiresAt` is derived from the access token's `exp` claim (`new Date(payload.exp * 1000)`) — the row is only useful until the token would expire anyway (access tokens live 365d per `JWT_ACCESS_EXPIRES`).
- **D-09:** **Lazy sweep on logout** — before inserting the new revocation row, `deleteMany({ where: { expiresAt: { lt: now } } })`. No scheduler/cron; cleanup cost is amortized onto the logout path.

### Logging policy (DEBT-04)
- **D-10:** `envToLogger` map keyed by `NODE_ENV`: `production` → pino JSON (`true`); `development` → pino-pretty (`{ transport: { target: "pino-pretty" } }`); `test` → `false` (keeps the Vitest suite silent — required by Success Criterion 4). Replaces the current `Fastify({ logger: false })`. Adds `pino-pretty` as a **dev dependency**.
- **D-11:** Redact `req.headers.authorization` and `req.headers.cookie` via pino `redact`. Fastify's default request serializer logs method/url/statusCode but **not** request bodies, so passwords/usernames in POST bodies never reach the logs — header redaction covers the stated secret/PII surface.

### Claude's Discretion
- **`jti` generation:** mint a unique `jti` (e.g. `randomUUID()`) on every access token. This must be added in **both** places that sign access tokens: `signAuthResponse` (`api/src/lib/auth-tokens.ts`) and the inline `reply.jwtSign` in `/auth/refresh` (`api/src/routes/auth.ts`). Use `@fastify/jwt`'s `jti`/`jwtid` mechanism or include `jti` in the payload — planner/executor decide.
- **TEST-01 approach:** assert a direct path to a save blob (e.g. `GET /uploads/saves/<userId>/<titleId>.bin`) returns 404 — no route serves it. Exact fixture shape is the executor's call.
- **TEST-02:** the four revision branches already exist in `saves.ts` PUT; tests only. Use the existing `app.inject()` + multipart pattern.
- Commit granularity, migration naming, and test-file organization — planner/executor decide.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scope & requirements
- `.planning/ROADMAP.md` §"Phase 2: API Security + Regression Tests" — goal, success criteria, and the locked **Planning flags** (jti scope, SEC-01 verify-only, no Redis).
- `.planning/REQUIREMENTS.md` — SEC-01, SEC-02, DEBT-04, TEST-01, TEST-02 acceptance text.
- `.planning/codebase/CONCERNS.md` — original audit findings behind these requirements.
- `.planning/phases/01-remove-community-feature/01-CONTEXT.md` — D-10 (blobs stay in `uploads/saves/`, no SAVES_DIR migration), preserved auth/session files.

### API code touchpoints
- `api/src/plugins/auth.ts` — `authenticate` / `optionalAuth` decorators + `JwtPayload`; where the blocklist check lands and where `jti` must surface in the payload type.
- `api/src/routes/auth.ts` — `/auth/logout` (current refresh-only behavior) and the `/auth/refresh` inline `jwtSign`.
- `api/src/lib/auth-tokens.ts` — `signAuthResponse`, the primary access-token signer.
- `api/src/lib/refresh-tokens.ts` — `revokeRefreshToken` (reused by logout, unchanged).
- `api/src/routes/saves.ts` — PUT revision branches (TEST-02 target, already implemented).
- `api/src/app.ts` — `Fastify({ logger: false })` to be replaced by the `envToLogger` map; `multipart` retained for save uploads.
- `api/src/config.ts` — `Env` schema + `NODE_ENV` enum (drives `envToLogger`); `JWT_ACCESS_EXPIRES` default `365d`.
- `api/prisma/schema.prisma` — add the `RevokedToken` model (migration required); `User`/`RefreshToken`/`SaveSlot` intact.
- `api/test/api.test.ts` — existing `app.inject()` Vitest scaffold; extend for TEST-01, TEST-02, and revoked-token coverage.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app.authenticate` decorator: single enforcement point already applied as `preHandler` on every `/saves` route — add the blocklist check here once, covers all.
- `revokeRefreshToken(prisma, rawToken)`: logout already calls it; keep as-is.
- `api/test/api.test.ts` `app.inject()` pattern (11 tests incl. "auth refresh and logout", "saves list, upload, download with revision", rate-limit tests): the template for all new regression tests; multipart upload pattern already demonstrated.
- Rate limiting on `/auth/*` already configured — logout changes must not disturb it.

### Established Patterns
- Error envelopes: `authError(...)` / `actionError(...)` return `{ ok: false, error: <code> }`; the generic `unauthorized` 401 (D-03) reuses this.
- Access tokens are signed in exactly **two** spots (`signAuthResponse` and `/auth/refresh`) — both must add `jti` (Claude's Discretion above).
- `loadConfig()` validates `process.env` via zod; `NODE_ENV` is an enum (`development`/`production`/`test`) ready to key the logger map.

### Integration Points
- New `RevokedToken` Prisma model + migration (mirror the existing `RefreshToken` migration style).
- Logout endpoint gains an optional bearer read; enforcement gains a conditional DB lookup — both within existing files, no new routes.

</code_context>

<specifics>
## Specific Ideas

- Revoked-token UX must funnel into the **existing** session-expired/re-login flow in the C++ client (which already handles a plain `401 { ok:false, error:"unauthorized" }`) — do not introduce a new error code the client wouldn't branch on.
- Logging redaction targets are explicit: `req.headers.authorization`, `req.headers.cookie`. Bodies are not serialized by default — keep it that way.

</specifics>

<deferred>
## Deferred Ideas

- **"Revoke all my sessions"** (bulk access-token revocation by `userId`) — the `userId` column on `RevokedToken` is added now to enable it, but the feature itself is out of scope for this phase.
- **Scheduled/cron purge of `RevokedToken`** — rejected in favor of lazy sweep on logout; revisit only if logout volume can't keep the table tight.
- **Distinct `token_revoked` error code** — considered and deferred; would need C++ client handling first.

</deferred>

---

*Phase: 2-api-security-regression-tests*
*Context gathered: 2026-06-04*
