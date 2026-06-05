---
gsd_state_version: 1.0
milestone: v0.4
milestone_name: Extração de Temas
status: executing
last_updated: "2026-06-05T16:27:18.343Z"
last_activity: 2026-06-05
progress:
  total_phases: 4
  completed_phases: 1
  total_plans: 9
  completed_plans: 8
  percent: 89
---

# Project State

## Project Reference

**Core value:** A user can apply downloaded themes on a fresh console using thomaz
alone — no second app — because thomaz extracts the firmware base layouts on-device,
keyless to the user (no `prod.keys` file required).

**Current focus:** Phase 02 — full-extraction-engine
(Option A, GPLv2) to extract home-menu base `.szs` into `/themes/systemData/`, feeding
Phase B's existing "Aplicar Tema".

## Current Position

Phase: 02 (full-extraction-engine) — EXECUTING
Plan: 4 of 4
Status: Ready to execute
Last activity: 2026-06-05
Progress: [█████████░] 89%

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

- [Phase 02 Plan 01]: `common` arm in `target_map()` uses title-ID `0100000000001000` (qlaunch) and szs `common.szs`, confirmed against `ThemeTargetInfo::QlaunchCommon` in `lib/switchthemes/Common.cpp`
- [Phase 02 Plan 01]: `ExtractAllResult` is additive alongside existing `ExtractResult`; `extract_base_layout` preserved for live caller in `theme_detail_activity.cpp`
- [Phase 02 Plan 01]: `ExtractAllResult` contract — `ok=false` only on systemic abort; `failed_parts` collects per-part warnings; `ok=false` implies `written_parts` is empty

- [Phase 02 Plan 02]: `szs_validate.cpp` doc comment rephrased to avoid literal `switch.h` text (neutral-TU acceptance grep checks for its absence as an include; comment text was triggering a false match)

- [Phase 02 Plan 03]: `nca_romfs_filter` prefix check uses `rfind(entry,0)==0` (starts_with semantics) — avoids matching `/lyt/` in interior path segments
- [Phase 02 Plan 03]: `extract_all_base_layouts()` opens privileged session once before the three-title loop, closes once on all exit paths (Pitfall 2 / T-02-10)

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

- **Phase 01 awaits on-hardware validation** — plan 05 Task 1 (docs) is complete
  (commit b7d3548), but Task 2 (human-verify) is open. The firmware extraction spike
  code is complete (plans 01-04 merged) but has NOT yet been executed on a real Nintendo
  Switch. Once the first hardware run succeeds:

  1. Record `setsysGetFirmwareVersion` major.minor.micro in `docs/title-takeover.md`
     Proveniência table and in `THIRD_PARTY.md` `## SPL key sources` block.

  2. Confirm Atmosphère 1.7.1/b39e29d key sources derived a valid header key.
  3. Approve Task 2 checkpoint (signal: "approved" or correction description).

## Session Continuity

Last completed: 02-03-PLAN.md — /lyt/ prefix RomFS filter + extract_all_base_layouts() multi-title driver (1785033, d5bfc11). D-01 prefix widening in nca_romfs_filter; D-02/D-02a best-effort/systemic split; D-03 flat overwrite; D-04 is_structurally_valid_szs write gate; single session across all three titles.
Next step: Execute Phase 02 Plan 04 (firmware_extract_fake.cpp stub + integration wiring).

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
