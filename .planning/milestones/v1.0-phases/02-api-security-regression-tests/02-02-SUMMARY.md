---
phase: 02-api-security-regression-tests
plan: "02"
subsystem: auth, api
tags: [jwt, jti, fastify, prisma, postgres, revocation, blocklist, fast-jwt]

# Dependency graph
requires:
  - phase: 02-api-security-regression-tests
    plan: "01"
    provides: RevokedToken Prisma model + migration applied to test DB; prisma.revokedToken delegate typed and ready
provides:
  - Both access-token signing sites (signAuthResponse + /auth/refresh) mint a unique jti via fast-jwt jti sign option
  - JwtPayload type extended with optional jti?: string and exp?: number (pre-deploy tokens remain compatible)
  - authenticate decorator enforces jti-gated fail-open RevokedToken blocklist (generic 401 on revocation, D-03/D-05/D-06)
  - /auth/logout performs best-effort access-token revocation (deleteMany sweep + upsert) with no preHandler and no rate-limit (D-01/D-09)
  - 6 new regression tests covering jti minting, revocation enforcement, double-logout safety, and no-bearer logout
affects:
  - 02-03 (SEC-02 regression tests — implementation is now in place; further tests target the blocklist behavior)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "jti sign option (fast-jwt API): { jti: randomUUID(), expiresIn } — NOT jwtid (jsonwebtoken legacy API)"
    - "Blocklist check pattern: jti-gated findUnique after jwtVerify, fail-open on DB error, generic 401 shape"
    - "Logout revocation pattern: best-effort jwtVerify in catch, deleteMany sweep then upsert, always-200"

key-files:
  created: []
  modified:
    - api/src/lib/auth-tokens.ts
    - api/src/routes/auth.ts
    - api/src/plugins/auth.ts
    - api/test/api.test.ts

key-decisions:
  - "Use jti sign option (not jwtid): @fastify/jwt 9.x uses fast-jwt, which exposes jti in SignerOptions — not jsonwebtoken's jwtid"
  - "Both signing sites use randomUUID() from node:crypto for collision-free stateless jti generation"
  - "JwtPayload jti/exp are optional (Pitfall 5/L-02) — pre-deploy tokens without jti pass blocklist unblocked"
  - "Blocklist check is jti-gated: if no jti, return immediately with no DB hit (D-05/L-02)"
  - "Fail-open on revocation DB error: log.warn + allow (D-06) — availability over strict revocation"
  - "Logout uses upsert (not create) to survive double-logout P2002 (Pitfall 3)"
  - "Logout has no authenticate preHandler and no authRateLimit config (D-01/Pitfall 4)"

patterns-established:
  - "Pattern: fast-jwt jti sign option — use { jti: randomUUID() } not { jwtid: randomUUID() } in all future jwtSign calls"
  - "Pattern: blocklist enforcement in authenticate decorator only (D-04) — one edit covers every preHandler:[app.authenticate] route"

requirements-completed: [SEC-02]

# Metrics
duration: 4min
completed: 2026-06-05
---

# Phase 02 Plan 02: SEC-02 Token Revocation Summary

**jti-bearing access tokens with Postgres-backed revocation blocklist: logout now invalidates tokens, pre-deploy tokens pass unblocked, and DB outages fail open**

## Performance

- **Duration:** ~4 min
- **Started:** 2026-06-05T00:46:20Z
- **Completed:** 2026-06-05T00:50:46Z
- **Tasks:** 2 (TDD: RED + GREEN for each)
- **Files modified:** 4 (auth-tokens.ts, routes/auth.ts, plugins/auth.ts, api.test.ts)

## Accomplishments

- Added `jti: randomUUID()` to both access-token signing sites (signAuthResponse + /auth/refresh), using fast-jwt's `jti` sign option
- Extended `JwtPayload` with `jti?: string` and `exp?: number` (both optional, covering pre-deploy tokens without jti)
- Wired jti-gated, fail-open `revokedToken.findUnique` blocklist inside the `authenticate` decorator — covers all protected routes via the single `preHandler: [app.authenticate]` wiring
- Added best-effort access-token revocation to `/auth/logout`: lazy `deleteMany` sweep + `upsert` (double-logout safe), with no preHandler and no rate-limit config added
- 6 regression tests: 3 for jti minting (register, refresh, uniqueness) + 3 for blocklist/logout behavior (revoked→401, no-bearer→200, double-logout→200)
- All 12 tests pass; typecheck clean

## Task Commits

1. **RED: failing jti minting + revocation tests** - `850f365` (test)
2. **Task 1: Mint jti on both access-token signing sites; extend JwtPayload** - `422bd8d` (feat)
3. **Task 2: Enforce blocklist in authenticate; best-effort revoke + lazy sweep in logout** - `5ce7491` (feat)

**Plan metadata:** (docs commit below)

_Note: TDD tasks have RED commit (test) + GREEN commit (feat) per task. Both tasks share a single RED commit since their test behaviors are interrelated._

## Files Created/Modified

- `api/src/lib/auth-tokens.ts` — Added `import { randomUUID } from "node:crypto"` and `jti: randomUUID()` to signAuthResponse's jwtSign options
- `api/src/routes/auth.ts` — Added randomUUID import + JwtPayload import; `jti: randomUUID()` in /auth/refresh jwtSign; best-effort revocation block in /auth/logout (deleteMany + upsert)
- `api/src/plugins/auth.ts` — Extended JwtPayload with `jti?: string; exp?: number`; added blocklist check in authenticate after jwtVerify
- `api/test/api.test.ts` — Added 6 regression tests (3 jti-minting T1, 3 blocklist/logout T2)

## Decisions Made

- **fast-jwt jti option, not jsonwebtoken jwtid:** @fastify/jwt 9.x uses fast-jwt under the hood (`fast-jwt: ^5.0.0` in package.json). The `SignerOptions.jti` field is the correct API. The `jwtid` documented in RESEARCH.md is the jsonwebtoken API (older @fastify/jwt). Using `jti` in options passes both typecheck and runtime correctly.
- **JwtPayload fields optional:** Pre-deploy tokens lack jti/exp. Making them optional (Pitfall 5/L-02) avoids tsc errors and lets the blocklist skip cleanly when jti is absent.
- **Upsert over create in logout:** Explicit `upsert({ update: {} })` makes double-logout idempotent without relying on the catch-all try/catch (Pitfall 3).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Used jti sign option instead of jwtid**
- **Found during:** Task 1 (typecheck after initial implementation)
- **Issue:** Plan specified `jwtid: randomUUID()` based on jsonwebtoken API, but @fastify/jwt 9.x uses fast-jwt which exposes `jti` in `SignerOptions`. TypeScript error: "jwtid does not exist in type Partial<SignOptions>, did you mean jti?"
- **Fix:** Changed `{ jwtid: randomUUID() }` to `{ jti: randomUUID() }` in both signing sites. fast-jwt's signer.js reads the `jti` field from options and injects it into the payload — verified in signer.js source.
- **Files modified:** api/src/lib/auth-tokens.ts, api/src/routes/auth.ts
- **Verification:** tsc --noEmit exits 0; runtime tests confirm jti claim is present in decoded payload
- **Committed in:** 422bd8d (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (1 Rule 1 bug — wrong sign option name for @fastify/jwt 9.x)
**Impact on plan:** Required fix for typecheck compliance and correct jti injection. Semantically identical to plan intent; only the option key name changed.

## Issues Encountered

None beyond the jti/jwtid API mismatch documented above.

## Known Stubs

None — all functionality is fully wired. The blocklist lookup hits the real `RevokedToken` table. The lazy sweep and upsert are real Prisma calls.

## Threat Flags

No new threat surface beyond what was planned in the PLAN.md threat model. The blocklist check and logout revocation both conform to the disposition table (T-02-04 through T-02-08).

## Next Phase Readiness

- **Plan 03 (SEC-02 regression tests):** Implementation is complete. The SEC-02 behavioral tests added in this plan already cover the core revocation scenarios. Plan 03 can extend with additional edge cases (fail-open simulation, no-jti pre-deploy token, etc.)
- **optionalAuth unchanged:** The `optionalAuth` decorator has no blocklist check — it remains a public-feed read-only path that continues to function for unauthenticated access
- **authRateLimit unchanged:** Only `/auth/register` and `/auth/login` carry the rate-limit config; `/auth/logout` and `/auth/refresh` do not
- **Typecheck clean:** All three source files compile with no errors

## Self-Check: PASSED

- FOUND: api/src/lib/auth-tokens.ts (contains `jti: randomUUID()`)
- FOUND: api/src/routes/auth.ts (contains `jti: randomUUID()`, `revokedToken.upsert`, `revokedToken.deleteMany`)
- FOUND: api/src/plugins/auth.ts (contains `revokedToken.findUnique`, `jti?: string`, `exp?: number`)
- FOUND: api/test/api.test.ts (contains 6 new SEC-02 tests)
- FOUND: commit 850f365 (test RED)
- FOUND: commit 422bd8d (feat Task 1 GREEN)
- FOUND: commit 5ce7491 (feat Task 2 GREEN)
- typecheck: PASSED (tsc --noEmit exits 0)
- test suite: PASSED (12/12 tests green)

---
*Phase: 02-api-security-regression-tests*
*Completed: 2026-06-05*
