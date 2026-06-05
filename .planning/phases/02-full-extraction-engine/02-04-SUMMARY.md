---
phase: 02-full-extraction-engine
plan: "04"
subsystem: platform/themes extraction engine (hardware verification gate)
status: awaiting-hardware
tags: [hardware-verify, checkpoint, deferred, title-takeover, extract-all]
dependency_graph:
  requires: ["02-03"]
  provides: ["extract_all_base_layouts-device-trigger"]
  affects: ["source/app/theme_detail_activity.cpp"]
tech_stack:
  added: []
  patterns:
    - "Phase 2 verification trigger reuses existing doExtract() / Extract Now button (no new UI)"
    - "ExtractAllResult printf logging to /switch/thomaz/hactool.log for hardware readout"
key_files:
  modified:
    - source/app/theme_detail_activity.cpp
decisions:
  - "Task 1 (auto) complete and committed; Task 2 (checkpoint:human-verify, blocking) DEFERRED by user — no real Switch available this session"
  - "Trigger is the existing 'Extract Now' button in showBaseMissingDialog → doExtract(), now calling extract_all_base_layouts() instead of the Phase 1 per-target loop"
  - "No new i18n keys — reuses extract_fail/extract_ok/extracting"
metrics:
  duration: "~3 minutes (Task 1 only)"
  completed: "2026-06-05 (Task 1); Task 2 pending hardware"
  tasks_completed: 1
  tasks_total: 2
  files_changed: 1
---

# Phase 02 Plan 04: Hardware Verification Gate — PROVISIONAL (Task 2 deferred)

**One-liner:** Task 1 wired a minimal on-device trigger (`doExtract()` → `extract_all_base_layouts()` with `ExtractAllResult` logging); Task 2, the blocking on-hardware verification, is **deferred** — no real Switch was available this session.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Pre-flight — wire device trigger for extract_all_base_layouts() with result logging | e14c820 | source/app/theme_detail_activity.cpp |

## Task Deferred

| Task | Name | Status |
|------|------|--------|
| 2 | Hardware verification — run extract_all_base_layouts() under title takeover | ⏸ DEFERRED — awaiting real Switch (blocking gate, NOT approved) |

## What Was Built (Task 1)

`doExtract()` in `source/app/theme_detail_activity.cpp` now invokes the Phase 2 multi-title
driver `extract_all_base_layouts()` (replacing the Phase 1 per-target
`extract_base_layout()` loop) and printf-logs the full `ExtractAllResult` — `ok`,
`written_parts` count, each written path, `failed_parts` list, `systemic_error`, and the
firmware version — to `/switch/thomaz/hactool.log`. The trigger is the existing
"Extract Now" button in the base-missing dialog; no new UI strings or i18n keys were
added (Phase 3 owns permanent UI wiring, INTEG-01/INTEG-05). The call site is annotated
`[Phase 2 verification trigger]` for Phase 3 promotion.

## Hardware Verification Procedure (for when a device is available)

1. Build the Switch NRO and copy to SD.
2. Launch thomaz via title takeover (hold R while opening a game) → Application mode.
3. Themezer browser → select a theme → Apply → base-missing dialog → **Extract Now**.
4. Inspect `/themes/systemData/`:
   - Six qlaunch layouts flat: `ResidentMenu.szs`, `Entrance.szs`, `Flaunch.szs`, `Set.szs`, `Notification.szs`, `common.szs` (criterion 1)
   - `Psl.szs` + `MyPage.szs` present (criterion 2); extras like `Option.szs`/`Eula.szs` OK
   - All flat, no `extracted/qlaunch/` nesting (criterion 3); each non-empty
5. Read `/switch/thomaz/hactool.log`: record `written`/`failed` counts + `firmware=` version. `ok=1` with a few `failed_parts` = correct D-02 best-effort.
6. (Optional negative path) applet mode → Extract Now → confirm relaunch message, nothing written, no crash (TAKEOVER-01).

## Self-Check

### Created/modified files exist:
- `source/app/theme_detail_activity.cpp` — modified (exists)

### Commits exist:
- `e14c820` — feat(02-04): wire extract_all_base_layouts() trigger in doExtract()

## Self-Check: PARTIAL — Task 1 complete; Task 2 (blocking hardware gate) DEFERRED, not approved
