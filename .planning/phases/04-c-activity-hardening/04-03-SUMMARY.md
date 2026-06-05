---
phase: 04-c-activity-hardening
plan: 03
subsystem: concurrency
tags: [cpp, activity, ThomazActivity, runAsync, CONC-02]

# Dependency graph
requires:
  - phase: 04-c-activity-hardening/04-01
    provides: "ThomazActivity base with alive/cancelled guards and runAsync template wrapper"
provides:
  - "mod_detail, clear_cheats, auth, theme_browser, cheat_detail, settings, theme_detail, mod_manager inherit ThomazActivity (not brls::Activity)"
  - "Per-activity alive member removed from all 9 headers — base class owns it"
  - "All brls::async call-sites in 8 files migrated to this->runAsync (D-01 complete app-wide)"
  - "mod_manager base-swapped with alive removal only (no direct brls::async; 4 brls::sync calls preserved unchanged)"
  - "Desktop build clean with zero new warnings after all 9 inheritance changes"
affects:
  - 04-04-PLAN (CONC-03: curl cancellation wiring using cancelledFlag)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "shared_ptr result structs for cross-async data handoff (same pattern as Plan 02): make_shared<T>() before runAsync; worker fills it; onSync reads it"
    - "UI-event alive captures preserved: IME callbacks (theme_browser openSearch) and dialog button callbacks (mod_detail confirmAndDownload) are not brls::async sites — captured alive resolves from ThomazActivity inheritance"

key-files:
  created: []
  modified:
    - source/app/mod_detail_activity.hpp
    - source/app/mod_detail_activity.cpp
    - source/app/clear_cheats_activity.hpp
    - source/app/clear_cheats_activity.cpp
    - source/app/auth_activity.hpp
    - source/app/auth_activity.cpp
    - source/app/theme_browser_activity.hpp
    - source/app/theme_browser_activity.cpp
    - source/app/cheat_detail_activity.hpp
    - source/app/cheat_detail_activity.cpp
    - source/app/settings_activity.hpp
    - source/app/settings_activity.cpp
    - source/app/theme_detail_activity.hpp
    - source/app/theme_detail_activity.cpp
    - source/app/mod_manager_activity.hpp
    - source/app/mod_manager_activity.cpp

key-decisions:
  - "IME and dialog button alive captures preserved as-is (same pattern as Plan 02): these are UI-event closures, not brls::async sites; alive resolves correctly from ThomazActivity inheritance"
  - "mod_manager base-swap-only confirmed: NO direct brls::async lines; its 4 brls::sync([this]{...}) calls at lines 105/176/275/298 are unguarded and out of D-01 scope — left unchanged"
  - "shared_ptr result struct pattern applied consistently for all data handoffs across async boundary (makes data lifetime explicit)"
  - "No DEBT-03 cast edits in any of the 9 files (scope discipline — only Plan 02 four files touched casts)"

patterns-established:
  - "Nine-activity single-pass migration: hpp base-swap + include + alive removal, then cpp dtor drop, then async sites to runAsync — same discipline as Plan 02 four-file pass"

requirements-completed: [CONC-02]

# Metrics
duration: 12min
completed: 2026-06-05
---

# Phase 04 Plan 03: Remaining Activity Migration (CONC-02 Complete) Summary

**Nine activities (mod_detail, clear_cheats, auth, theme_browser, cheat_detail, settings, theme_detail, mod_manager) migrated to ThomazActivity; alive member removed from all 9 headers; 14 async sites across 8 files migrated to runAsync; mod_manager base-swap-only (no brls::async); desktop build clean with zero new warnings; whole-app brls::async count in activity cpps = 0**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-06-05T17:54:29Z
- **Completed:** 2026-06-05T18:06:00Z
- **Tasks:** 2
- **Files modified:** 16 (8 headers + 8 cpps)

## Accomplishments

- Migrated all 9 remaining CONC-02-scope activities from `public brls::Activity` to `public ThomazActivity`; removed per-activity `alive` member from all 9 headers; replaced `#include <atomic>` (where no longer needed) with `#include "app/thomaz_activity.hpp"`
- Dropped 7 hand-rolled dtors (`*this->alive = false;`): mod_detail dtor dropped; clear_cheats dtor dropped; auth dtor dropped; theme_browser dtor dropped; cheat_detail dtor dropped; settings dtor dropped; theme_detail dtor dropped; mod_manager dtor dropped
- Migrated 14 direct `brls::async` call-sites to `this->runAsync(worker, onSync)` across 8 files: mod_detail (2), clear_cheats (1), auth (1), theme_browser (2), cheat_detail (1), settings (3), theme_detail (5)
- `mod_manager`: base-swapped + alive member removed + dtor dropped — NO direct `brls::async` lines; its 4 unguarded `brls::sync([this]{...})` calls at lines 105/176/275/298 preserved unchanged (out of D-01 scope)
- Preserved UI-event alive captures: `theme_browser::openSearch` IME callback and `mod_detail::confirmAndDownload` dialog button both capture `alive = this->alive` by value — these are UI-event closures, not `brls::async` sites; not migrated (same decision as Plan 02 IME captures)
- No DEBT-03 cast changes in any of the 9 files (scope discipline — Pitfall 4)
- Desktop build (`cmake -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON && cmake --build build-desktop --parallel 4`) compiled clean at `[100%] Built target thomaz` — zero errors, zero new warnings from migrated files

## Task Commits

1. **Task 1: Migrate 5 async activities** - `d1b4b0a` (feat)
2. **Task 2: Migrate settings/theme_detail/mod_manager + desktop build** - `8a68459` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

Per-file migration details:

| File | Dtor | Async sites migrated | Notes |
|------|------|---------------------|-------|
| `source/app/mod_detail_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/mod_detail_activity.cpp` | dtor dropped (empty after removal) | 2 (onContentAvailable, startDownload) | dialog alive capture preserved |
| `source/app/clear_cheats_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/clear_cheats_activity.cpp` | dtor dropped (empty after removal) | 1 (onContentAvailable) | |
| `source/app/auth_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/auth_activity.cpp` | dtor dropped (empty after removal) | 1 (submit) | |
| `source/app/theme_browser_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/theme_browser_activity.cpp` | dtor dropped (empty after removal) | 2 (runQuery, loadThumb) | IME alive capture preserved |
| `source/app/cheat_detail_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/cheat_detail_activity.cpp` | dtor dropped (empty after removal) | 1 (onContentAvailable) | |
| `source/app/settings_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/settings_activity.cpp` | dtor dropped (empty after removal) | 3 (checkForUpdate, installUpdate, refreshDatabase) | |
| `source/app/theme_detail_activity.hpp` | removed `alive` decl; removed dtor decl | — | |
| `source/app/theme_detail_activity.cpp` | dtor dropped (empty after removal) | 5 (preview fetch, detail resolve, startDownload, doApply, doRemove) | |
| `source/app/mod_manager_activity.hpp` | removed `alive` decl; removed dtor decl | — | base-swap-only |
| `source/app/mod_manager_activity.cpp` | dtor dropped (one-liner `*this->alive = false;`) | 0 (no direct brls::async) | 4 brls::sync calls preserved unchanged |

## Whole-App brls::async Status

```
grep -rc "brls::async" source/app/*.cpp | awk -F: '{s+=$2} END{print s}'
→ 0
```

The 2 occurrences in `thomaz_activity.hpp` are the `runAsync` base-class implementation — expected and correct. All activity `.cpp` files: zero direct `brls::async` calls.

## Desktop Build Details

**Configure command:** `cmake -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release`
**Build command:** `cmake --build build-desktop --parallel 4`
**Result:** `[100%] Built target thomaz` — zero errors, zero new warnings from modified files
**Warning delta:** Only pre-existing warnings from vendored `lib/switchthemes/third_party/stb_image.h` (unused-function); zero warnings from `source/app/*` files touched in this plan

## Decisions Made

- **IME and dialog button alive captures preserved:** `theme_browser::openSearch` and `mod_detail::confirmAndDownload` dialog button use `alive = this->alive` by value in UI-event closures — not `brls::async` sites; same decision as Plan 02 (non-async closures not migrated)
- **mod_manager confirmed as base-swap-only:** No direct `brls::async` lines; 4 `brls::sync([this]{...})` calls at lines 105/176/275/298 left unchanged (unguarded, out of D-01 scope per plan spec)
- **Scope discipline (Pitfall 4):** Zero cast edits in any of the 9 files — DEBT-03 cast changes are Plan 02 scope only

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None — all data sources wired; no placeholder text or hardcoded empty values introduced.

## Threat Flags

No new network endpoints, auth paths, file access patterns, or schema changes introduced. This is a pure in-repo refactor moving the alive guard from per-activity to base class. T-04-05 mitigation verified: each migration removes the member AND its dtor line in the same per-file pass; every `brls::async` site routes through `runAsync`.

## Self-Check: PASSED

- `04-03-SUMMARY.md` exists on disk (this file)
- All 16 modified source files exist (8 headers + 8 cpps)
- Commit `d1b4b0a` (Task 1) exists in git log
- Commit `8a68459` (Task 2) exists in git log
- All 9 headers declare `public ThomazActivity` (grep confirmed)
- All 9 headers declare no `alive` member (grep confirmed)
- Zero `brls::async` in all activity .cpp files (whole-app grep = 0)
- `mod_manager_activity.cpp` still has 4 `brls::sync([this]` calls (grep = 4)
- `mod_manager_activity.cpp` has zero `this->alive` references (grep = 0)
- Desktop build: `[100%] Built target thomaz`, zero errors, zero new warnings

---
*Phase: 04-c-activity-hardening*
*Completed: 2026-06-05*
