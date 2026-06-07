# Roadmap: thomaz

## Milestones

- ✅ **v1.0 Hardening** — Phases 1-4 (shipped 2026-06-05)
- 🟡 **v0.5.0 Theme Extraction** — Phases 1-2 shipped 2026-06-05; Phases 3-4 open
- ✅ **v1.1 Switch-Only Simplification** — Phases 5-7 (shipped 2026-06-06)
- 🟢 **v1.2 Game Management** — Phases 8-11 (active, started 2026-06-06)

## Phases

<details>
<summary>✅ v1.0 Hardening (Phases 1-4) — SHIPPED 2026-06-05</summary>

Resolved every issue surfaced by the codebase audit (`CONCERNS.md`) without adding features — community-feature removal, live-API security hardening with regression tests, isolated C++ platform fixes, and the C++ activity refactor. Verified by host tests (Vitest + doctest) and a clean desktop build (`-DUSE_SDL2=ON`). Full detail: [milestones/v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md).

- [x] Phase 1: Remove Community Feature (3/3 plans) — completed 2026-06-04
- [x] Phase 2: API Security + Regression Tests (3/3 plans) — completed 2026-06-05
- [x] Phase 3: C++ Platform Hardening (4/4 plans) — completed 2026-06-05
- [x] Phase 4: C++ Activity Hardening (6/6 plans) — completed 2026-06-05

**Deferred at close (hardware-only, non-gating):**

- Phase 03 TLS-insecure banner visual render (requires forced latch on real hardware)
- Phase 03 `save_service_switch.cpp` Switch-toolchain compile check (devkitPro only)
- Phase 04 5 UAT crash-path scenarios — activity-pop / dialog-button UAF probes (require a physical Switch)

</details>

<details>
<summary>🟡 v0.5.0 Theme Extraction (Phases 1-2) — SHIPPED 2026-06-05 (partial)</summary>

On-device extraction of the firmware home-menu base `.szs` layouts into `/themes/systemData/`, keyless to the user (ported exelix BIS+SPL+hactool, GPLv2). Released as app version 0.5.0. Verified by host doctests and a clean desktop build; on-hardware run deferred. Full detail: [milestones/v0.5.0-ROADMAP.md](milestones/v0.5.0-ROADMAP.md).

- [x] Phase 1: Privileged Extraction Spike (5/5 plans) — completed 2026-06-05
- [x] Phase 2: Full Extraction Engine (4/4 plans) — completed 2026-06-05

**Carried forward (planned, not built):**

- [ ] Phase 3: Theme UI Integration (0/3 plans) — INTEG-01..05
- [ ] Phase 4: Forwarder, optional (0/1 plans) — TAKEOVER-03

</details>

<details>
<summary>✅ v1.1 Switch-Only Simplification (Phases 5-7) — SHIPPED 2026-06-06</summary>

Removed the desktop (PC/SDL2/GLFW) build target entirely so the source tree targets only Nintendo Switch, cutting the platform-abstraction surface that existed solely to run the GUI on PC — with **zero change to the shipped `.nro`**. Sequenced for safety: collapsed the source selection seams first, then stripped the build system, then cleaned docs and ran the final combined gate. Verified by a green host doctest suite (209/209) and a clean Switch build (`build_switch/thomaz.nro`, 7.7 MB), redeployed and confirmed on real hardware. Full detail: [milestones/v1.1-ROADMAP.md](milestones/v1.1-ROADMAP.md).

- [x] Phase 5: Collapse Source Seams to Switch-Only (4/4 plans) — completed 2026-06-06
- [x] Phase 6: Strip Desktop from Build System (2/2 plans) — completed 2026-06-06
- [x] Phase 7: Docs Cleanup & Final Verification Gate (2/2 plans) — completed 2026-06-06

</details>

### 🟢 v1.2 Game Management (Phases 8-11) — ACTIVE

A Tinfoil-style on-device game-management client: link a user-owned content server (or local SD), browse a cover-art catalog, and install/update/uninstall base titles + DLC through the device's own ncm/ns/es content-management APIs — thomaz hosting no content, the cloud account syncing server config only.

**Core value:** The user links their own content source and installs/manages games and DLC from inside the hub with a smoother, cover-art-rich, resumable flow than stock installers — thomaz being purely the client, never a content host.

**Granularity:** coarse · **Coverage:** 19/19 v1.2 requirements mapped (SRC, CAT, INST, MGMT, QUEUE, UPD, SYNC) — no orphans, no duplicates.

**Sequencing principle** — host-tested logic first so any hardware failure is an install-mechanism bug, not a logic bug:

```
8 (catalog/sources/browse/sync) → 9 (queue + install-decision core) → 10 (NSP + NSZ install engine, HARDWARE)
                                                                          └→ 11 (installed mgmt + uninstall + update/DLC + queue UI, HARDWARE)
```

All decidable logic (index parse, title-ID/kind derivation, PFS0 parse, install planner, queue state machine, update/DLC diff, server-link codec) lives in pure `core/games/` under the doctest gate; `platform/games/` stays thin Switch orchestration; the cloud API gains exactly one model + one route — **config only, never content**.

- [ ] **Phase 8: Catalog, Content Sources & Server Linking** — Link a user-owned server (or local SD), parse its Tinfoil index, browse a cover-art catalog, and one-tap sync the server config to the cloud account — zero install risk
- [ ] **Phase 9: Install-Decision Core & Resumable Queue** — Host-tested PFS0 parse + install planner + queue state machine, plus an app-scoped runner with an on-SD journal that resumes downloads across restarts — before any NCM write
- [ ] **Phase 10: NSP & NSZ Install Engine (HARDWARE)** — Stream base/update/DLC NSP and NSZ installs into NCM with destination choice, free-space pre-flight, transactional rollback, startup reconciliation, gated ticket import, and serialized DB mutation; NSZ adds streaming NCZ decompression + on-device AES-CTR re-encryption within the applet-mode budget
- [ ] **Phase 11: Installed Management, Uninstall & Update/DLC Detection (HARDWARE)** — View installed titles with version/DLC/size + NAND/SD free space, uninstall base/update/DLC, auto-detect available updates/DLC, and watch the live queue with cancel/resume

## Phase Details

### Phase 8: Catalog, Content Sources & Server Linking

**Goal**: A user can link their own content server (or pick local SD files), see its catalog rendered as a cover-art grid with base/update/DLC kinds, and sync that server configuration one-tap to their thomaz account — with no content ever touching the cloud API and no source enabled by default.
**Depends on**: Nothing (first phase of the milestone; builds on existing `http_client_curl`, title/icon UI, and the cloud-saves auth pattern).
**Requirements**: SRC-01, SRC-02, SRC-03, SRC-04, CAT-01, CAT-02, SYNC-01, SYNC-02
**Success Criteria** (what must be TRUE):

  1. With no source configured, the source list is empty — nothing is bundled or enabled by default; the user must add a server before any catalog appears.
  2. A user can add a server URL returning a Tinfoil-style JSON index (`files[{url,size}]`, `directories`), including a server requiring auth (basic-auth-in-URL, custom header, or referrer gate), and see its catalog as a grid/list with cover art, title name, and size.
  3. Each catalog entry shows its kind (base / update / DLC) derived from the 64-bit title ID, and the user can filter/search the catalog.
  4. A user can browse and select local NSP/NSZ files on the SD card through the same catalog/detail surface.
  5. A user can tap once to sync their server-link configuration to their thomaz cloud account and see it restored on another console signed into the same account; the synced record is owner-scoped, credentials are protected at rest, and the API stores config only — never catalog or content bytes.

**Plans**: 6 plans
Plans:
**Wave 1**

- [x] 08-01-PLAN.md — Pure core/games logic: tolerant Tinfoil index parse, title-ID kind/grouping, catalog view transforms, recurse bounds, config-only sync codec (+ doctests)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 08-02-PLAN.md — Streaming/Range HTTP seam + platform index fetch (redirect-safe), empty-by-default source store, per-source SD cache
- [ ] 08-03-PLAN.md — Cloud config sync: owner-scoped SourceLink model + JWT config-only /sources route (encrypted at rest) + device sync client

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 08-04-PLAN.md — Shared UI scaffolding: catalog/sources i18n (both locales), tile_games token, Home Games card, 3-tier cover-art service

**Wave 4** *(blocked on Wave 3 completion)*

- [ ] 08-05-PLAN.md — Catalog grid/list + detail Activities (cache-first paint, search/sort/filter, kind chips, no install)

**Wave 5** *(blocked on Wave 4 completion)*

- [ ] 08-06-PLAN.md — Source-list Activity: empty default, add auth-gated remote + local SD peer, one-tap sync, redacted credentials

**UI hint**: yes
**Research flag**: Tinfoil index schema is community-reverse-engineered, not formally specified — point the parser at a real user server early; treat the encrypted-index variant as out of scope for MVP. Validate `Accept-Ranges: bytes` and the streaming/Range fallback on the target server (the streaming seam established here is consumed by Phase 10). Keep fail-closed TLS; strip `Authorization` on cross-host redirects.

### Phase 9: Install-Decision Core & Resumable Queue

**Goal**: Every install decision and the durable download/install queue exist and are proven on the host before any hardware write: PFS0/NSP parsing, the install plan (which NCAs, NAND-vs-SD target, free-space requirement, install order, CNMT meta-type), and a journaled, app-scoped queue runner that resumes interrupted downloads across a full app restart.
**Depends on**: Phase 8 (content sources + catalog feed the queue and the planner).
**Requirements**: QUEUE-01, QUEUE-02
**Success Criteria** (what must be TRUE):

  1. Host doctests parse PFS0/NSP headers into an entry table and produce a correct `InstallPlan` (NCA set, storage target, free-space need, ordering, meta-type) against fixtures — no hardware required.
  2. A user can enqueue a download/install job; it runs off the UI thread through an app-scoped runner with per-item progress and a cancel action, and navigating away from the queue does not stop it.
  3. An interrupted download resumes after a full app restart from a persisted on-SD journal (per-content_id offset + placeholder_id, HTTP Range based); a user-cancelled item is discarded, not resumed.
  4. The queue serializes work (one job in-flight) and the journal schema already carries the fields the install engine will checkpoint, so Phase 10 builds on it without redesign.

**Plans**: TBD
**Research flag**: The persistence schema (per-content_id offset + placeholder_id + target storage) must be designed here so resume builds on top in Phase 10 without redesign. The runner must NOT borrow a `ThomazActivity`/`runAsync` `alive`/`cancelled` lifetime — it owns its own cancelled flag at app scope.

### Phase 10: NSP & NSZ Install Engine (HARDWARE)

**Goal**: A user can install a base title, update, or DLC — packaged as either a plain NSP or a compressed NSZ — from a linked server or a local file: content streams directly into NCM storage by offset (never buffered whole in RAM), the user chooses NAND or SD with a free-space pre-flight check, an interrupted or failed install rolls back cleanly with orphaned placeholders/partial meta reconciled at startup, tickets are imported only when the NCA's rights_id requires one, and NSZ packages are stream-decompressed and re-encrypted on-device into the same placeholder write loop within the applet-mode memory budget.
**Depends on**: Phase 9 (queue runner + journal + PFS0 parse + install planner) and Phase 8 (streaming content source).
**Requirements**: INST-01, INST-02, INST-03, INST-04, INST-05
**Success Criteria** (what must be TRUE):

  1. A user can install a base title (NSP) end-to-end from either a linked server or a local SD file, and the installed title appears and launches on the HOME menu.
  2. A user can install updates and DLC for a title through the same pipeline, with prerequisite/version and `RequiredSystemVersion` checks surfaced rather than producing un-launchable content.
  3. Installs stream into the NCM placeholder by offset with flat (non-size-proportional) peak RAM, let the user choose NAND or SD, and refuse early with a clear message when free space is insufficient.
  4. Interrupting or failing an install at any step leaves no orphaned placeholders, no registered-but-uncommitted NCAs, and an uncommitted content-meta DB; startup reconciliation (`CleanupAllPlaceHolder` + content/meta reconcile) clears any residue, and DB-mutating ops are serialized (install mutually exclusive with itself and uninstall).
  5. A title whose NCAs carry a non-zero rights_id imports its ticket+cert (common ticket preferred; personalized surfaced, not blindly imported); a no-ticket title installs without a spurious ticket.
  6. A user can install a solid-compressed NSZ package and the resulting title launches correctly (re-encryption is byte-correct — no silent corruption surfacing only at launch); NSZ decompression streams with flat peak RAM (not proportional to decompressed section size), the zstd context is freed deterministically so installs succeed even in applet mode, and the NCZ structure boundary (first 0x4000 bytes stored uncompressed, the zstd stream after the header) is handled correctly so the NCA is not corrupted.

**Plans**: TBD
**UI hint**: yes
**Research flag**: HIGHEST-RISK, hardware-gated phase. Validate the NCM content-meta struct layout on the actual target firmware (layout has grown across firmware versions — do not hardcode sizes where libnx provides them). Verify the exact ncm/ns/es call sequence and interrupt-at-each-step rollback. Rollback + startup reconciliation + serialization + gated ticket import MUST ship in this phase — they are NOT a later hardening phase. **NSZ sub-gate (merged in, NOT dissolved):** NSZ carries higher technical uncertainty than plain NSP and the launch-only-corruption risk remains even though the user accepted the merge — it still needs its OWN dedicated spike and a distinct on-hardware validation sub-gate within this phase. NCZ format boundaries (0x4000 offset, per-section AES-CTR keys from the NCZ header), solid-vs-block streaming, applet-mode memory; reference Awoo `nca_writer` + the nsz spec; validate re-encryption against reference fixtures because corruption only fails at game launch. Do NOT collapse the NSZ validation into the NSP happy path — gate it separately.

### Phase 11: Installed Management, Uninstall & Update/DLC Detection (HARDWARE)

**Goal**: A user can see what is installed (version, owned DLC, on-disk size) and how much free space remains on NAND and SD, uninstall a title's base/update/DLC to reclaim space, have available updates and DLC on the linked server auto-surfaced for installed titles, and watch the live download/install queue with cancel and resume.
**Depends on**: Phase 10 (shares the ncm/ns lifecycle; the queue runner and catalog already exist). Update/DLC diff also draws on the Phase 8 catalog.
**Requirements**: MGMT-01, MGMT-02, MGMT-03, UPD-01
**Success Criteria** (what must be TRUE):

  1. A user can view installed titles with version, installed DLC, and on-disk size, and see free space on both NAND and SD card.
  2. A user can uninstall an installed title's base, update, and/or DLC and observe the freed space reflected.
  3. For installed titles, the app auto-detects and surfaces available updates and DLC present on the linked server (update-available / N-new-DLC badges).
  4. A user can open the queue screen to watch per-item progress, cancel a discardable item, and see an interrupted item resume — without the queue screen owning the install's lifetime.

**Plans**: TBD
**UI hint**: yes
**Research flag**: Granular update/DLC uninstall is materially trickier than full-title `nsDeleteApplicationCompletely` — it needs application-record merge/trim logic; flag for phase-level research during planning.

## Coverage Map (v1.2)

| Requirement | Phase |
|-------------|-------|
| SRC-01 | Phase 8 |
| SRC-02 | Phase 8 |
| SRC-03 | Phase 8 |
| SRC-04 | Phase 8 |
| CAT-01 | Phase 8 |
| CAT-02 | Phase 8 |
| SYNC-01 | Phase 8 |
| SYNC-02 | Phase 8 |
| QUEUE-01 | Phase 9 |
| QUEUE-02 | Phase 9 |
| INST-01 | Phase 10 |
| INST-02 | Phase 10 |
| INST-03 | Phase 10 |
| INST-04 | Phase 10 |
| INST-05 | Phase 10 |
| MGMT-01 | Phase 11 |
| MGMT-02 | Phase 11 |
| MGMT-03 | Phase 11 |
| UPD-01 | Phase 11 |

**Total:** 19/19 mapped. No orphans. No duplicates. (Phase 8: 8 · Phase 9: 2 · Phase 10: 5 · Phase 11: 4)

## Progress

| Phase | Milestone | Plans Complete | Status   | Completed  |
|-------|-----------|----------------|----------|------------|
| 1. Remove Community Feature        | v1.0 | 3/3 | Complete | 2026-06-04 |
| 2. API Security + Regression Tests | v1.0 | 3/3 | Complete | 2026-06-05 |
| 3. C++ Platform Hardening          | v1.0 | 4/4 | Complete | 2026-06-05 |
| 4. C++ Activity Hardening          | v1.0 | 6/6 | Complete | 2026-06-05 |
| 1. Privileged Extraction Spike     | v0.5.0 | 5/5 | Complete | 2026-06-05 |
| 2. Full Extraction Engine          | v0.5.0 | 4/4 | Complete | 2026-06-05 |
| 3. Theme UI Integration            | v0.5.0 | 0/3 | Carried forward | - |
| 4. Forwarder (Optional)            | v0.5.0 | 0/1 | Carried forward | - |
| 5. Collapse Source Seams to Switch-Only | v1.1 | 4/4 | Complete   | 2026-06-06 |
| 6. Strip Desktop from Build System      | v1.1 | 2/2 | Complete   | 2026-06-06 |
| 7. Docs Cleanup & Final Verification Gate | v1.1 | 2/2 | Complete   | 2026-06-06 |
| 8. Catalog, Content Sources & Server Linking | v1.2 | 1/6 | In Progress|  |
| 9. Install-Decision Core & Resumable Queue | v1.2 | 0/? | Not started | - |
| 10. NSP & NSZ Install Engine (HARDWARE) | v1.2 | 0/? | Not started | - |
| 11. Installed Mgmt, Uninstall & Update/DLC Detection (HARDWARE) | v1.2 | 0/? | Not started | - |
