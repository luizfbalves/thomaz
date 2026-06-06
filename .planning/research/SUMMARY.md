# Project Research Summary

**Project:** thomaz - v1.2 Game Management milestone
**Domain:** On-device Nintendo Switch title install/update/DLC/uninstall client (Tinfoil-style; user links their own content server; thomaz hosts no content)
**Researched:** 2026-06-06
**Confidence:** HIGH

## Executive Summary

v1.2 adds a Tinfoil-style on-device title manager to thomaz: the user links their own HTTP/HTTPS content server (or browses local SD), the client reads a Tinfoil-style JSON index, and installs/updates/removes base titles, updates, and DLC through the device own content-management APIs. thomaz ships only the client - no bundled server, index, or keys, and the cloud account syncs server configuration only, never content. All four research tracks converged on the same headline: this milestone is overwhelmingly net-new code, not net-new dependencies. The install pipeline runs on libnx services (ncm/ns/es/fs) already in the toolchain, plus libzstd already on the link line (pulled transitively by libarchive). Two pillars of the feature - listing installed titles with versions/DLC/size, and showing NAND/SD free space - are already implemented in title_service_switch.cpp and system_status.cpp and only need extending.

The recommended approach is a clean-architecture extension of the existing tree: pure, host-doctest-covered decision logic in a new core/games/ (index parsing, PFS0/CNMT parse, install planning, the queue state machine, update/DLC diffing), with a thin platform/games/ doing only the IO that core planned (curl byte ranges, file reads, libnx ncm/ns calls, SD journal writes). The genuinely new, hardware-only, highest-risk component is the NCM content-DB install pipeline (CreatePlaceHolder to WritePlaceHolder to Register to ContentMetaDatabase.Set/Commit to nsPushApplicationRecord). It is non-atomic with no built-in transaction or rollback, so a bug here can corrupt the console installed-titles database, leave un-deletable titles, or leak space. Game vs update vs DLC is derived from the 64-bit title ID, not from any JSON field.

The risk profile is concentrated and well-understood. Five cross-cutting mandates emerged: (1) the install pipeline must ship rollback + startup reconciliation/CleanupAllPlaceHolder from day one - not as later hardening; (2) the resumable queue must be an app-scoped runner with an on-SD journal, NOT tied to a ThomazActivity/runAsync lifetime (an install must survive navigation and app restart); (3) content bytes must stream into the NCM placeholder - the existing HttpResponse.body std::string buffering will OOM on multi-GB titles, especially in applet mode (~440 MB); the HTTP path needs Range-based resume; (4) installs must be serialized (one in-flight, mutually exclusive with uninstall) because the content-meta imkvdb is not concurrency-safe; (5) legal posture stays clean only if defaults are empty and cloud sync carries server config only. NSZ (zstd-compressed) support is the one higher-uncertainty piece and is recommended as its own later phase with an on-hardware validation gate.

## Key Findings

### Recommended Stack

The stack story is almost entirely reuse what is already linked. Plain NSP install copies encrypted NCAs verbatim - it never decrypts - so it needs no prod.keys, no SPL, no hactool (the theme-extraction stack stays scoped to themes). The net-new work is code: a PFS0/NSP container parser, a CNMT reader, and an NCM placeholder-install state machine; plus, for NSZ, an NCZ section decoder over the already-linked libzstd (the per-section AES-CTR keys live inside the NCZ header, so re-encryption needs no external keys). See STACK.md for the verified libnx call sequence.

**Core technologies (all already in the toolchain):**
- libnx ncm - NCA content storage: create/write/register placeholders, content-meta DB set/commit - the only sanctioned on-device path to write NCAs; every homebrew installer uses exactly these calls.
- libnx ns - push application records (make a title/update/DLC visible), list installed records, free/total space, delete - already used by thomaz for listing and space.
- libnx es - import ticket + cert (esImportTicket) for titlekey-crypto titles - cheap to add; required for those titles to launch.
- libzstd - streaming NCZ decompression for NSZ install - already on the link line; only the <zstd.h> include + ZSTD_decompressStream usage is new.
- http_client_curl, nlohmann/json (lib/json) - reused directly for downloads and index parsing.

### Expected Features

The reference clients (Tinfoil, Awoo, DBI, Goldleaf) converge on one expected flow: add a source, browse a catalog, pick + install, manage what is installed. thomaz differentiation is purely UX. See FEATURES.md.

**Must have (table stakes):**
- Link a remote HTTP/HTTPS source with auth (basic-auth URL, custom header, referrer) - interop foundation
- Parse Tinfoil index JSON (files/directories/titledb/success/referrer/headers) - ecosystem compatibility
- Catalog browse (list first), install base title (NSP) end-to-end, install update + DLC (same pipeline)
- NSZ/XCZ decompression on install - modern packages are NSZ; NSP-only feels broken
- Install from local SD file; list installed with version/DLC/size (reuse); free space NAND+SD (reuse)
- Uninstall base/update/DLC; per-install progress + cancel + NCA sanity warning

**Should have (competitive differentiators):**
- Cover-art/metadata browse grid - reuse existing game grid + image_transcode
- Resumable download/install queue with progress + cancel - the biggest UX gap in Tinfoil/Awoo
- Auto-detect available updates/DLC for installed titles (pure core diff logic)
- One-tap server linking synced to the cloud account (config only)

**Defer (v2+):**
- mTLS client-cert auth, directories recursion / nested sub-shops, resume-across-restart polish, USB-HDD source, Tinfoil encrypted index, full XCI gamecard path, cloud-protocol zoo (gdrive/1fichier/MTP). Anti-features (never): bundling a default/curated source, hosting/re-sharing content, key bundling/derivation, parallel installs, auto-install without consent.

### Architecture Approach

A subsequent-milestone integration, not greenfield: reuse the existing layering (core/ pure + host-tested, platform/ thin Switch orchestration, Borealis *Activity on ThomazActivity/runAsync, Fastify+Prisma API) as-is. New per-domain folders core/games/ and platform/games/ plus four new activities. The largest de-risking lever is pushing all decidable logic (index parse, PFS0 layout, install plan, queue state machine, update/DLC diff) into core/games/ so the doctest gate covers it, leaving nsp_installer_switch.cpp a thin, auditable hardware orchestration. See ARCHITECTURE.md.

**Major components:**
1. core/games/ (NEW, pure/host-tested) - index_parser, catalog_model, filename_tags, pfs0_parse, install_planner, update_dlc_diff, queue_state, server_link.
2. platform/games/ (NEW, thin IO) - IContentSource (remote curl+Range / local file), catalog_fetch, nsp_installer_switch, installed_query_switch (extends title_service), install_queue_runner (app-scope), queue_journal, server_link_store.
3. App activities (NEW x4 + server-link) - store/browse, catalog detail, installed manager, download queue.
4. Cloud API (MODIFIED) - one ContentServer Prisma model + one /content-servers route, mirroring the cloud-saves auth flow, config only - never content.

Key patterns: two content sources behind one IContentSource interface (the single seam that makes remote and local install one pipeline); the chunked WritePlaceHolder loop as the single cancel + resume + journal-checkpoint point; an app-scoped queue runner with an SD journal as durable truth and activities as alive-guarded views.

### Critical Pitfalls

1. **Partial install corrupting the NCM content-meta database** - the sequence is non-atomic and has no built-in rollback. Register all NCAs before Set/Commit, treat the meta Commit as the single linearization point, roll back placeholders/NCAs on any failure, and run CleanupAllPlaceHolder + reconciliation on startup. Must ship with the first NCM write, not later.
2. **Queue tied to an activity lifetime / no resume** - runAsync/alive/cancelled is for short activity-scoped work; tearing it down mid-install closes ncm/ns handles in use (UAF) and abandons the transaction. The engine owns its own cancelled flag and handles, app-scoped, with per-content_id offset + placeholder_id persisted to SD so resume survives a full restart.
3. **In-memory buffering of multi-GB downloads (OOM)** - content bytes must NEVER go through IHttpClient::request/HttpResponse.body; stream straight into the NCM placeholder by offset. Use the JSON client only for index/CNMT/thumbnails. Bound every in-memory buffer.
4. **Concurrency / DB corruption** - the imkvdb content-meta DB is not concurrency-safe. Serialize all DB-mutating ops (one install at a time, install mutually exclusive with uninstall), mirroring the existing cloudBusy atomic gate.
5. **Ticket handling + applet-mode RAM + NAND/SD free-space + TLS/auth + legal** - gate ticket import on NCA rights_id (do not hard-require or silently skip); keep peak RAM flat in applet mode (~440 MB); pre-flight free-space + firmware checks and handle the FAT32 4 GB split-file trap; keep fail-closed TLS and strip Authorization on cross-host redirects; ship no default server/index/keys and sync config only.

## Implications for Roadmap

Based on research, the dependency-ordered build spine is 1 -> 2 -> {4, 5} -> 6 -> 7, with 3, 8, 9, 10 as parallelizable UI/sync leaves. Hardware risk (the NCM install engine) is deliberately placed after the host-tested foundations so failures are install-mechanism bugs, not logic bugs.

### Phase 1: Catalog + index parsing (core)
**Rationale:** Fully host-testable, depends on nothing, unblocks browse. Title ID / kind derivation lives here.
**Delivers:** catalog_model, index_parser, filename_tags with doctest coverage.
**Addresses:** Parse Tinfoil index; derive game/update/DLC from the 64-bit title ID (not a JSON field).
**Avoids:** Unbounded JSON parse (size caps, type validation).

### Phase 2: Content sources + catalog fetch (platform)
**Rationale:** First on-device fetch of a real user index; establishes the streaming seam before the install engine consumes it.
**Delivers:** IContentSource (remote curl+Range / local file), catalog_fetch.
**Uses:** Existing http_client_curl (+ Range, + auth headers).
**Avoids:** OOM buffering (streaming sink from day one); TLS/auth leakage on redirects.

### Phase 3: Store + catalog browse UI (app) - leaf, parallelizable
**Rationale:** Catalog visible on hardware with zero install risk - early validation. Cover-art grid reuses the existing game grid + image_transcode.
**Delivers:** GameStoreActivity (cover grid) + CatalogDetailActivity (read-only first).

### Phase 4: Queue state machine + journal (core + platform)
**Rationale:** Validates resume/persistence with download-only jobs before touching ncm. The persistence schema (per-content_id offset + placeholder_id) must be designed here so resume builds on top without redesign.
**Delivers:** queue_state (pure, doctest), queue_journal, InstallQueueRunner skeleton wired into main() (app scope).
**Avoids:** Queue tied to activity lifetime; lost-on-restart resume.

### Phase 5: PFS0 parse + install planner (core)
**Rationale:** De-risks install decision logic on the host before any hardware code (storage target, free-space need, install order, CNMT meta-type).
**Delivers:** pfs0_parse, install_planner with doctest fixtures.
**Avoids:** NAND/SD destination + free-space + version-prerequisite mistakes (decided here).

### Phase 6: NSP install engine (platform, HARDWARE) - highest risk
**Rationale:** The one genuinely HIGH-risk, hardware-gated, non-atomic piece. Driven by 4+5; isolated and thin by design.
**Delivers:** nsp_installer_switch - chunked, cancelable, journaled WritePlaceHolder loop; rollback; startup reconciliation; ticket import gated on rights_id; serialized (installBusy).
**Avoids:** Content-DB corruption (Pitfall 1), concurrency UAF (Pitfall 8), silent-ticket-skip (Pitfall 3) - all must ship in this phase.

### Phase 7: Installed query + uninstall + free space (platform/app, HARDWARE)
**Rationale:** Shares ncm/ns lifecycle with the install engine. Extends already-built listing/free-space.
**Delivers:** installed_query_switch, InstalledManagerActivity, uninstall (base/update/DLC) via ns.

### Phase 8: Update/DLC diff + badges (core + UI) - leaf
**Rationale:** Pure comparison of installed (Phase 7) vs catalog (Phase 1).
**Delivers:** update_dlc_diff surfaced as update-available / N-new-DLC badges.

### Phase 9: Download queue UI (app) - leaf
**Rationale:** Subscribes to the app-scoped runner (Phase 4) once install (Phase 6) works.
**Delivers:** DownloadQueueActivity progress/cancel/resume; separate cancel-discard from pause/leave-resume-later.

### Phase 10: Server-link config + one-tap cloud sync (core + platform + API) - leaf, can start after Phase 1
**Rationale:** Mirrors the existing cloud-saves auth flow; shares no hardware code.
**Delivers:** server_link model/codec, server_link_store, ServerLinkActivity, ContentServer Prisma model + /content-servers route. Config only - no content.
**Avoids:** Legal drift (Pitfall 11) - empty defaults, config-only sync, no proxy endpoint.

### NSZ Support: separate, higher-uncertainty phase (recommended after Phase 6)
**Rationale:** NCZ re-encryption bugs produce silently-corrupt installs that only fail at game launch; solid compression must stream (no random access) within the applet-mode budget. Add the NCZ section decoder over the already-linked libzstd + libnx aes128Ctr*.
**Flag:** Needs a dedicated spike + on-hardware validation gate.

### Phase Ordering Rationale
- **Dependencies:** 1 -> 2 -> {4,5} -> 6 is the critical spine; everything host-testable comes before hardware so a failure on hardware is an install-mechanism bug, not a logic bug.
- **Architecture grouping:** core decision logic clusters in early phases (1, 4, 5) under the doctest gate; thin hardware orchestration (6, 7) follows; UI/sync leaves (3, 8, 9, 10) parallelize.
- **Pitfall avoidance:** rollback + reconciliation + serialization are forced into Phase 6 (not deferred); the streaming/OOM contract is set in Phase 2; the app-scoped queue + journal schema in Phase 4; legal/config-only in Phase 10.

### Research Flags
Phases likely needing deeper research during planning:
- **NSZ Support phase:** highest technical uncertainty - NCZ format boundaries (0x4000 offset, per-section keys), solid-vs-block streaming, applet-mode memory; reference Awoo nca_writer + nsz spec; on-hardware gate.
- **Phase 6 (NSP install engine):** validate NCM content-meta struct layout on the actual target firmware (layout has grown across firmware); interrupt-at-each-step rollback verification.
- **Phase 2 (index schema):** point the parser at a real user server early - the Tinfoil format is community-reverse-engineered, not formally specified (encrypted-index variant out of scope for MVP).
- **Phase 7 (granular update/DLC uninstall):** materially trickier than full-title nsDeleteApplicationCompletely; needs application-record merge/trim logic.

Phases with standard patterns (lighter research):
- **Phase 1, 4, 5 (core logic):** pure parsing/state-machine/diff with doctest - established in-repo pattern (themezer_json, save_sync_state).
- **Phase 10 (cloud sync):** directly mirrors the existing /saves JWT auth + optimistic-locking sync.
- **Phase 3, 8, 9 (UI):** reuse existing Borealis grid/activity substrate.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | libnx ncm/ns/es calls verified against current libnx + Awoo/Tinfoil source; zstd already linked. MEDIUM-HIGH for NCZ specifics. |
| Features | HIGH | Tinfoil index format + installer behaviors from official docs and primary sources; existing-infra fit confirmed by direct code read. |
| Architecture | HIGH | Existing patterns read directly from source; install pipeline verified against libnx headers + Goldleaf reference. |
| Pitfalls | HIGH | NCM/install-sequence + codebase-integration from switchbrew + Awoo/Tinfoil + direct code read. MEDIUM for NSZ memory specifics + applet thresholds. |

**Overall confidence:** HIGH

### Gaps to Address
- **NCM content-meta struct layout vs firmware:** firmware-version-sensitive; do not hardcode struct sizes where libnx provides them; validate on target firmware (Phase 6 hardware gate).
- **Tinfoil index schema variants:** exact files/directories/encrypted-index shapes are community-derived; spike against a real server in Phase 2; treat encrypted index as out of scope unless a target server requires it.
- **NSZ/NCZ decoder correctness:** silently-corrupt installs only surface at launch; needs reference fixtures (nsz tool) + on-hardware validation in the dedicated NSZ phase.
- **Granular update/DLC uninstall:** application-record merge/trim beyond full-title delete; flag for phase-level research in Phase 7.
- **Secret-at-rest for synced server config:** the synced blob may carry auth credentials; treat as sensitive (encrypt at rest, owner-scoped like v1.0 save blobs).

## Sources

### Primary (HIGH confidence)
- libnx ncm.h / ns.h / es.h (switchbrew.github.io/libnx, release v4.11.1) - install/record/ticket API surface.
- Awoo-Installer (install.cpp, install_nsp.cpp, nca_writer.cpp) + Goldleaf (nsp_Installer) + Tinfoil (install_nsp_remote.cpp) - verified install sequence and NCZ writer.
- Tinfoil Custom Index / Network Install docs + TinfoilWebServer README - JSON index schema, [titleid] filename convention, auth, categories.
- Nintendo Switch Brew (NCM services, CNMT, Memory layout) - placeholder/register/commit sequence, content meta types, applet vs application memory.
- thomaz codebase (direct read): title_service_switch.cpp, system_status.cpp, http_client.hpp, http_client_curl.cpp, mods/mod_download.cpp, mods/mod_actions.cpp, themes/nca_extract_switch.cpp, saves/http_cloud_save_client.cpp, api/.../saves.ts, prisma/schema.prisma, thomaz_activity.hpp, async_guard.hpp, CMakeLists.txt.

### Secondary (MEDIUM confidence)
- nicoboss/nsz (NCZ format spec) - 0x4000 offset, per-section keys, solid vs block, decompress memory-leak fix.
- splitNSP / FAT32 split-file references - archive-bit folder, 4 GB limit, 00/01/02 parts.
- GBAtemp threads - DBI/Awoo behaviors, personalized vs common tickets, NSZ; WikiTemp RequiredSystemVersion.

### Tertiary (LOW confidence)
- Title-ID derivation convention (NUT/Tinfoil naming) - base / +0x800 update / +0x1000 DLC rule; validate against real indexes.

---
*Research completed: 2026-06-06*
*Ready for roadmap: yes*
