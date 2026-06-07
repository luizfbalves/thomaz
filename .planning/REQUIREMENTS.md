# Requirements: thomaz — v1.2 Game Management

**Defined:** 2026-06-06
**Core Value:** The user links their own content source and installs/manages games and DLC from inside the hub with a smoother, cover-art-rich, resumable flow than stock installers — thomaz being purely the client, never a content host.

## v1.2 Requirements

Requirements for the Game Management milestone. Each maps to exactly one roadmap phase.

### Content Source & Linking (SRC)

- [x] **SRC-01**: User can link a content server by entering a URL that returns a Tinfoil-style JSON index (`files[{url,size}]`, `directories`)
- [x] **SRC-02**: User can link a server that requires authentication (basic-auth-in-URL, custom header, or referrer gate)
- [x] **SRC-03**: User can browse and install from local NSP/NSZ files on the SD card
- [x] **SRC-04**: No content server is bundled or enabled by default — the source list is empty until the user adds one (responsibility/policy boundary)

### Catalog Browse (CAT)

- [x] **CAT-01**: User can browse a linked server's catalog as a grid/list showing cover art, title name, and file size, reusing the existing titledb/icon UI
- [x] **CAT-02**: Each catalog entry shows its kind (base / update / DLC) derived from the 64-bit title ID, and the user can filter/search the catalog

### Install (INST)

- [ ] **INST-01**: User can install a base title (NSP) from a linked server or a local file
- [ ] **INST-02**: User can install updates and DLC for a title
- [ ] **INST-03**: Installs stream content directly into NCM storage without buffering whole files in RAM, choose NAND or SD destination, and run a free-space pre-flight check before starting
- [ ] **INST-04**: An interrupted or failed install rolls back cleanly, and orphaned placeholders / partial content-meta are reconciled at app startup so the system content database is never left corrupt
- [ ] **INST-05**: User can install NSZ (compressed) packages, decompressed and re-encrypted on-device

### Installed Title Management (MGMT)

- [ ] **MGMT-01**: User can view installed titles with version, installed DLC, and on-disk size
- [ ] **MGMT-02**: User can see free space on NAND and SD card
- [ ] **MGMT-03**: User can uninstall an installed title's base, update, and/or DLC, freeing space

### Download / Install Queue (QUEUE)

- [ ] **QUEUE-01**: Downloads and installs run through a queue with per-item progress and a cancel action, processed off the UI thread by an app-scoped runner
- [ ] **QUEUE-02**: An interrupted download/install resumes after an app restart from a persisted on-SD journal (HTTP Range based); a user-cancelled item is discarded, not resumed

### Update / DLC Detection (UPD)

- [ ] **UPD-01**: For installed titles, the app auto-detects and surfaces available updates and DLC present on the linked server

### Cloud Sync (SYNC)

- [x] **SYNC-01**: The user's server-link configuration syncs one-tap to their thomaz cloud account (config only — never content), reusing the existing JWT auth and the cloud-saves sync pattern
- [x] **SYNC-02**: Synced server credentials are protected at rest and scoped to the owning account

## Future Requirements

Deferred to a later milestone. Tracked but not in the v1.2 roadmap.

### Game Management (later)

- **GAME-F01**: Install from XCI/XCZ (gamecard image) sources
- **GAME-F02**: Install from USB mass-storage / USB-HDD (`usbHsFs`)
- **GAME-F03**: Dump an inserted gamecard to a file
- **GAME-F04**: Installable Home-menu forwarder for one-tap launch of managed content

## Out of Scope

Explicitly excluded for v1.2. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Bundling a default content server / index / shop URL | thomaz is a client only; shipping a source would change its legal posture. Source list starts empty (SRC-04) |
| Hosting, proxying, or re-sharing content via the thomaz cloud API | The API stores only server-link config, never content (SYNC-01); content hosting is an explicit anti-pattern |
| Bundling Nintendo keys / `prod.keys` | NSP install needs no keys; NSZ carries its own per-section keys. No key material is shipped |
| XCI / XCZ install | Higher complexity (partition conversion); research recommends deferring → Future (GAME-F01) |
| USB-HDD mass-storage source | Needs `usbHsFs` and mount semantics; deferred → Future (GAME-F02). SD/local files cover the local case for v1.2 |
| Official Nintendo CDN downloads with user credentials | Out of the user-linked-server model chosen for this milestone |
| Signature-patch / sigpatch management | Orthogonal CFW concern; thomaz does not manage Atmosphère patches |

## Traceability

Which phases cover which requirements. Phase numbering continues from v1.1 (phases 5-7); v1.2 starts at Phase 8.

| Requirement | Phase | Status |
|-------------|-------|--------|
| SRC-01 | Phase 8 | Complete |
| SRC-02 | Phase 8 | Complete |
| SRC-03 | Phase 8 | Complete |
| SRC-04 | Phase 8 | Complete |
| CAT-01 | Phase 8 | Complete |
| CAT-02 | Phase 8 | Complete |
| INST-01 | Phase 10 | Pending |
| INST-02 | Phase 10 | Pending |
| INST-03 | Phase 10 | Pending |
| INST-04 | Phase 10 | Pending |
| INST-05 | Phase 10 | Pending |
| MGMT-01 | Phase 11 | Pending |
| MGMT-02 | Phase 11 | Pending |
| MGMT-03 | Phase 11 | Pending |
| QUEUE-01 | Phase 9 | Pending |
| QUEUE-02 | Phase 9 | Pending |
| UPD-01 | Phase 11 | Pending |
| SYNC-01 | Phase 8 | Complete |
| SYNC-02 | Phase 8 | Complete |

**Coverage:**

- v1.2 requirements: 19 total
- Mapped to phases: 19 ✓ (Phase 8: 8 · Phase 9: 2 · Phase 10: 5 · Phase 11: 4)
- Unmapped: 0 ✓

---
*Requirements defined: 2026-06-06*
*Last updated: 2026-06-06 — roadmap revised (NSZ merged into Phase 10; old Phase 12 → Phase 11; phases 8-11 mapped)*
