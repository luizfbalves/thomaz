# Roadmap: thomaz — v0.4 Extração de Temas

## Overview

This milestone makes thomaz self-sufficient for theming a fresh console: it ports
exelix's proven BIS + SPL + hactool extraction mechanism (Option A, GPLv2) so the app
can read the home-menu base `.szs` layouts straight out of the running firmware,
keyless to the user, and drop them into `/themes/systemData/` where Phase B's "Aplicar
Tema" already looks. The journey is sequenced to crush risk first: before any UI is
built, a thin on-hardware spike proves the single scariest path — that the pinned SPL
public key sources still derive a valid header key on the target firmware, and that
title takeover grants the privileged FS/SPL access — by extracting ONE title to ONE
`.szs` on the SD card. Only once that is observed working do we broaden to all three
titles, then wrap it in the theme UI, and finally (optional) ship a forwarder so users
skip the manual hold-`R` launch.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [~] **Phase 1: Privileged Extraction Spike** - Re-vendor hactool+mbedtls, run as a title takeover, and prove BIS→lr→SPL→hactool extracts ONE qlaunch `.szs` to the SD on real hardware (code-complete 2026-06-04; awaiting on-hardware validation — plan 01-05 Task 2 human-verify checkpoint open)
- [ ] **Phase 2: Full Extraction Engine** - Extract every required layout from all three titles (qlaunch, Psl, MyPage) into the canonical `/themes/systemData/` flat layout
- [ ] **Phase 3: Theme UI Integration** - "Extrair layouts do firmware" action with already-extracted/re-extract state, firmware-version record, base_missing unblock, and clear success/failure messaging
- [ ] **Phase 4: Forwarder (Optional)** - Provide and document an installable Home-menu forwarder that launches thomaz directly in Application mode

## Phase Details

### Phase 1: Privileged Extraction Spike
**Goal**: Prove the entire privileged extraction path works end-to-end on real hardware with the smallest possible vertical slice — re-vendor the removed dependencies, run thomaz under title takeover, and extract a single qlaunch layout to the SD card — so the two highest-risk unknowns (SPL key derivation on the target firmware, and title-takeover permission sufficiency) are validated before any further work is built on top.
**Depends on**: Nothing (first phase)
**Requirements**: EXTRACT-04, TAKEOVER-01, TAKEOVER-02
**Success Criteria** (what must be TRUE):
  1. On hardware, launching thomaz via title takeover and running the spike produces a valid, non-empty `ResidentMenu.szs` written to the SD card — derived without any user-supplied `prod.keys` (SPL derives the header key on-device).
  2. The desktop build stays green: the extraction entry point compiles and links via a `*_fake.cpp` no-op (no BIS/SPL/hactool symbols pulled into the desktop target).
  3. Running the spike in applet mode shows a clear "relaunch via title takeover" message and exits cleanly — no crash and no silent `fsOpenBisFileSystem` failure.
  4. The required title-takeover launch path (how to hold-`R` / override into Application mode) is written down in user-facing docs, and SPL key-source provenance is recorded against the firmware the spike ran on.
**Plans**: 5 plans
**UI hint**: yes

Plans:
- [x] 01-01-PLAN.md: Re-vendor the hactool fork + a CMAC-enabled `mbedtls` (`MBEDTLS_CMAC_C`, built from source); wire both as Switch-only CMake static libs (correct link order) and re-add `THIRD_PARTY.md` attribution — Wave 1
- [x] 01-02-PLAN.md: Port `key_loader` (`__SWITCH__`) — pmdmnt/spl/splCrypto init, raw BIS System mount, `lr` title→NCA resolve, SPL header-key derivation from pinned public sources (keyless, EXTRACT-04) — Wave 2
- [x] 01-03-PLAN.md: Port a minimal `hactool` facade — in-memory NCA RomFS read with a `/lyt/*.szs` filename filter, keyed by the SPL-derived header key — Wave 2
- [x] 01-04-PLAN.md: Add the `platform/themes` extraction entry point with a `*_switch.cpp` real impl + `*_fake.cpp` desktop no-op; applet-vs-Application gate (TAKEOVER-01) → compose key_loader + hactool → validate → write to canonical SD path; on-hardware spike run — Wave 3
- [x] 01-05-PLAN.md: Document the title-takeover launch path (TAKEOVER-02) and record firmware/key-source provenance from the on-hardware spike — Wave 4

### Phase 2: Full Extraction Engine
**Goal**: Generalize the proven spike into the complete extraction engine: pull every required `.szs` from all three system titles (qlaunch ResidentMenu/Entrance/Flaunch/Set/Notification/common, Psl, MyPage) and write them into the exact flat `/themes/systemData/` layout `cfw_paths::base_layout_dir()` expects.
**Depends on**: Phase 1
**Requirements**: EXTRACT-01, EXTRACT-02, EXTRACT-03
**Success Criteria** (what must be TRUE):
  1. On hardware, a full extraction run produces all six qlaunch layouts (ResidentMenu, Entrance, Flaunch, Set, Notification, common) on the SD card.
  2. On hardware, the same run extracts the Psl layout from title `…1007` and the MyPage layout from title `…1013`.
  3. Every extracted `.szs` lands directly in `/themes/systemData/` (flat layout, adapted from exelix's `extracted/{qlaunch,...}/` subdirs) with the exact filenames `target_map()` resolves to.
  4. Pure parsing/target-table logic (title-ID → szs-name mapping, path resolution) is covered by host doctest where it does not touch privileged services.
**Plans**: 4 plans
**UI hint**: no

Plans:
- [ ] 02-01-PLAN.md — Add `common` to `target_map()` (D-01a) + extend `test_cfw_paths` doctest; declare `ExtractAllResult` + `extract_all_base_layouts()` in the neutral `firmware_extract.hpp` and add the desktop no-op (interface contract) — Wave 1
- [ ] 02-02-PLAN.md — Extract D-04 structural validation into a neutral `szs_validate.{hpp,cpp}` (Yaz0+SARC) + host doctest; bump `tests/Makefile` to C++20 with SarcLib so success criterion 4 covers validation — Wave 1
- [ ] 02-03-PLAN.md — Widen `nca_romfs_filter` to a `/lyt/` directory prefix (D-01); implement `extract_all_base_layouts()` single-session multi-title best-effort driver (three titles, D-02/D-02a, D-04 validate, D-03 flat overwrite) — Wave 2
- [ ] 02-04-PLAN.md — Hardware verify: run `extract_all_base_layouts()` under title takeover, confirm all six qlaunch layouts + Psl + MyPage land flat in `/themes/systemData/` (success criteria 1-3) — Wave 3 (checkpoint)

### Phase 3: Theme UI Integration
**Goal**: Surface extraction as a first-class one-time action in the theme UI, wired so a successful run immediately unblocks "Aplicar Tema", with visible already-extracted/re-extract state, a recorded firmware version, and clear success/failure messaging.
**Depends on**: Phase 2
**Requirements**: INTEG-01, INTEG-02, INTEG-03, INTEG-04, INTEG-05
**Success Criteria** (what must be TRUE):
  1. User can start extraction from an "Extrair layouts do firmware" action in the theme UI.
  2. After a successful extraction, the theme detail page no longer shows the `base_missing` block and "Aplicar Tema" proceeds — because outputs satisfy `cfw_paths::base_present_for()`.
  3. The UI shows whether layouts are already extracted and lets the user re-extract on demand (e.g. after a firmware update), recording the firmware version it ran against.
  4. On completion the user sees a clear success message, or on failure a message naming the reason (e.g. applet mode, key-derivation failure, missing title).
**Plans**: TBD
**UI hint**: yes

Plans:
- [ ] 03-01: Add the "Extrair layouts do firmware" action/page to the theme UI and wire it to the extraction engine with progress + result reporting
- [ ] 03-02: Persist extraction state + firmware version (via `setsysGetFirmwareVersion`); render already-extracted status and a re-extract path
- [ ] 03-03: Map engine outcomes to user-facing success/failure messages; confirm `base_missing` clears and "Aplicar Tema" runs immediately after extraction

### Phase 4: Forwarder (Optional)
**Goal**: Remove the manual hold-`R` step by providing and documenting an installable Home-menu forwarder that launches thomaz directly in Application mode, so extraction's title-takeover requirement is satisfied by a single icon tap. This phase is OPTIONAL and must not block the core extraction path (Phases 1-3 are fully usable via the manual title-takeover route documented in Phase 1).
**Depends on**: Phase 3
**Requirements**: TAKEOVER-03 (optional)
**Success Criteria** (what must be TRUE):
  1. A forwarder (installable Home-menu icon) launches thomaz in Application mode, and extraction run from that launch succeeds without the manual hold-`R` step.
  2. Installing and using the forwarder is documented for users, including any caveats vs. the manual title-takeover route.
**Plans**: TBD
**UI hint**: yes

Plans:
- [ ] 04-01: Provide the forwarder artifact + install steps and document the Application-mode launch path; verify extraction works when launched via the forwarder

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Privileged Extraction Spike | 5/5 | Complete   | 2026-06-05 |
| 2. Full Extraction Engine | 0/4 | Not started | - |
| 3. Theme UI Integration | 0/3 | Not started | - |
| 4. Forwarder (Optional) | 0/1 | Not started | - |
