---
gsd_state_version: 1.0
milestone: v1.2
milestone_name: Game Management
status: phase_complete
stopped_at: Completed 08-06-PLAN.md
last_updated: "2026-06-07T19:30:00.000Z"
last_activity: 2026-06-07 -- Completed 08-06-PLAN.md (Phase 8 complete)
progress:
  total_phases: 4
  completed_phases: 1
  total_plans: 6
  completed_plans: 6
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-06-06)

**Core value:** The user links their own content source and installs/manages games and DLC from inside the hub with a smoother, cover-art-rich, resumable flow than stock installers — thomaz being purely the client, never a content host.
**Current focus:** Phase 8 — catalog-content-sources-server-linking

## Current Position

Phase: 8 (catalog-content-sources-server-linking) — COMPLETE
Plan: 6 of 6 complete
Status: Phase gate UAT pending (nxlink)
Last activity: 2026-06-07 -- Completed 08-06-PLAN.md

## Milestone v1.2 Roadmap

Phases 8-11 (numbering continues from v1.1's 5-7). Coverage 19/19, granularity coarse.

| Phase | Goal (one line) | Requirements | Hardware |
|-------|-----------------|--------------|----------|
| 8. Catalog, Content Sources & Server Linking | Link a server/local SD, browse cover-art catalog, one-tap cloud-sync server config | SRC-01..04, CAT-01/02, SYNC-01/02 | No |
| 9. Install-Decision Core & Resumable Queue | Host-tested PFS0 parse + install planner + queue state machine + journaled app-scoped runner | QUEUE-01/02 | No |
| 10. NSP & NSZ Install Engine | Streamed NSP + NSZ base/update/DLC install: dest choice, free-space pre-flight, rollback, reconciliation, gated tickets, serialized DB; NSZ = streamed NCZ decompress + on-device AES-CTR re-encrypt, applet-mode bounded (own spike + sub-gate) | INST-01..05 | Yes (HIGH risk; NSZ research-gated sub-gate) |
| 11. Installed Mgmt, Uninstall & Update/DLC Detection | Installed list (version/DLC/size) + NAND/SD free space, uninstall, update/DLC badges, live queue UI | MGMT-01..03, UPD-01 | Yes |

**Spine:** 8 → 9 → 10 → 11. Host-tested logic (8, 9) precedes hardware (10, 11) so failures on hardware are install-mechanism bugs, not logic bugs.

## Performance Metrics

**Velocity:**

- Total plans completed: 10
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 3 | - | - |
| 02 | 3 | - | - |
| 03 | 4 | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
| Phase 01-remove-community-feature P02 | 308 | 2 tasks | 24 files |
| Phase 01-remove-community-feature P03 | 12 | 2 tasks | 3 files |
| Phase 02-api-security-regression-tests P01 | 15 | 2 tasks | 4 files |
| Phase 02-api-security-regression-tests P02 | 4 | 2 tasks | 4 files |
| Phase 02-api-security-regression-tests P03 | 5min | 2 tasks | 1 files |
| Phase 03-c-platform-hardening P04 | 5min | 1 tasks | 2 files |
| Phase 04-c-activity-hardening P01 | 3min | 3 tasks | 3 files |
| Phase 04-c-activity-hardening P05 | 3min | 1 tasks | 1 files |
| Phase 04-c-activity-hardening P02 | 20min | 2 tasks | 8 files |
| Phase 04-c-activity-hardening P04 | 25min | 2 tasks | 11 files |
| Phase 04-c-activity-hardening P06 | 10min | 4 tasks | 9 files |
| Phase 05-collapse-source-seams-switch-only P01 | 5min | 2 tasks | 2 files |
| Phase 05 P02 | 3min | 1 tasks | 9 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- v1.2 Roadmap (2026-06-06, revised): 4 coarse phases (8-11). Folded the research's 10-phase decomposition — host-testable core leaves and low-risk UI/sync leaves merged into their natural neighbors (catalog+sources+browse+server-sync → Phase 8; queue+PFS0+planner → Phase 9; installed mgmt+uninstall+update/DLC+queue UI → Phase 11). NSZ was merged into the NSP install engine (Phase 10) at user request, keeping its own spike + on-hardware validation sub-gate inside that phase.
- v1.2 Phase 10 (NSP & NSZ install engine) is the ONE genuinely HIGH-risk, hardware-gated phase. Per research, rollback + startup reconciliation + serialized DB mutation + free-space pre-flight + gated ticket import MUST ship in this phase — NOT split into a later hardening phase.
- v1.2 NSZ (INST-05) merged into Phase 10 (was a standalone Phase 11). The launch-only-corruption risk remains: NSZ keeps a dedicated spike and a distinct on-hardware validation sub-gate WITHIN Phase 10 — NCZ re-encryption corruption only surfaces at game launch, so its validation must NOT collapse into the NSP happy path.
- v1.2 sequencing: streaming content-source/Range seam established in Phase 8 BEFORE the install engine consumes it (the existing curl client buffers whole responses in a std::string and OOMs on multi-GB titles). The resumable queue (Phase 9) is an app-scoped runner with an on-SD journal, NOT tied to a ThomazActivity/runAsync lifetime.
- [Phase 08-02]: IHttpClient::stream() default stub (ok=false) keeps existing test doubles compiling; index fetch uses capped request(), content install will use stream() in Phase 10.
- [Phase 08-02]: Header-auth index fetch disables auto-follow and re-attaches custom header only on same-host redirects; BasicInUrl relies on libcurl default cross-host Authorization strip.
- [Phase 08-02]: source_store returns empty vector on missing sources.json (SRC-04); SD credentials plaintext accepted MVP limitation documented in source_store header.
- [Phase 08-03]: SourceLink + /sources route store config only (no blob field, no multipart); credentials AES-256-GCM at rest via SOURCE_ENC_KEY; HttpSourceSyncClient::push takes cloud id separately from SourceConfig.
- [Phase 08-04]: catalog/sources i18n namespace (20 keys, both locales); tile_games Home card; cover_art 3-tier (titledb stream-cache → libnx icon → placeholder); SourceListActivity header only until Plan 06.
- [Phase 08-05]: CatalogActivity cache-first grid/list + CatalogDetailActivity Base/Update/DLC rows; no install affordance; kind chips via text+color; core::apply_view for sort/filter/search.
- [Phase 08-06]: SourceListActivity empty-by-default + local://sd peer + one-tap sync; local_source bounded SD scan; remoteId for idempotent cloud PUT; credential-redacted row labels.
- v1.2 legal boundary: no default server/index/keys bundled (SRC-04); cloud API stores server CONFIG ONLY, never content (SYNC-01) — enforced in the Prisma model + route schema.

#### Prior milestone decisions (v1.1, retained for context)

- v1.1 Roadmap: 3 coarse phases — source-seam collapse first (Phase 5), build-system strip second (Phase 6), docs + final gate third (Phase 7); order enforces a buildable tree at every phase boundary
- v1.1 verification: host doctest suite (`tests/Makefile`) is the in-phase gate for the source layer (independent of the desktop build, survives); the Switch build (`build-switch.sh`, devkitPro Docker) is the post-removal compile gate — no non-devkitPro full-tree compile check remains
- v1.1 keep: `saves/fake_cloud_save_client.*` is a doctest test double, NOT a desktop GUI stub — explicitly retained and still compiled by the suite
- [Phase 05]: **Option D (scope decision, 2026-06-05):** path-helper `#ifdef __SWITCH__/#else` blocks are platform-PORTABILITY seams (Switch absolute path vs host-writable path), NOT `*_fake`-vs-`*_switch` stub-selection — RETAINED to keep the host doctest suite green.

### Pending Todos

None yet.

### Blockers/Concerns

Carried into v1.2 from research (PITFALLS.md / ARCHITECTURE.md):

- Phase 8: Tinfoil index schema is community-reverse-engineered — point the parser at a real server early; encrypted-index variant out of scope for MVP. Keep fail-closed TLS; strip `Authorization` on cross-host redirects; cap JSON parse size. Content bytes must NEVER go through `IHttpClient::request`/`HttpResponse.body` (OOM) — establish the streaming/Range seam here.
- Phase 9: Persistence schema (per-content_id offset + placeholder_id + target storage) must be designed here so Phase 10 resume builds on it without redesign. The runner must own its own app-scoped `cancelled` flag — NOT borrow an activity's `alive`/`cancelled`.
- Phase 10: HIGHEST RISK. NCM content-meta struct layout is firmware-version-sensitive — validate on target firmware, don't hardcode sizes libnx provides. Register all NCAs before meta `Set`/`Commit` (commit = the single linearization point); rollback (DeletePlaceHolder/Delete) on any failure; `CleanupAllPlaceHolder` + reconcile on startup. Gate ticket import on NCA rights_id (no spurious tickets; refuse cleanly on missing required ticket). Serialize via an `installBusy` atomic (mirror `cloudBusy`), install mutually exclusive with uninstall. Switch build = devkitPro Docker; hardware UAT required. **NSZ sub-gate (merged into this phase):** NCZ format boundary (first 0x4000 uncompressed, zstd stream after header), per-section AES-CTR keys from the NCZ header. Stream-decompress with a bounded zstd context + fixed output buffer; free the context deterministically (historical leak). Validate re-encryption against nsz reference fixtures — corruption only fails at launch. Keep the dedicated spike + a distinct on-hardware validation sub-gate; do NOT fold NSZ verification into the NSP happy path.
- Phase 11: Granular update/DLC uninstall (application-record merge/trim) is trickier than full-title `nsDeleteApplicationCompletely` — flag for phase-level research. Leaving the queue UI must NOT tear down an in-progress install.

#### Prior-milestone hardware UAT still open (non-gating, see Deferred Items)

- v1.0 Phase 03/04 hardware UAT (TLS banner render, save_service_switch compile, 5 activity-pop UAF scenarios) and v0.5.0 on-hardware extraction verification remain outstanding.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260605-ot7 | Theme-apply screen UX: busy-guard download/apply button against repeated clicks | 2026-06-05 | 4f9205b | [260605-ot7-melhorar-a-ux-da-tela-de-aplicacao-de-te](./quick/260605-ot7-melhorar-a-ux-da-tela-de-aplicacao-de-te/) |
| 260605-qgb | BootActivity boot screen: login/guest entry before HomeActivity when no session | 2026-06-05 | d668063 | [260605-qgb-vamos-adicionar-uma-bootscreen-no-app-us](./quick/260605-qgb-vamos-adicionar-uma-bootscreen-no-app-us/) |
| 260605-rcu | Hybrid title-visibility system: heuristic auto-hides forwarders (save=0 & acct=0) + per-title overrides + global toggle | 2026-06-05 | c816ffc | [260605-rcu-da-pra-ocultar-apps-que-nao-sao-jogos-da](./quick/260605-rcu-da-pra-ocultar-apps-que-nao-sao-jogos-da/) |
| 260605-tbt | Fix gamepad focus highlight stuck on previous screen: claimInitialFocus helper in ThomazActivity + applied in 5 async-list screens | 2026-06-06 | 604b564 | [260605-tbt-nos-menus-das-telas-quando-navegando-por](./quick/260605-tbt-nos-menus-das-telas-quando-navegando-por/) |
| 260605-sbk | Fix save backup failing on Switch: copy_tree now creates its full destination dir chain (ensure_parent_dirs) instead of a single non-recursive mkdir; verified on hardware | 2026-06-05 | 9e3d4ad | [260605-sbk-corrigir-save-backup-no-switch](./quick/260605-sbk-corrigir-save-backup-no-switch/) |
| 260606-og5 | Rewrite README to reflect the real multi-feature Switch hub (cheats + mods + temas + cloud saves), with all technical claims verified against the codebase | 2026-06-06 | 30f0330 | [260606-og5-vamos-melhorar-o-readme-pra-refletir-mel](./quick/260606-og5-vamos-melhorar-o-readme-pra-refletir-mel/) |

## Deferred Items

Items acknowledged and deferred at milestone close on 2026-06-05:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Performance | PERF-01: avoid double archive traversal | v2 backlog | Roadmap |
| Performance | PERF-02: cache CloudStatus for upload skip | v2 backlog | Roadmap |
| Hardware UAT | Phase 03: TLS-insecure red banner visual render across all activity screens (forced latch) | human_needed | v1.0 close |
| Hardware UAT | Phase 03: save_service_switch.cpp compiles clean under devkitPro Switch toolchain (IN-03 uid_from_hex) | human_needed | v1.0 close |
| Hardware UAT | Phase 04: 5 activity-pop / dialog-button UAF crash-path scenarios (settings, clear_cheats, mod_manager, theme_detail) | testing | v1.0 close |

Items acknowledged and deferred at v1.1 milestone close on 2026-06-06 (orthogonal UI quick-tasks, not v1.1 milestone work):

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Quick Task | 260605-ot7 (theme-apply UX busy-guard) | committed 4f9205b — missing SUMMARY/status marker | v1.1 close |
| Quick Task | 260605-qgb (bootscreen) | committed d668063 — missing SUMMARY/status marker | v1.1 close |
| Quick Task | 260605-rcu (hide non-game apps) | committed c816ffc — missing SUMMARY/status marker | v1.1 close |
| Quick Task | 260605-tbt (gamepad focus highlight fix) | committed 604b564 — missing SUMMARY/status marker | v1.1 close |
| Quick Task | 260605-s2h (navbar SD-card space) | concurrent session in-progress; resolve separately | v1.1 close |

All deferred UAT/verification items are on-hardware checks the host test suite cannot exercise; all automated gates (Vitest + doctest + clean `-DUSE_SDL2=ON` build) passed. Resume with `/gsd-verify-work 03` / `/gsd-verify-work 04` when a Switch is available.

## Session Continuity

Last session: 2026-06-07T18:30:00.000Z
Stopped at: Completed 08-06-PLAN.md
Resume file: None

## Operator Next Steps

- Run Phase 8 hardware UAT via nxlink (empty list → add auth server → browse → sync → remove)
- Plan Phase 9 with `/gsd-plan-phase 9`
