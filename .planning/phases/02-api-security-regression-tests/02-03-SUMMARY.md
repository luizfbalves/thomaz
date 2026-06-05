---
phase: 02-api-security-regression-tests
plan: "03"
subsystem: test
tags: [vitest, regression, sec-01, sec-02, saves, jwt, revision]

# Dependency graph
requires:
  - phase: 02-api-security-regression-tests
    plan: "01"
    provides: RevokedToken table in test DB
  - phase: 02-api-security-regression-tests
    plan: "02"
    provides: jti minting + blocklist enforcement + SEC-02 regression tests (6 tests)
provides:
  - TEST-01 regression guard asserting GET /uploads/saves/<userId>/<titleId>.bin → 404
  - TEST-02 fourth PUT revision branch asserting revision_required (400) with no revision field
  - Full four-branch revision matrix asserted (new-slot 200, matching 200, conflict 409, required 400)
  - SEC-02 logged-out/sibling-token behavior locked (pre-existing from Wave 2)
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "TEST-01: plain app.inject GET no-auth → 404 for static-less route guard"
    - "TEST-02: FormData multipart PUT to existing slot omitting revision field → revision_required 400"
    - "titleId fixture: 010000 prefix + Date.now() hex-padded to 16 uppercase hex digits (valid BigInt titleId)"

key-files:
  created: []
  modified:
    - api/test/api.test.ts

key-decisions:
  - "TEST-01 uses no auth header: the assertion is that no static route serves /uploads/ at all, so auth is irrelevant"
  - "TEST-02 titleId uses 010000 prefix + Date.now().toString(16).padStart(10): produces a valid 16-hex-digit titleId unlike the original 0100TEST... approach which used invalid hex chars"
  - "SEC-02 (logged-out token rejected, sibling token 200) was already implemented in Wave 2 (commit 5ce7491); Task 2 requires no new code"

patterns-established:
  - "Pattern: titleId fixture for tests — 16 uppercase hex digits; Date.now() as suffix for uniqueness"

requirements-completed: [SEC-01, TEST-01, TEST-02]

# Metrics
duration: ~5min
completed: 2026-06-05
---

# Phase 02 Plan 03: Regression Tests — TEST-01, TEST-02, SEC-02 Summary

**TEST-01 (save-blob 404 guard), TEST-02 (revision_required branch), and SEC-02 revoked-token behavior locked behind regression tests; all 14 Vitest tests pass silently**

## Performance

- **Duration:** ~5 min
- **Started:** 2026-06-05T00:51:00Z
- **Completed:** 2026-06-05T00:56:21Z
- **Tasks:** 2 (Task 1 committed; Task 2 pre-existing from Wave 2)
- **Files modified:** 1 (api/test/api.test.ts)

## Accomplishments

- Added `it("TEST-01: direct save-blob path is not publicly accessible", ...)` — plain `app.inject({ method: "GET", url: "/uploads/saves/some-user-id/01008BB901469000.bin" })` asserts `statusCode === 404`; no auth header needed; no static route serves `/uploads/`
- Added `it("TEST-02: PUT to existing slot without revision → 400 revision_required", ...)` — registers a fresh user, PUTs once to create a slot (revision 1), then PUTs again to the same titleId with no `revision` field; asserts `statusCode === 400` and `json()` equals `{ ok: false, error: "revision_required" }`
- Four-branch revision matrix now fully asserted: new-slot 200 (existing), matching-revision 200 (existing), revision_conflict 409 (existing), revision_required 400 (NEW in this plan)
- SEC-02 revoked-token test (`it("SEC-02 T2: logged-out access token is rejected; sibling token still works", ...)`) was already present from Plan 02 — Task 2 required no new code
- 14/14 tests pass with no spurious log output (NODE_ENV=test → logger false)

## Task Commits

1. **Task 1: Add TEST-01 save-blob 404 guard and TEST-02 revision_required branch** - `4449d1b` (test)
2. **Task 2: SEC-02 logged-out/sibling-token test** — pre-existing from Wave 2 (`5ce7491`), no new commit

## Files Created/Modified

- `api/test/api.test.ts` — Added 56 lines: TEST-01 (plain inject, 6 lines) + TEST-02 (FormData multipart with first PUT + second PUT without revision, 50 lines)

## Test Coverage Summary

| Test | Status | Commit |
|------|--------|--------|
| TEST-01: `GET /uploads/saves/.../...bin` → 404 | NEW ✓ | 4449d1b |
| TEST-02: PUT existing slot, no revision → 400 `revision_required` | NEW ✓ | 4449d1b |
| SEC-02: logged-out token → 401, sibling → 200 | Pre-existing ✓ | 5ce7491 |
| SEC-02: no-bearer logout → 200 | Pre-existing ✓ | 5ce7491 |
| SEC-02: double logout → 200 | Pre-existing ✓ | 5ce7491 |
| SEC-02 T1: jti minting (3 tests) | Pre-existing ✓ | 850f365/422bd8d |

**Final test count:** 14 tests, all passing.

## Decisions Made

- **TEST-01 needs no auth:** The 404 must come from the absence of any static route — not from an auth guard. An unauthenticated request to `/uploads/saves/...` must 404, not 401. No `Authorization` header is included.
- **Valid titleId for TEST-02:** The original titleId scheme (`0100TEST...`) used the literal string "TEST" which contains non-hex characters, causing `parseTitleIdParam` to return null and the PUT to fail with 400 `invalid_title_id`. Fixed to `010000` + `Date.now().toString(16).toUpperCase().padStart(10, "0")` which produces a valid 16-char uppercase hex string.
- **No new Task 2 code:** Plan 02 (Wave 2) already added the SEC-02 revoked-token/sibling-token test in commit `5ce7491`. The test asserts exactly what Task 2 requires: logout with bearer→200, revoked token→401 `{ok:false,error:"unauthorized"}`, sibling token→200.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Invalid titleId fixture used non-hex chars**
- **Found during:** Task 1 (first test run)
- **Issue:** Generated titleId `0100TEST${Date.now().toString(16).toUpperCase().padStart(8, "0")}` contained the literal text "TEST" — not valid hex. `parseTitleIdParam` returned null → saves route returned 400 `invalid_title_id` on the first PUT instead of 200.
- **Fix:** Changed to `010000${Date.now().toString(16).toUpperCase().padStart(10, "0")}` — produces a valid 16-digit uppercase hex string (e.g. `01000019600C3E5E`).
- **Files modified:** api/test/api.test.ts
- **Commit:** 4449d1b (same commit, fixed before final commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 bug — invalid hex chars in titleId fixture)
**Impact on plan:** Required fix for TEST-02 first PUT to succeed. Semantically identical to plan intent; only the fixture string format changed.

## Known Stubs

None — all tests exercise real behavior against the live test Postgres and the in-process Fastify app.

## Threat Flags

No new threat surface introduced. This plan adds tests only.

## Self-Check: PASSED

- FOUND: api/test/api.test.ts (contains `uploads/saves`, `revision_required`, `SEC-02`)
- `grep -c "uploads/saves" api/test/api.test.ts` → 1 ✓
- `grep -c "revision_required" api/test/api.test.ts` → 5 ✓
- `grep -c "SEC-02" api/test/api.test.ts` → 8 ✓
- FOUND: commit 4449d1b (test Task 1)
- `npm test` → 14/14 tests passed ✓
- No spurious log output (NODE_ENV=test) ✓

---
*Phase: 02-api-security-regression-tests*
*Completed: 2026-06-05*
