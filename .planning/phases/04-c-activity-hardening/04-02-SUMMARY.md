---
phase: 04-c-activity-hardening
plan: 02
subsystem: concurrency
tags: [cpp, activity, ThomazActivity, runAsync, dynamic_cast, DEBT-03, CONC-02, async_guard]

# Dependency graph
requires:
  - phase: 04-c-activity-hardening/04-01
    provides: "ThomazActivity base with alive/cancelled guards and runAsync template wrapper"
  - phase: 03-c-platform-hardening/03-04
    provides: "cloudBusy atomicized (CONC-01) — safe to edit save_detail_activity.hpp"
provides:
  - "game_list, save_manager, save_detail, mod_browser inherit ThomazActivity (not brls::Activity)"
  - "Per-activity alive member removed from all 4 headers — base class owns it"
  - "All brls::async call-sites in 4 files migrated to this->runAsync (CONC-02 complete for this set)"
  - "DEBT-03 null-guarded dynamic_cast for every view cast in these 4 files"
  - "Desktop build clean with zero new warnings after inheritance and cast changes"
affects:
  - 04-03-PLAN (CONC-03: curl cancellation wiring)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "shared_ptr result structs for cross-async data handoff: make_shared<T>() before runAsync; worker fills it; onSync reads it — no raw captures across async boundary"
    - "null-guarded dynamic_cast contract: bare C-style cast -> dynamic_cast<T*> + if (!ptr) { Logger::error(...); return; }"
    - "Non-brls::async alive captures preserved in IME and dialog button callbacks — only brls::async sites migrated to runAsync"

key-files:
  created: []
  modified:
    - source/app/mod_browser_activity.hpp
    - source/app/mod_browser_activity.cpp
    - source/app/save_detail_activity.hpp
    - source/app/save_detail_activity.cpp
    - source/app/game_list_activity.hpp
    - source/app/game_list_activity.cpp
    - source/app/save_manager_activity.hpp
    - source/app/save_manager_activity.cpp

key-decisions:
  - "shared_ptr<T> result struct for data handoff across runAsync worker/onSync boundary — worker fills shared_ptr, onSync reads it; cleaner than capturing mutable locals by reference across async boundary"
  - "Non-async alive captures in IME callbacks and dialog buttons preserved as-is — these are UI-event closures, not brls::async sites; alive is now inherited from ThomazActivity so they resolve correctly"
  - "Redundant null checks removed after mandatory null-guard early-returns in game_list and save_manager populate() — simplifies code without behavior change"
  - "save_detail_activity.hpp cloudBusy std::atomic<bool> (CONC-01) left entirely untouched per S2 constraint"

patterns-established:
  - "All 4 shared-file activities (DEBT-03 + CONC-02 scope) migrated in single-pass per-file: hpp base-swap + alive removal first, then cpp dtor drop, then async sites, then DEBT-03 casts"
  - "runAsync data handoff: std::make_shared<ResultType>() before runAsync; worker lambda captures and fills it; onSync lambda reads it — avoids allocation of std::function + safe across async boundary"

requirements-completed: [CONC-02, DEBT-03]

# Metrics
duration: 20min
completed: 2026-06-05
---

# Phase 04 Plan 02: Activity Base Migration (DEBT-03 + CONC-02 Shared Files) Summary

**Four activities (game_list, save_manager, save_detail, mod_browser) migrated from brls::Activity to ThomazActivity in a single pass per file: alive member removed, 12 async sites migrated to runAsync, 11 C-style casts replaced with null-guarded dynamic_cast; desktop build clean with zero new warnings**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-06-05T17:38:00Z
- **Completed:** 2026-06-05T17:50:23Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- Migrated all 4 DEBT-03/CONC-02 shared activities from `public brls::Activity` to `public ThomazActivity`; removed per-activity `alive` member declaration from all 4 headers
- Removed 4 hand-rolled dtors (`*this->alive = false;`) — base dtor handles it; 3 dtors dropped entirely (now empty), 1 was already empty after removal
- Migrated 12 `brls::async/brls::sync` call-sites to `this->runAsync(worker, onSync)` across 4 files: mod_browser (3), save_detail (6), game_list (2), save_manager (1)
- Replaced 11 C-style `(brls::T*)this->getView(...)` casts with `dynamic_cast<brls::T*>` + null guard (log + return); added new null guards to all bare-assignment sites
- `save_detail_activity.hpp` `cloudBusy std::atomic<bool>` (Phase 3 CONC-01) untouched
- Desktop build (`cmake -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON && cmake --build ...`) compiled clean at `[100%] Built target thomaz` — zero errors, zero new warnings from migrated files

## Task Commits

1. **Task 1: Migrate mod_browser + save_detail** - `740f04f` (feat)
2. **Task 2: Migrate game_list + save_manager + clean desktop build** - `1cbf2a7` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

Per-file migration details:

| File | Dtor | Async sites migrated | Casts hardened |
|------|------|---------------------|----------------|
| `source/app/mod_browser_activity.hpp` | removed decl | — | — |
| `source/app/mod_browser_activity.cpp` | dtor dropped (empty after removal) | 3 (onContentAvailable, runGameSearch, runGlobalSearch) | 5 (emptyLabel x3, resolvedLabel, resultsBox/emptyLabel in populate) |
| `source/app/save_detail_activity.hpp` | removed decl; cloudBusy preserved | — | — |
| `source/app/save_detail_activity.cpp` | dtor dropped (empty after removal) | 6 (doBackup, performRestore, doUpload, pushAtRevision, doDownload, refreshCloudStatus) | 3 (lastBackup, historyBox, cloudStatus) |
| `source/app/game_list_activity.hpp` | removed decl | — | — |
| `source/app/game_list_activity.cpp` | dtor dropped (empty after removal) | 2 (onContentAvailable, loadCheatIndexAsync) | 2 (gameListBox, emptyLabel) |
| `source/app/save_manager_activity.hpp` | removed decl | — | — |
| `source/app/save_manager_activity.cpp` | dtor dropped (empty after removal) | 1 (onContentAvailable) | 2 (saveListBox, emptyLabel) |

## Decisions Made

- **runAsync data handoff pattern:** Worker and onSync share data via a `std::make_shared<ResultType>()` struct allocated before the call. Worker captures and writes into it; onSync captures and reads from it. This is safe because the shared_ptr keeps the result object alive past the worker's stack frame and the onSync runs on the UI thread after the worker completes.
- **Non-async alive captures preserved:** IME keyboard callbacks (`[this, alive = this->alive]`) and dialog button callbacks in save_detail are not `brls::async` call-sites — they're synchronous UI-event closures. They correctly capture `alive` by value (now inherited from `ThomazActivity`) for their own out-of-band lifetime guard. Not migrated to runAsync since they have no worker/onSync structure.
- **Redundant null-guard simplification:** After adding mandatory null-guard returns for bare-assignment casts (gameListBox/emptyLabel in game_list, saveListBox/emptyLabel in save_manager), the subsequent `if (emptyLabel)` / `if (listBox)` defensive checks in the same function were always-true. Removed them to keep code simple.

## Deviations from Plan

None — plan executed exactly as written. The single-pass per-file edit order from PATTERNS.md was followed (hpp base-swap → cpp dtor drop → runAsync migration → DEBT-03 casts).

## Issues Encountered

None. The `runAsync` template signature from Plan 01 takes two zero-argument callables (`worker()` and `onSync()`), requiring data to be shared via captured `shared_ptr` structs rather than function arguments. This was the expected pattern — the Plan 01 SUMMARY documented the exact template form used.

## Desktop Build Details

**Configure command:** `cmake -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release`
**Build command:** `cmake --build build-desktop --parallel 4`
**Result:** `[100%] Built target thomaz` — zero errors, zero new warnings from modified files
**Warning delta (existing pre-plan warnings):** Only pre-existing warnings from vendored `lib/switchthemes/third_party/stb_image.h` (unused-function); zero warnings from `source/app/*` files touched in this plan

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- Plans 02 (this plan) provides the 4 DEBT-03/CONC-02 shared activities migrated; Plans 03 (the remaining 9 activities — base-swap + runAsync only, no DEBT-03) can proceed
- `cancelledFlag()` accessor on `ThomazActivity` is available for Plan 04 (CONC-03) to thread the `cancelled` shared_ptr into platform-layer curl calls

## Self-Check: PASSED

- `04-02-SUMMARY.md` exists on disk
- All 8 source files exist (4 headers + 4 cpp)
- Commit `740f04f` (Task 1) exists in git log
- Commit `1cbf2a7` (Task 2) exists in git log
- All 4 headers: `public ThomazActivity` confirmed by grep
- Zero `brls::async` calls remain in all 4 cpp files
- Zero C-style `(brls::T*)this->getView` casts remain in all 4 cpp files
- `cloudBusy` preserved in `save_detail_activity.hpp`

---
*Phase: 04-c-activity-hardening*
*Completed: 2026-06-05*
