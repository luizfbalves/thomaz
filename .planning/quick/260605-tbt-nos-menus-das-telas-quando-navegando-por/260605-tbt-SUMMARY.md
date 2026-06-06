---
phase: quick-260605-tbt
plan: "01"
subsystem: ui/focus
tags: [borealis, focus, gamepad, async, activities]
dependency_graph:
  requires: []
  provides: [claimInitialFocus helper, initial gamepad focus for 5 async-list screens]
  affects: [source/app/thomaz_activity.hpp, source/app/game_list_activity.cpp, source/app/save_manager_activity.cpp, source/app/mod_browser_activity.cpp, source/app/mod_detail_activity.cpp, source/app/clear_cheats_activity.cpp]
tech_stack:
  added: []
  patterns: [idempotent focus-claim guard, shared base-class helper for Borealis activities]
key_files:
  created: []
  modified:
    - source/app/thomaz_activity.hpp
    - source/app/game_list_activity.cpp
    - source/app/save_manager_activity.cpp
    - source/app/mod_browser_activity.cpp
    - source/app/mod_detail_activity.cpp
    - source/app/clear_cheats_activity.cpp
decisions:
  - "Inline implementation in thomaz_activity.hpp (no new .cpp) — ThomazActivity had no .cpp and the method is small; borealis.hpp already included"
  - "Private guard flag initialFocusClaimed_ on ThomazActivity — each activity push creates a fresh instance, so no cross-activity state leakage"
  - "claimInitialFocus placed after all addView() calls in each populate/rebuildList — ensures getDefaultFocus() finds actual content"
metrics:
  duration: ~5 min
  completed: "2026-06-06T00:17:01Z"
  tasks_completed: 2
  files_modified: 6
---

# Quick 260605-tbt: Fix gamepad focus highlight stuck on previous screen — Summary

**One-liner:** Added idempotent `claimInitialFocus(container)` helper to `ThomazActivity` and wired it into the 5 async-list screens so the Borealis focus highlight appears in the new screen instead of staying on the previous card when navigating by gamepad/d-pad.

## Root Cause (D-01)

`brls::Application::pushActivity` calls `giveFocus(activity->getDefaultFocus())`. For screens whose focusable content is built asynchronously (spinner visible at push time), `getDefaultFocus()` returns null → `giveFocus(null)` is a no-op → `currentFocus` remains on the card of the previous screen → the highlight is drawn over the new screen.

Fix: after the async build completes and rows are `addView`'d, call `giveFocus` on the first focusable descendant. An idempotency guard (`initialFocusClaimed_`) ensures that subsequent rebuilds triggered by toggle actions (X/Y buttons in game list) do not reposition focus away from the user's current item.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Add claimInitialFocus + apply in game_list and save_manager | `1752694` | thomaz_activity.hpp, game_list_activity.cpp, save_manager_activity.cpp |
| 2 | Apply claimInitialFocus in mod_browser, mod_detail, clear_cheats | `604b564` | mod_browser_activity.cpp, mod_detail_activity.cpp, clear_cheats_activity.cpp |

## Changes Made

### thomaz_activity.hpp

Added to the `protected:` section (after `runAsync`):

```cpp
void claimInitialFocus(brls::View* container)
{
    if (initialFocusClaimed_ || !container) return;
    brls::View* f = container->getDefaultFocus();
    if (!f) return;
    brls::Application::giveFocus(f);
    initialFocusClaimed_ = true;
}
```

Added to a new `private:` section at the end of the class:

```cpp
bool initialFocusClaimed_ = false;
```

No new `#include` needed — `<borealis.hpp>` was already present.

### Call sites (all in their respective `onSync` callbacks, after all rows are added)

| File | Method | Container ID | Position |
|------|--------|--------------|----------|
| game_list_activity.cpp | rebuildList() | gameListBox | after visibility set, before loadCheatIndexAsync() |
| save_manager_activity.cpp | populate() | saveListBox | last statement after row loop |
| mod_browser_activity.cpp | populate() | resultsBox | last statement after load-more block |
| mod_detail_activity.cpp | populate() | filesBox | last statement after file-row loop |
| clear_cheats_activity.cpp | populate() | clearListBox | last statement after clearBtn addView |

## Guard Behavior

- `initialFocusClaimed_` starts `false` on every new activity push (fresh instance).
- First successful `claimInitialFocus` call sets it `true`.
- `GameListActivity::rebuildList()` is also triggered by toggle X/Y (hide/show games). The guard ensures these rebuilds do not reset the user's cursor position.
- Empty-list paths: `getDefaultFocus()` returns null on an empty container → early-return → no spurious focus claim.

## Verification

**Automated (run in this session):**
- `grep` confirmed `initialFocusClaimed_` and `claimInitialFocus` present in `thomaz_activity.hpp` (lines 84, 88, 92, 82).
- `grep` confirmed single `claimInitialFocus` call in each of the 5 `.cpp` files (game_list:273, save_manager:132, mod_browser:363, mod_detail:166, clear_cheats:114).
- Call positions verified by code inspection: placed after all `addView()` calls in each method, within the `onSync` callback path (UI thread).
- `cd tests && make && ./run`: **208/208 passed, 0 failed** — no regressions introduced.

**NOT compile-checked in this environment:**
- The Borealis NRO app (`source/app/`) cannot be built here: `main.cpp` includes `<switch.h>` (Switch-only header, no devkitPro installed) and the desktop build target has been removed (Phase 5). Compilation correctness for the app layer must be verified by the user with the devkitPro Switch toolchain (`make` in the Switch build environment).

## Deviations from Plan

None — plan executed exactly as written. The `protected:` section placement (after `runAsync`, before a new `private:` close) was chosen to keep all guard members together without splitting the existing `private:` members above.

## Known Stubs

None introduced.

## Threat Flags

None — no new network endpoints, auth paths, file access patterns, or schema changes. Focus management runs entirely on the UI thread within existing activity lifecycle boundaries (T-260605tbt-01 and T-260605tbt-02 accepted per threat register in PLAN.md).

## Self-Check: PASSED

All 6 modified source files verified present on disk. Both task commits (1752694, 604b564) found in git history.
