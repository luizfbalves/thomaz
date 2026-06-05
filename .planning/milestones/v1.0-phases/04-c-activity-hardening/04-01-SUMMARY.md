---
phase: 04-c-activity-hardening
plan: 01
subsystem: concurrency
tags: [cpp, atomic, shared_ptr, doctest, async_guard, ThomazActivity, CONC-02, TEST-04b]

# Dependency graph
requires:
  - phase: 03-c-platform-hardening
    provides: "cloudBusy atomicized (CONC-01) — safe to touch save_detail_activity.hpp in Phase 4"
provides:
  - "ThomazActivity base class (source/app/thomaz_activity.hpp) with alive + cancelled guards and runAsync wrapper"
  - "thomaz::core::run_if_alive pure guard helper (source/core/async_guard.hpp) — Borealis-free, C++17-clean"
  - "TEST-04b host doctest suite (tests/test_async_guard.cpp) — dropped-callback semantics verified"
  - "cancelledFlag() accessor on ThomazActivity — Plan 04 CONC-03 wiring point"
affects:
  - 04-02-PLAN (base-class migration for 13 activities)
  - 04-03-PLAN (CONC-03: curl cancellation using cancelled flag)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ThomazActivity: single brls::Activity subclass base owns both alive and cancelled shared_ptr<atomic<bool>> guards"
    - "runAsync template: captures alive by value, dispatches worker on brls::async pool, guards onSync via run_if_alive"
    - "run_if_alive: pure inline bool function in thomaz::core — Borealis-free, host-testable, C++17-clean"
    - "cancelledFlag() protected accessor: exposes cancelled shared_ptr to derived classes and platform layer"

key-files:
  created:
    - source/core/async_guard.hpp
    - source/app/thomaz_activity.hpp
    - tests/test_async_guard.cpp
  modified: []

key-decisions:
  - "runAsync uses template<typename Worker, typename OnSync> form (D-01a discretion) — moves worker/onSync into lambda to avoid std::function overhead; both are mutable to allow move-only callables"
  - "cancelledFlag() returns the cancelled shared_ptr (not raw bool) — Plan 04 hands this directly to platform curl transfer calls"
  - "run_if_alive null-guard branch implemented — returns false without calling onSync when alive is nullptr (robustness for TEST-04b Test 3)"
  - "Comment text in async_guard.hpp avoids the word 'borealis' to preserve the automated grep check in acceptance criteria"

patterns-established:
  - "Pure guard helper in source/core/: extracted run_if_alive so test suite (C++17, no Borealis) can verify the dropped-callback decision directly"
  - "shared_ptr<atomic<bool>> capture-by-value: both guards captured by value so flag objects outlive the activity via the lambda's copy"

requirements-completed: [CONC-02, TEST-04b]

# Metrics
duration: 3min
completed: 2026-06-05
---

# Phase 04 Plan 01: Async Guard and ThomazActivity Base Summary

**ThomazActivity base class with template runAsync and thomaz::core::run_if_alive; alive + cancelled shared_ptr guards; TEST-04b dropped-callback doctest passing under C++17**

## Performance

- **Duration:** 3 min
- **Started:** 2026-06-05T17:29:43Z
- **Completed:** 2026-06-05T17:32:54Z
- **Tasks:** 3
- **Files modified:** 3 (all created)

## Accomplishments

- Created `source/core/async_guard.hpp` — pure, Borealis-free `thomaz::core::run_if_alive` inline function; C++17-clean; returns false and drops onSync when guard is null or cleared, otherwise calls onSync and returns true
- Created `source/app/thomaz_activity.hpp` — `ThomazActivity : public brls::Activity` base class owning `alive` (init true) and `cancelled` (init false) as `shared_ptr<atomic<bool>>`; destructor sets `*alive=false; *cancelled=true`; protected template `runAsync(worker, onSync)` that captures alive by value and delegates the guard decision to `thomaz::core::run_if_alive`; protected `cancelledFlag()` accessor for Plan 04 CONC-03 wiring
- Created `tests/test_async_guard.cpp` — three TEST-04b doctest cases (alive runs, not-alive drops, null-guard drops); C++17-clean, no Borealis; 182 total suite tests pass, 0 failures

## Task Commits

1. **Task 1: Create run_if_alive pure guard helper** - `fef2542` (feat)
2. **Task 2: Create ThomazActivity base class** - `9ca9873` (feat)
3. **Task 3: Add TEST-04b dropped-callback doctest** - `135e3ed` (test)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `source/core/async_guard.hpp` — `thomaz::core::run_if_alive` inline bool; `#pragma once`; no Borealis; C++17
- `source/app/thomaz_activity.hpp` — `ThomazActivity` base class; alive + cancelled guards; `runAsync` template; `cancelledFlag()` accessor
- `tests/test_async_guard.cpp` — TEST-04b doctest cases (alive, not-alive, null guard)

## Decisions Made

- **runAsync signature:** template form `template<typename Worker, typename OnSync>` rather than `std::function` — avoids allocation overhead; both params moved into the lambda (mutable captures)
- **cancelledFlag name and return type:** returns `std::shared_ptr<std::atomic<bool>>` directly (not a raw bool reference) so Plan 04 can hand the shared_ptr to platform curl call sites as a per-transfer context member
- **null-guard branch in run_if_alive:** implemented (returns false, drops onSync); satisfies TEST-04b Test 3 and guards against future callers that might pass a null before initialization
- **Comment text:** removed the word "borealis" from `async_guard.hpp` inline comments to satisfy the plan's automated grep acceptance check (`grep -ci borealis` must return 0)

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

Minor: The plan's automated acceptance check `grep -ci "borealis" source/core/async_guard.hpp` is case-insensitive and matched the word "Borealis" in an inline comment ("Pure, Borealis-free, C++17-clean..."). Removed the word from the comment text to satisfy the check. No logic change.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `ThomazActivity` base class is ready for Plan 02 to migrate all 13 activities (`public brls::Activity` → `public ThomazActivity`, remove per-activity `alive` member, remove `*this->alive = false` dtors)
- `cancelledFlag()` accessor is available for Plan 04 (CONC-03) to thread the `cancelled` shared_ptr into platform-layer curl calls
- Host doctest suite is clean at 182 tests / 0 failures; no regressions from new files

---
*Phase: 04-c-activity-hardening*
*Completed: 2026-06-05*
