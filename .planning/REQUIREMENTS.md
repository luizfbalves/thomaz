# Requirements — Milestone v0.4: Extração de Temas

Native on-device extraction of firmware home-menu base layouts into
`/themes/systemData/`, implementing the "Path B" extraction deferred in Phase B.
Approach locked: **Option A** (port exelix BIS + SPL + hactool, GPLv2).

## v0.4 Requirements

### Extraction Engine (EXTRACT)
- [ ] **EXTRACT-01**: User can extract the qlaunch home-menu layouts (ResidentMenu, Entrance, Flaunch, Set, Notification, common) from the running firmware to the SD card
- [ ] **EXTRACT-02**: User can extract the player-select (Psl) layout
- [ ] **EXTRACT-03**: User can extract the MyPage layout
- [ ] **EXTRACT-04**: Extraction succeeds without the user supplying a `prod.keys` file (NCA header key derived on-device via SPL)

### App Integration (INTEG)
- [ ] **INTEG-01**: User can start extraction from an "Extrair layouts do firmware" action in the theme UI
- [ ] **INTEG-02**: Extracted layouts are written to `/themes/systemData/` in the exact layout `cfw_paths` expects, so "Aplicar Tema" works immediately afterward with no `base_missing` block
- [ ] **INTEG-03**: User can see whether layouts are already extracted and can re-extract on demand (e.g. after a firmware update)
- [ ] **INTEG-04**: Extraction records the firmware version it ran against
- [ ] **INTEG-05**: User gets a clear success message, or a failure message naming the reason

### Title Takeover (TAKEOVER)
- [ ] **TAKEOVER-01**: If extraction is attempted in applet mode, the user is shown a clear "relaunch via title takeover" message instead of a crash or silent failure
- [ ] **TAKEOVER-02**: The required title-takeover launch path for privileged FS/SPL access is documented for users
- [ ] **TAKEOVER-03** *(optional)*: Provide and document a forwarder (installable Home-menu icon) that launches thomaz directly in Application mode, so users avoid the manual hold-`R` step

## Future Requirements (deferred)
- Auto-detect firmware update and prompt re-extraction
- Option B (`fsOpenFileSystemWithId`) as a lighter fallback if Option A breaks on a future firmware

## Out of Scope
- The `prod.keys`-on-SD / offline-dump extraction path — only the on-device, keyless-to-user path
- Distributing extracted Nintendo `.szs` assets (copyrighted)
- Raw-NCA dump feature
- Changing the Phase B apply/remove/reboot flow — this milestone only feeds it base layouts

## Traceability
<!-- REQ-ID → Phase. Every v0.4 requirement maps to exactly one phase. -->

| Requirement | Phase | Status |
|-------------|-------|--------|
| EXTRACT-04 | Phase 1 | Pending |
| TAKEOVER-01 | Phase 1 | Pending |
| TAKEOVER-02 | Phase 1 | Pending |
| EXTRACT-01 | Phase 2 | Pending |
| EXTRACT-02 | Phase 2 | Pending |
| EXTRACT-03 | Phase 2 | Pending |
| INTEG-01 | Phase 3 | Pending |
| INTEG-02 | Phase 3 | Pending |
| INTEG-03 | Phase 3 | Pending |
| INTEG-04 | Phase 3 | Pending |
| INTEG-05 | Phase 3 | Pending |
| TAKEOVER-03 *(optional)* | Phase 4 | Pending |

**Coverage:** 12/12 v0.4 requirements mapped (11 core + 1 optional). No orphans, no duplicates.
