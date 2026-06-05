---
gsd_state_version: 1.0
milestone: v0.4
milestone_name: Extração de Temas
status: executing
last_updated: "2026-06-05T00:12:09.753Z"
last_activity: 2026-06-05
progress:
  total_phases: 4
  completed_phases: 0
  total_plans: 5
  completed_plans: 2
  percent: 40
---

# Project State

## Project Reference

**Core value:** A user can apply downloaded themes on a fresh console using thomaz
alone — no second app — because thomaz extracts the firmware base layouts on-device,
keyless to the user (no `prod.keys` file required).

**Current focus:** Phase 01 — privileged-extraction-spike
(Option A, GPLv2) to extract home-menu base `.szs` into `/themes/systemData/`, feeding
Phase B's existing "Aplicar Tema".

## Current Position

Phase: 01 (privileged-extraction-spike) — EXECUTING
Plan: 3 of 5
Status: Ready to execute
Last activity: 2026-06-05
Progress: [████░░░░░░] 40%

## Roadmap Summary

| Phase | Goal | Requirements |
|-------|------|--------------|
| 1. Privileged Extraction Spike | Prove BIS→lr→SPL→hactool extracts ONE szs on hardware under title takeover | EXTRACT-04, TAKEOVER-01, TAKEOVER-02 |
| 2. Full Extraction Engine | Extract all layouts from all three titles into canonical `/themes/systemData/` | EXTRACT-01, EXTRACT-02, EXTRACT-03 |
| 3. Theme UI Integration | "Extrair layouts do firmware" action; unblock base_missing; state + messaging | INTEG-01..05 |
| 4. Forwarder (Optional) | Installable Application-mode forwarder; skip manual hold-`R` | TAKEOVER-03 (optional) |

## Accumulated Context

### Decisions

- Approach LOCKED: Option A (port exelix BIS+SPL+hactool, GPLv2). Option B
  (`fsOpenFileSystemWithId`) is rejected/out of scope.

- Keyless-to-user only — SPL-derived header key, no `prod.keys` path.
- Re-vendor hactool fork + custom mbedtls (`MBEDTLS_CMAC_C`) — reverses the Phase B
  exclusion. Required by Option A.

- Extraction is hardware-only verified; host doctest covers only pure parsing/mapping.
  Privileged path uses `*_switch.cpp` real impl + `*_fake.cpp` desktop no-op; desktop
  build must stay green.

- Requires title takeover (run as Application); applet mode fails gracefully.
- Output layout: adapt exelix's `extracted/{qlaunch,...}/` subdirs to thomaz's FLAT
  `/themes/systemData/<szs>` layout (confirmed in `cfw_paths.cpp`).

- [Phase ?]: KeyDerivationOutput name avoids grep collision with libnx Result type
- [Phase ?]: SPL key sources pinned to Atmosphère 1.7.1 (b39e29d) — provenance in-code for plan 05 doc

### De-risking rationale

- Highest hardware unknowns sequenced FIRST in Phase 1: (a) do the pinned SPL public
  key sources still derive a valid header key on the target firmware; (b) is
  title-takeover permission sufficient for raw BIS/SPL/pmdmnt/lr. The thin spike
  (one title → one szs) validates both before broader engine/UI work.

### TODOs / Open hardware questions (from research)

- [ ] Confirm pinned public key sources derive valid header key on target FW (highest risk).
- [ ] Confirm `lrLrResolveProgramPath` signature/availability across FW (5.x/11.x/20.x).
- [ ] Confirm exact `/lyt/*.szs` set per firmware (does `common.szs` exist; pre-6.0 semantics).
- [ ] Confirm title-takeover launch path + resulting permission set for thomaz.

### Blockers

- None. Roadmap awaiting user approval before planning Phase 1.

## Session Continuity

Next step: user approves roadmap → `/gsd-plan-phase 1`.

Key files for Phase 1:

- `source/platform/themes/cfw_paths.{hpp,cpp}` — flat `/themes/systemData/` layout +
  `target_map` (consumer of extraction output).

- `source/platform/themes/theme_install.cpp` — `base_present_for()` gate ("base layouts
  missing"); must clear after extraction.

- `source/app/theme_detail_activity.cpp` — surfaces `base_missing`/`base_missing_help`
  (Phase 3 UI integration point).

- `THIRD_PARTY.md` — re-add hactool/mbedtls attribution in Phase 1.
- Reference source: exelix11/SwitchThemeInjector @ `2618b0c`
  (`key_loader.cpp`, `hactool.cpp`, `RomfsCache.cpp`, `Common.{hpp,cpp}`, `NcaDumpPage.cpp`).
