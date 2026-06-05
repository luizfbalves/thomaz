---
quick_id: 260605-ot7
phase: quick
plan: 260605-ot7
subsystem: theme-detail-activity
tags: [ux, busy-guard, download, async, borealis]
completed_date: "2026-06-05"
duration: ~8min
tasks_completed: 1
tasks_total: 1
files_modified: 2
key_decisions:
  - "setButtonBusy placed after refreshActionButton() in cpp — natural grouping with other button-state helpers"
  - "Belt-and-suspenders: setFocusable(!busy) on the view + if(busy)return in click handler — both needed because gesture recognizers can bypass focus"
  - "doExtract() left with bare this->busy assignments as specified — it uses brls::async not runAsync and has no downloadButton involvement"
dependency_graph:
  requires: []
  provides: [UX-01]
  affects: [theme_detail_activity]
tech_stack:
  added: []
  patterns: [busy-guard, visual-feedback, setAlpha, setFocusable]
key_files:
  created: []
  modified:
    - source/app/theme_detail_activity.hpp
    - source/app/theme_detail_activity.cpp
---

# Phase quick Plan 260605-ot7: Theme Detail Busy-Guard UX Summary

**One-liner:** Added `setButtonBusy(bool)` helper to dim (alpha 0.5) and defocus the download button during in-flight async operations, with a belt-and-suspenders early-return guard in the click handler to block duplicate download/apply/remove launches.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Add setButtonBusy helper + wire into all async paths | 4f9205b | theme_detail_activity.hpp, theme_detail_activity.cpp |

## What Was Done

### Task 1: Add setButtonBusy helper to header and implement in .cpp

**Header (`source/app/theme_detail_activity.hpp`):**
- Added `void setButtonBusy(bool busy);` private method declaration after `refreshActionButton()`.

**Implementation (`source/app/theme_detail_activity.cpp`):**

1. **`setButtonBusy(bool busy)`** — implemented after `refreshActionButton()`:
   - Sets `this->busy = busy`
   - Resolves `downloadButton` view; returns early if null
   - Calls `btn->setFocusable(!busy)` and `btn->setAlpha(busy ? 0.5f : 1.0f)`

2. **`startDownload()`** — two changes:
   - Added `if (this->busy) return;` guard after the `!resolved` guard
   - Added `this->setButtonBusy(true)` at entry (before notify)
   - In the `onSync` lambda: replaced the bare notify+state block with `this->setButtonBusy(false)` first, then notify, then success state update — covers both success and failure paths

3. **`doApplyMode(bool)`** — replaced bare `this->busy = true` with `this->setButtonBusy(true)` and `this->busy = false` with `this->setButtonBusy(false)` in the `onSync` lambda.

4. **`doRemove()`** — same substitution: `this->busy = true` → `this->setButtonBusy(true)`, `this->busy = false` → `this->setButtonBusy(false)`.

5. **Click handler (in `onContentAvailable`)** — added `if (this->busy) return;` immediately after the `if (!alive->load()) return;` alive guard. Belt-and-suspenders: the button is already non-focusable via `setButtonBusy`, but gesture recognizers and programmatic calls can bypass focus.

**Left unchanged (per spec):**
- `doApply()` — already has its own `!resolved || busy` guard; delegates to `doApplyMode`
- `doExtract()` — uses `brls::async` (not `runAsync`); sets `this->busy` directly; no `downloadButton` visual relationship
- Dialog callbacks, `analyzeCompat()`, `updateCompatBadge()` — untouched

## Verification Results

**Code inspection (grep):**
- `grep -n "setButtonBusy" theme_detail_activity.hpp` — 1 line (declaration) ✓
- `grep -n "setButtonBusy" theme_detail_activity.cpp` — 7 lines (1 impl + 6 call sites) ✓
- `grep -n "this->busy = " theme_detail_activity.cpp` — 3 lines: 1 inside `setButtonBusy` itself (line 277), 2 in `doExtract()` (lines 443/465) — none in startDownload/doApplyMode/doRemove ✓

**Desktop build:**
```
[100%] Built target thomaz
```
Zero errors. Zero warnings. ✓

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None.

## Threat Flags

None — changes are purely UI state management within an existing activity; no new network endpoints, auth paths, or file access patterns introduced.

## Self-Check: PASSED

- [x] `source/app/theme_detail_activity.hpp` exists and declares `setButtonBusy`
- [x] `source/app/theme_detail_activity.cpp` exists and implements `setButtonBusy` with setFocusable + setAlpha
- [x] Commit `4f9205b` exists
- [x] Desktop build clean (exit 0, no errors or warnings)
