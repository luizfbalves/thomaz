---
status: passed
phase: 02-api-security-regression-tests
verified: 2026-06-04
requirements: [SEC-01, SEC-02, DEBT-04, TEST-01, TEST-02]
method: inline (source inspection + full test suite + migration status)
---

# Phase 2: API Security + Regression Tests — Verification

**Goal:** The live API has no remaining HIGH-severity security issues: save blobs require authentication, tokens can be revoked on logout, production logging is enabled, and regression tests guard these fixes.

**Verdict:** ✅ PASSED — all 5 requirements and all 4 success criteria satisfied in code and guarded by tests.

## Evidence

- **Test suite:** `cd api && npx vitest run` → **14/14 passing**, no spurious log output (Success Criterion 4 confirmed).
- **Migration:** `prisma migrate status` against the test DB (`localhost:5433`) → "Database schema is up to date" (`20260605004126_revoked_tokens` applied).

## Success Criteria

| # | Criterion | Result | Evidence |
|---|-----------|--------|----------|
| 1 | Direct save-blob path returns 404; no static serving | ✅ | No `@fastify/static`/`sendFile`/uploads root in `src/`; TEST-01 asserts `GET /uploads/saves/...bin` → 404 |
| 2 | Logged-out token → 401; sibling token → 200; pre-deploy (no jti) passes unblocked | ✅ | `plugins/auth.ts:48` `if (!jti) return;` (no DB hit); revoked → generic 401 (`:56`); SEC-02 test asserts both |
| 3 | Saves PUT revision branches: 400/409/200/200, all four covered | ✅ | TEST-02 added `revision_required` 400; other three pre-existing — full matrix asserted |
| 4 | production emits pino JSON logs; test silent | ✅ | `app.ts:36` `production: { redact }`, `:37` `test: false`; suite runs silent |

## Requirement Traceability

| REQ-ID | Plan(s) | Status | Notes |
|--------|---------|--------|-------|
| SEC-01 | 02-03 | ✅ | Verify-only; regression guard TEST-01 asserts no static blob route |
| SEC-02 | 02-02 | ✅ | jti minted at both signers (`auth-tokens.ts:15`, `auth.ts:99`); jti-gated fail-open blocklist in `authenticate`; best-effort logout revoke + lazy sweep |
| DEBT-04 | 02-01 | ✅ | `envToLogger` map; `authorization`/`cookie` headers redacted; `pino-pretty` devDep |
| TEST-01 | 02-03 | ✅ | Save-blob 404 guard |
| TEST-02 | 02-03 | ✅ | `revision_required` 400 branch completes the PUT matrix |

## Locked-Decision Compliance (spot-checked in source)

- **D-01/D-02:** `/auth/logout` has no `authenticate` preHandler; best-effort `jwtVerify` wrapped; always returns `{ ok: true }` (`routes/auth.ts:105–134`).
- **D-03:** Revoked token → generic `401 {ok:false,error:"unauthorized"}` — no distinct code.
- **D-05/L-02:** Pre-deploy tokens without `jti` allowed with no DB hit.
- **D-06:** Fail-open on DB error — warns and allows (`plugins/auth.ts:58–59`).
- **D-09/Pitfall-3:** Lazy `deleteMany` expired sweep + `upsert` (double-logout P2002 safe).
- **Pitfall 2:** production logger is an object carrying `redact`, never bare `true`.

## Human Verification

None required — all criteria are automatically verifiable and covered by the test suite. Optional production smoke (observe redacted pino JSON logs in the live `production` environment) can be confirmed on next deploy but is not blocking.
