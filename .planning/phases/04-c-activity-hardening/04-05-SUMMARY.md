---
phase: 04-c-activity-hardening
plan: "05"
subsystem: testing
tags: [doctest, c++17, save-sync, cloud-saves, composition-testing]

# Dependency graph
requires:
  - phase: 04-c-activity-hardening
    provides: async_guard.hpp + run_if_alive (TEST-04b, Plan 01)
provides:
  - "TEST-04a: host doctest cases for classify->plan_push composition (doUpload decision path)"
affects: [04-c-activity-hardening, future-save-sync-changes]

# Tech tracking
tech-stack:
  added: []
  patterns: ["classify->plan_push composition test: drive (cloudExists, cloudRev, syncedRev) triple through both functions; assert composed (situation, revision, isConflict)"]

key-files:
  created: []
  modified:
    - tests/test_save_sync.cpp

key-decisions:
  - "No new production code — TEST-04a is a pure test addition; implementation already correct"
  - "Three composition triples: conflict (true,5,3), clean-push (true,3,3), new-slot (false,0,0)"
  - "Asserted values derived from save_sync.cpp implementation, not assumptions"

patterns-established:
  - "Composition test pattern: call classify() then plan_push() on its result; assert sit, plan.isConflict, plan.revision together"

requirements-completed: [TEST-04]

# Metrics
duration: 3min
completed: 2026-06-05
---

# Phase 04 Plan 05: classify->plan_push Composition Tests (TEST-04a) Summary

**Host doctests guard the cloud-save upload conflict-resolution composition (classify->plan_push) by asserting conflict, clean-push, and new-slot outcomes against the real save_sync.cpp implementation.**

## Performance

- **Duration:** 3 min
- **Started:** 2026-06-05T17:35:00Z
- **Completed:** 2026-06-05T17:38:18Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- Added three composition TEST_CASEs to `tests/test_save_sync.cpp` covering the `classify(...) -> plan_push(...)` pipeline that `doUpload` relies on
- Conflict triple `(true, 5, 3)`: asserts `SyncSituation::CloudAhead`, `isConflict==true`, `revision==5`
- Clean-push triple `(true, 3, 3)`: asserts `SyncSituation::InSync`, `isConflict==false`, `revision==3`
- New-slot triple `(false, 0, 0)`: asserts `SyncSituation::NoCloud`, `isConflict==false`, `revision==0`
- Host suite: 185 test cases, 552 assertions — all pass, zero new warnings under C++17 -Wall -Wextra

## Task Commits

1. **Task 1: Add classify->plan_push composition doctest cases (TEST-04a)** - `e2cfdda` (test)

**Plan metadata:** pending docs commit

## Files Created/Modified
- `tests/test_save_sync.cpp` — Extended with three composition TEST_CASEs (conflict, clean-push, new-slot); existing plan_push-per-situation cases untouched (Pitfall 3 avoided)

## Decisions Made
- No production code changes needed — `classify` and `plan_push` implementations in `save_sync.cpp` already produce the correct values; this plan is purely a regression guard
- Asserted revision and isConflict values read from `save_sync.cpp` implementation before writing tests (not assumed)
- Third case (NoCloud / new slot) added as it exercises the third enum branch at zero cost

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None — no external service configuration required.

## Next Phase Readiness
- TEST-04a (D-05a) complete: `classify->plan_push` composition is regression-guarded
- TEST-04b (D-05b, `run_if_alive` dropped-callback) was delivered in Plan 01 (`tests/test_async_guard.cpp`)
- Both TEST-04 sub-requirements satisfied; Phase 4 Plans 02-04 (activity migrations, cast fixes, curl cancellation) remain

---
*Phase: 04-c-activity-hardening*
*Completed: 2026-06-05*
