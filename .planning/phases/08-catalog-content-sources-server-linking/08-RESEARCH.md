# Phase 8: Catalog, Content Sources & Server Linking - Research

**Researched:** 2026-06-06
**Domain:** Tinfoil-style content-source linking · Switch title-ID logic · cover-art catalog UI · streaming HTTP seam · owner-scoped cloud config sync
**Confidence:** HIGH (codebase analogs, title-ID bitmask, libcurl seam) · MEDIUM (Tinfoil index schema — community spec, must validate on a real server) · MEDIUM (titledb cover-art source — public but availability/licensing risk)

## Summary

Phase 8 is a **read-only, zero-install** feature built almost entirely from patterns the codebase already ships. The decidable work — Tinfoil index parse, title-ID→kind derivation, base-title grouping, `directories` recurse/flatten, sort/filter, and the server-link sync codec — is pure logic that belongs in a new `source/core/games/` namespace under the host doctest gate (`tests/Makefile`), exactly mirroring `core/cheat_db`, `core/saves/cloud_save_json`, and `core/saves/save_sync`. The Switch-facing work — catalog grid Activity, on-SD index cache, source-list/credential persistence, local-icon fallback — mirrors `theme_browser_activity`, `cheat_store`, `app_settings`, and `title_service_switch`. The cloud-sync half is a near-mechanical clone of the `SaveSlot` + `saves.ts` + `HttpCloudSaveClient` trio: one new owner-scoped Prisma model, one new JWT-authed Fastify route, config-only.

Three external unknowns drove the research. (1) The **Tinfoil JSON index schema** is community-documented by blawar's own docs — `files: [{url, size}]` (or bare string URLs), `directories: [url...]`, plus `success`/`error` MOTD, `referrer`, and `headers` keys — but it is not a formal spec, so the parser must be validated against a real user server early and must tolerate missing/extra keys. (2) The **cover-art source** for D-02 is **blawar/titledb** regional JSON (`US.en.json` etc.) whose entries carry an `iconUrl`/`bannerUrl` pointing at Nintendo's eShop CDN; this is bulk-regional (no per-ID endpoint), large, and carries real availability/licensing risk — treat as metadata-only and design the 3-tier fallback so the catalog is fully functional with art absent. (3) The **streaming/Range seam** extends `http_client_curl` with a `CURLOPT_WRITEFUNCTION` sink callback + `CURLOPT_RANGE`/`CURLOPT_RESUME_FROM_LARGE`, validating `Accept-Ranges: bytes` — but this phase only *designs and proves the interface*; Phase 10 consumes it.

One security finding materially affects the plan: libcurl already strips the `Authorization` header on cross-host redirects by default (good for basic-auth-in-URL), **but it does NOT strip custom headers** (`CURLOPT_HTTPHEADER`). SRC-02's custom-header auth variant therefore leaks to redirect targets unless the redirect is handled with host-awareness. This must be an explicit task.

**Primary recommendation:** Build `core/games/` (index parse + title-ID logic + grouping + recurse-bound + sync codec) under doctest first, point its parser at a real Tinfoil server before building UI, then layer the grid Activity, SD cache, source persistence, and the one-model/one-route cloud sync on top — reusing the named analogs verbatim. Establish the streaming seam as an interface + a single host-validated Range probe; do not build the install loop.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01 — Grouped by base title.** One cover-art card per base game (mask low title-ID bits to derive base ID); updates/DLC nested in the title's detail view, not as top-level cards. Grouping + base/update/DLC kind-derivation = pure `core/games/`, host-doctest-gated.
- **D-02 — Cover art: online → local → placeholder (3-tier).** Try online titledb (title ID → cover/icon) first; fall back to installed libnx icon (if installed); else a name+kind placeholder. Cache art on SD. Art source is metadata only — never content. Exact endpoint unconfirmed (resolved below). Fail-closed TLS for it.
- **D-03 — Art grid default, compact list toggle.** Default cover-art grid (model theme/mod browsers); button toggles a compact text list for large catalogs.
- **D-04 — Search + sort + content filter.** Live search by name and title ID; sort by name/size; content-filter chips: has update / has DLC / base-only. No installed-state cross-reference (that is Phase 11).
- **D-05 — Per-source SD cache + auto-refresh-if-stale.** On open, render cached index instantly from SD (per source), then refresh in background. Manual Refresh always available. Mirror cheat-db SD index cache.
- **D-06 — Refresh aggressiveness: every open when online.** Paint cache for instant first frame, then always background-refresh if online. Offline → show cached (with offline/stale note).
- **D-07 — Nested `directories`: recurse + flatten, bounded.** Follow sub-directory sub-indexes, merge into the one unified grouped catalog (not a folder browser). Enforce max recursion depth + max total entries/requests; surface a note when a bound is hit.

### Claude's Discretion
- **Source management & navigation.** Entry point = new "Games"/"Content" card on Home (alongside themesCard/savesCard/modsCard/cheatsCard/systemCard). Model = a list of multiple linkable sources (not a single active server). Source list empty by default (SRC-04). Add-source flow = URL + optional auth (basic-auth-in-URL / custom header / referrer). Local SD = a peer pseudo-source in the same list, browsed through the same catalog/detail surface (SRC-03). USB-HDD out of scope (GAME-F02).
- **Credential security (satisfies SYNC-02 "protected at rest").** On device: store source config (incl. credentials) under existing `app_settings` config location (`/switch/thomaz/config/...`); SD is plaintext-readable, so at minimum keep credentials out of logs and out of any synced *catalog* data. In cloud: owner-scoped record (one Prisma model + one route mirroring SaveSlot+saves+JWT), credentials encrypted at rest server-side. Config only — never catalog or content bytes. Full E2E (device key) not required for MVP. Planner picks the concrete at-rest scheme (server-side encrypted column vs app-layer encryption).

### Locked upstream constraints (do NOT re-litigate)
- No default source/index/keys bundled or enabled — source list starts empty (SRC-04).
- Cloud API stores config only, never content — enforce in Prisma model + route schema (SYNC-01). Content hosting/proxying via the API is an explicit anti-pattern.
- Content bytes NEVER flow through `IHttpClient::request` / `HttpResponse.body` (OOM on multi-GB titles). Establish a streaming/Range seam here; validate `Accept-Ranges: bytes` + a streaming/Range fallback against a real target server. Seam consumed by Phase 10.
- Fail-closed TLS; strip `Authorization` on cross-host redirects; cap the index JSON parse size.
- Encrypted-index ("encrypted shop") Tinfoil variant → out of scope for MVP.
- New decidable logic lives in pure `core/games/` under the doctest gate; `platform/games/` stays thin Switch orchestration; no exceptions — return-value error handling.

### Deferred Ideas (OUT OF SCOPE — confirm excluded)
- Full E2E credential encryption (device-held key) — future hardening, not MVP.
- USB-HDD mass-storage source (GAME-F02).
- XCI/XCZ gamecard sources (GAME-F01).
- Encrypted-index ("encrypted shop") Tinfoil variant.
- Update/DLC "available" badges for *installed* titles (Phase 11 — needs installed-state cross-reference).
- NCM writes / downloads-to-install / install queue / uninstall / installed-title management (Phases 9–11).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SRC-01 | Link a content server by URL returning a Tinfoil-style JSON index (`files[{url,size}]`, `directories`) | Tinfoil schema confirmed (Standard Stack + Pitfall 1); parse in `core/games/index_parse` |
| SRC-02 | Link a server requiring auth (basic-auth-in-URL, custom header, or referrer gate) | Auth-variant mechanics + the custom-header-redirect leak (Security Domain, Pitfall 4) |
| SRC-03 | Browse and install from local NSP/NSZ files on the SD card | Local pseudo-source via `dirent` scan (Architecture Pattern 5); same catalog surface |
| SRC-04 | No content server bundled/enabled by default — source list empty until user adds one | Source list persisted in `app_settings`, starts empty; nothing bundled (verify in Pitfall 6) |
| CAT-01 | Browse catalog as grid/list with cover art, title name, file size, reusing titledb/icon UI | Cover-art 3-tier (D-02 resolution); grid models `theme_browser_activity` |
| CAT-02 | Each entry shows kind (base/update/DLC) from 64-bit title ID; filter/search | Title-ID bitmask logic (Architecture Pattern 1, HIGH confidence); filter/sort in core |
| SYNC-01 | Server-link config syncs one-tap to thomaz account (config only, never content), reusing JWT + cloud-saves pattern | One Prisma model + one route mirroring SaveSlot/saves.ts (Architecture Pattern 6) |
| SYNC-02 | Synced credentials protected at rest, scoped to owning account | Owner-scoped `@@unique([userId, ...])` + at-rest encryption options (Security Domain) |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Tinfoil index JSON parse | Pure core (`core/games/`) | — | Decidable, host-testable; no I/O. Mirrors `core/cheat_db`. |
| Title-ID → kind (base/update/DLC) + base-ID grouping | Pure core | — | Pure bitmask logic; doctest-gated (D-01). |
| `directories` recurse/flatten + safety bounds | Pure core (planner) + platform (fetch) | platform fetch loop | Bound logic & merge are pure; the per-dir fetch is platform orchestration driving a core-decided plan. |
| Catalog sort / filter / search | Pure core | — | Pure transforms over the grouped model (D-04). |
| Server-link sync codec (serialize/deserialize config to/from API JSON) | Pure core | — | Pure; doctest-gated. Mirrors `core/saves/cloud_save_json`. |
| Index fetch (HTTP, capped) | Platform (`platform/games/`) | core fetcher seam | Network is platform; core consumes an injected fetcher lambda (like `themezer_browse`). |
| Cover-art fetch + SD cache | Platform | — | I/O + libnx icon; thin orchestration. |
| Source-list + credential persistence | Platform (`app_settings`) | — | SD JSON config; plaintext, keep out of logs. |
| Catalog grid / detail Activity | App (`source/app/`) | — | UI composition only; owns no business logic. |
| Streaming/Range HTTP capability | Platform (`http_client_curl`) | — | libcurl sink callback; interface designed here, consumed Phase 10. |
| Owner-scoped config sync (cloud) | API (`api/src/`) | — | One Prisma model + one Fastify route, JWT-authed, config-only. |

## Standard Stack

This phase introduces **no new third-party libraries.** Every dependency is already vendored and built. The "stack" is the set of existing in-repo facilities the plan must reuse.

### Core (existing, reuse verbatim)
| Library / Facility | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| nlohmann/json | vendored `lib/json/` (header-only) | Parse the Tinfoil index + titledb + API JSON | Already the project's only JSON parser (`core/cheat_db`, `core/saves/cloud_save_json`) `[VERIFIED: tests/Makefile -I../lib/json + core usage]` |
| libcurl (+ mbedTLS on Switch) | system / devkitPro | HTTP fetch + the new streaming/Range seam | Already the sole HTTP transport (`http_client_curl.cpp`) `[VERIFIED: source/platform/http_client_curl.cpp]` |
| doctest | vendored `lib/doctest/` | Host gate for all `core/games/` logic | The established core test framework (`tests/Makefile`) `[VERIFIED: tests/Makefile]` |
| Borealis (vendored fork) | `lib/borealis/` | Catalog grid/detail/source-list Activities | Every screen is a Borealis Activity `[VERIFIED: source/app/*_activity.cpp]` |
| libnx `ns` (NsApplicationControlData) | devkitPro | Installed-title icon fallback (D-02 tier 2) | Already used by `title_service_switch.cpp` for icons `[VERIFIED: source/platform/title_service_switch.cpp]` |

### Supporting (API side, existing)
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Prisma | 6.8 | New owner-scoped config model | SYNC-01/02 storage (mirror `SaveSlot`) `[VERIFIED: INTEGRATIONS.md + schema.prisma]` |
| Fastify + @fastify/jwt | (existing) | New config-sync route, JWT-authed | SYNC-01 endpoint (mirror `saves.ts` + `app.authenticate`) `[VERIFIED: api/src/routes/saves.ts, plugins/auth.ts]` |
| zod | (existing) | Validate the sync request body | Route input validation (mirror `savePutFieldsSchema`) `[VERIFIED: api/src/routes/saves.ts]` |
| node:crypto (AES-256-GCM) | Node built-in | Server-side credential-at-rest encryption (one at-rest option) | SYNC-02 if app-layer-encrypted column chosen `[ASSUMED]` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| blawar/titledb regional JSON for cover art | Direct eShop CDN per-title guess | No per-title-ID endpoint exists publicly; titledb is the index that maps ID→`iconUrl`. The 3-tier fallback (D-02) is the real mitigation for titledb availability risk. |
| node:crypto AES-GCM app-layer encryption | Postgres `pgcrypto` column encryption / KMS | App-layer keeps the key in API env (matches existing `JWT_SECRET` posture, no new infra); pgcrypto needs an extension + key management. Either satisfies SYNC-02; planner decides. |
| Recurse `directories` eagerly | Lazy/per-folder browse | D-07 locks recurse-and-flatten into one unified catalog; lazy browse is a different UX the user rejected. |

**Installation:** None — no new packages. (Confirm titledb/eShop CDN reachability at plan time; both are public, unauthenticated HTTPS.)

**Version verification:** No new packages to verify on a registry. The titledb and eShop CDN endpoints are network resources, not packages — validate reachability and TLS at plan time against a real fetch.

## Package Legitimacy Audit

No external packages are installed in this phase (no `npm install`, no new C++ deps). The titledb and eShop CDN are network endpoints, not registry packages, so the slopcheck/registry gate does not apply.

| Package | Registry | Disposition |
|---------|----------|-------------|
| (none) | — | No new packages added in Phase 8 |

**Packages removed due to slopcheck [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

*Network endpoints to validate at plan time (NOT packages): `raw.githubusercontent.com/blawar/titledb/...` and `img-eshop.cdn.nintendo.net` — verify reachability + fail-closed TLS, treat as metadata-only.*

## Architecture Patterns

### System Architecture Diagram

```
                         HOME ("Games"/"Content" card)
                                   │ push
                                   ▼
                    ┌──────────────────────────────┐
   add source ─────▶│  SourceListActivity (app)    │◀── source list (app_settings, SD)
   (URL + auth)     │  - remote servers + local SD │     starts EMPTY (SRC-04)
                    └──────────────┬───────────────┘
                                   │ pick source → push
                                   ▼
                    ┌──────────────────────────────┐
                    │  CatalogActivity (app)        │
                    │  grid ⇄ list (D-03)           │
                    │  search/sort/filter (D-04)    │
                    └───┬───────────────────┬───────┘
            cache-first │                   │ background refresh if online (D-06)
                        ▼                   ▼
        ┌───────────────────────┐   ┌─────────────────────────────────────┐
        │ SD index cache (per   │   │  platform/games/ (thin orchestration)│
        │ source) — cheat-db    │   │  fetch index (IHttpClient, CAPPED)   │
        │ pattern (D-05)        │   │  recurse `directories` (bounded D-07) │
        └───────────────────────┘   └──────────────┬──────────────────────┘
                                                    │ raw JSON
                                                    ▼
                                ┌────────────────────────────────────────┐
                                │  core/games/ (PURE, doctest-gated)       │
                                │  index_parse  → files[{url,size}], dirs  │
                                │  title_id     → kind(base/update/DLC),   │
                                │                 base-ID mask (D-01)       │
                                │  grouping     → 1 card per base title     │
                                │  recurse_plan → depth/count bounds (D-07) │
                                │  sort/filter/search (D-04)                │
                                │  source_link codec (SYNC serialize)       │
                                └──────────────┬───────────────────────────┘
                                               │ grouped catalog model
                  ┌────────────────────────────┼───────────────────────────┐
                  ▼ (per card)                  ▼ (detail)                   ▼ (one-tap sync)
        ┌──────────────────┐        ┌────────────────────┐      ┌─────────────────────────┐
        │ COVER ART (D-02) │        │ Detail: Base /      │      │ HttpSourceSyncClient    │
        │ 1 titledb iconUrl│        │ Update vN / DLC rows│      │ (mirror HttpCloudSave)  │
        │ 2 libnx icon     │        │ + sizes             │      │   PUT /sources (JWT)    │
        │ 3 name+kind ph.  │        └────────────────────┘      └───────────┬─────────────┘
        └──────────────────┘                                                │ config ONLY
                                                                            ▼
                                                          ┌──────────────────────────────┐
                                                          │  thomaz API (Fastify)        │
                                                          │  /sources route (authenticate)│
                                                          │  Prisma model (owner-scoped,  │
                                                          │  creds encrypted at rest)     │
                                                          │  NEVER catalog/content bytes  │
                                                          └──────────────────────────────┘

  ── separate seam, designed here / consumed Phase 10 ──
  http_client_curl: add streaming sink (CURLOPT_WRITEFUNCTION → caller buffer/file)
                    + CURLOPT_RANGE / RESUME_FROM_LARGE; validate Accept-Ranges: bytes
```

### Recommended Project Structure
```
source/core/games/         # PURE, doctest-gated (NEW)
├── index_parse.{hpp,cpp}    # Tinfoil JSON → ParsedIndex{files[{url,size,name}], directories[]}
├── title_id.{hpp,cpp}       # kind(id)->Base/Update/DLC; base_id(id); content_index(id)
├── catalog.{hpp,cpp}        # group flat files → vector<GroupedTitle{base, base/update/DLC rows}>
├── catalog_view.{hpp,cpp}   # sort (name/size) + filter (hasUpdate/hasDLC/baseOnly) + search
├── recurse_plan.{hpp,cpp}   # bounds: max depth, max entries, max requests; "bound hit" flag
└── source_link.{hpp,cpp}    # SourceConfig <-> sync JSON codec (no secrets in catalog data)

source/platform/games/     # thin Switch orchestration (NEW)
├── index_fetcher.{hpp,cpp}  # IHttpClient fetch (capped), drives recurse_plan
├── source_store.{hpp,cpp}   # source list + credential persistence (uses app_settings/fs)
├── catalog_cache.{hpp,cpp}  # per-source SD index cache (cheat-db pattern)
├── cover_art.{hpp,cpp}      # 3-tier: titledb fetch+cache → libnx icon → placeholder
└── http_source_sync_client.{hpp,cpp}  # mirror HttpCloudSaveClient (config sync)

source/app/                # UI (NEW)
├── source_list_activity.{hpp,cpp}
├── catalog_activity.{hpp,cpp}   # model on theme_browser_activity
└── catalog_detail_activity.{hpp,cpp}

api/                       # cloud (NEW)
├── prisma/schema.prisma     # + one model (e.g. SourceLink)
└── src/routes/sources.ts    # one route, authenticate, config-only, register in app.ts

resources/xml/activity/    # + games_*.xml, + Games card in home.xml
tests/                     # + test_index_parse / test_title_id / test_catalog /
                           #   test_catalog_view / test_recurse_plan / test_source_link
```

### Pattern 1: Title-ID → kind + base-ID grouping (D-01, CAT-02) — pure core
**What:** Derive base/update/DLC from the 64-bit title (program) ID and the base ID for grouping.
**When to use:** Every catalog entry; the grouping key.
**Rules (community consensus, used by Tinfoil/NX_Game_Info):**
- **Base application ID** is aligned: low 13 bits zero — `base = id & 0xFFFFFFFFFFFFE000ULL`.
- **Update/patch** ID = `base | 0x800` (bit 0x800 set, the only variant with it).
- **DLC (AOC)** IDs live in `base | 0x1000 .. base | 0x1FFF` (DLC content index = `id - base - 0x1000`, i.e. `(id & 0x1FFF) - 0x1000`, range 0x000..0xFFF).
```cpp
// Source: switchbrew Title list + community tools (NX_Game_Info). HOST-DOCTEST.
enum class TitleKind { Base, Update, Dlc, Unknown };

inline std::uint64_t base_title_id(std::uint64_t id) {
    return id & 0xFFFFFFFFFFFFE000ULL;            // mask low 13 bits
}
inline TitleKind title_kind(std::uint64_t id) {
    std::uint64_t low = id & 0x1FFFULL;
    if (low == 0x000)  return TitleKind::Base;
    if (low == 0x800)  return TitleKind::Update;
    if (low >= 0x1000) return TitleKind::Dlc;      // 0x1000..0x1FFF
    return TitleKind::Unknown;                      // other low bits = not a standard title id
}
```
Confidence: **HIGH** — corroborated by switchbrew + multiple tools. Add doctest cases for a known triple (base/update/DLC of one game) plus the `Unknown` guard.

### Pattern 2: Tinfoil index parse (SRC-01) — pure core
**What:** Parse the index JSON into a normalized struct, tolerant of both `files` shapes and missing keys.
**Schema (from blawar's docs):** `files` = array of either `"url-string"` or `{ "url": ..., "size": <bytes> }`; `directories` = array of URL strings; optional `success`/`error` (MOTD), `referrer`, `headers` (array of `"Header: value"`), `version`. Filename override via `#filename.nsp` shebang on the URL. (titledb key, encrypted index, gdrive/1fichier keys → out of scope.)
```cpp
// Source: blawar.github.io/tinfoil/custom_index. HOST-DOCTEST with real-server sample.
struct IndexFile { std::string url; std::uint64_t size = 0; std::string nameOverride; };
struct ParsedIndex {
    std::vector<IndexFile>   files;
    std::vector<std::string> directories;
    std::string motd;            // from "success"/"error"
    bool        ok = false;
};
ParsedIndex parse_index(const std::string& json);  // tolerant: skip non-.nsp/.nsz, ignore unknown keys
```
Confidence: **MEDIUM** — schema is community-documented, not formal. **Validate against a real user server before building UI** (research flag). Tolerate: bare-string files, absent `size`, absent `directories`, extra keys.

### Pattern 3: `directories` recurse + flatten, bounded (D-07) — pure plan + platform fetch
**What:** Pure `recurse_plan` decides whether to descend and enforces bounds; platform `index_fetcher` executes fetch+parse, feeding results back.
**Bounds (recommended):** `max_depth = 3`, `max_total_entries = 50000`, `max_requests = 256`. Surface a "catalog truncated (limit reached)" note when any bound trips. Guard against cycles (a directory URL that re-points at an ancestor) by tracking visited URLs.
```cpp
struct RecurseBounds { int maxDepth = 3; std::size_t maxEntries = 50000; int maxRequests = 256; };
struct RecurseState { int depth = 0; std::size_t entries = 0; int requests = 0; bool truncated = false; };
bool may_descend(const RecurseState&, const RecurseBounds&);  // pure, doctest-gated
```
Confidence: **MEDIUM** (bounds are a judgment call) — values chosen to protect a memory-constrained console against a pathological server; planner may tune.

### Pattern 4: Catalog Activity models `theme_browser_activity` (CAT-01, D-03)
**What:** Grid of `brls::Box` cards (cover image + name + size + kind chip), `runAsync` fetch with a `listGen` generation guard and `cancelledFlag()`, populate on the UI thread. Add a grid⇄list toggle and search/sort/filter chips.
**Reuse directly:** the thumbnail-fetch + `listGen` use-after-free guard, `cancelledFlag()`, the loading/empty/results visibility dance, `image_transcode` for non-PNG art. See `source/app/theme_browser_activity.cpp:124-278`.
Confidence: **HIGH** — this is a direct structural clone.

### Pattern 5: Local SD as a peer pseudo-source (SRC-03)
**What:** A pseudo-source whose "index" is produced by scanning an SD directory for `.nsp`/`.nsz`, synthesizing `IndexFile{url=file path, size=stat size}`, then feeding the SAME `core/games/catalog` grouping/kind path. Use the `dirent`/`stat` pattern already in `cheat_store.cpp:dir_has_nonempty_txt`. The title ID for kind/grouping comes from the filename if Tinfoil-named (`[TITLEID][vNN].nsp`); otherwise the entry groups as Unknown until Phase 9 parses PFS0/NACP (out of scope here).
Confidence: **HIGH** for the scan; **MEDIUM** for filename-derived title IDs (depends on user naming — fall back to Unknown gracefully).

### Pattern 6: Cloud config sync mirrors the SaveSlot trio (SYNC-01/02)
**What:** One new Prisma model + one new route + one new client, owner-scoped, JWT-authed, config-only.

Prisma model (mirror `SaveSlot`):
```prisma
/// User-owned content-source link config. CONFIG ONLY — never catalog or content bytes.
model SourceLink {
  id           String   @id @default(cuid())
  userId       String
  user         User     @relation(fields: [userId], references: [id], onDelete: Cascade)
  label        String   @default("")
  url          String                    // server index URL
  authType     String   @default("none") // none | basicInUrl | header | referrer
  authSecretEnc String?                  // encrypted-at-rest credential (null if none)
  updatedAt    DateTime @updatedAt
  @@index([userId])                      // owner-scoped (SYNC-02)
}
```
(Add a `User.sourceLinks SourceLink[]` back-relation, like `saveSlots`.)

Route (mirror `saves.ts`): `GET /sources` (list owner's), `PUT /sources/:id` or `POST /sources` (upsert), `DELETE /sources/:id`. Each `{ preHandler: [app.authenticate] }`, `userId = userIdFromRequest(request)` → 401 if absent, all Prisma queries `where: { userId, ... }`, zod-validated body, **no file/blob handling** (the proof that it's config-only). Register in `app.ts` next to `savesRoutes`.

Client (mirror `HttpCloudSaveClient`): pass token per call, `Bearer` header, 401 → `kCloudAuthExpired`, return-value status structs.

Confidence: **HIGH** — near-mechanical clone. The only design choice is the at-rest encryption scheme (see Security Domain).

### Anti-Patterns to Avoid
- **Business logic in `platform/games/` or Activities.** Index parse, kind derivation, grouping, sort/filter, recurse bounds, sync codec ALL go in `core/games/` (the only host-tested layer). Platform = fetch + persist; Activity = compose. (ARCHITECTURE.md "Adding business logic to platform layer".)
- **Routing content bytes through `IHttpClient::request`/`HttpResponse.body`.** It `append`s the whole body to a `std::string` (`http_client_curl.cpp:10-14`) → OOM on multi-GB. The index/art are small (cap them); content uses the new streaming seam.
- **Bundling any default source/index/key.** SRC-04: the source list MUST start empty. No seed data, no default URL, nothing enabled.
- **Storing catalog or content bytes in the cloud model/route.** SYNC-01 is config only; a `blobKey`/file field is the anti-pattern.
- **Enabling `CURLOPT_UNRESTRICTED_AUTH` or trusting custom-header redirects.** See Security Domain.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON parsing | A hand-written tokenizer | nlohmann/json (`lib/json`) | Already vendored, already the project standard, tolerant parsing via `value(key, default)` |
| HTTP + TLS verification | A raw socket client | `IHttpClient`/`HttpClientCurl` + `apply_curl_tls` | Fail-closed TLS, cancel hook, connection sharing already solved |
| Capped file read from SD | A naive `fread` loop | `read_text_file` (`cheat_store.cpp`) | Already caps at 1 MiB and distinguishes EOF vs I/O error (WR-04/WR-05) |
| Recursive mkdir on write | `mkdir` once | `write_text_file` → `ensure_parent_dirs` | Save-backup bug already fixed there (260605-sbk) |
| Installed-title icons | New libnx control-data reader | `title_service_switch` / `InstalledTitle.icon` | Already extracts JPEG icon from NACP control data |
| Owner-scoped auth on the API | New auth check | `app.authenticate` + `userIdFromRequest` + `where:{userId}` | JWT verify + revocation already wired in `plugins/auth.ts` |
| Optimistic-concurrency sync | New revisioning scheme | The `revision` + 409 pattern in `saves.ts` (if needed) | Proven conflict handling; reuse if multi-console edit matters |
| Image decode for non-PNG art | New decoder | `platform/image_transcode` (`to_decodable_image`) | Already transcodes WebP→PNG for stb_image |
| Title-ID hex formatting | Ad-hoc sprintf | `titleIdHex` pattern (`http_cloud_save_client.cpp:9-13`) | `%016llx`, consistent with `parseTitleIdParam` on the API |

**Key insight:** Almost nothing here is novel I/O — the novelty is *pure logic* (parse, derive, group, bound, codec), which is exactly what belongs in `core/games/` under doctest. Every platform/UI/API piece has a working analog in-tree to clone.

## Common Pitfalls

### Pitfall 1: Treating the Tinfoil schema as fixed/spec-grade
**What goes wrong:** Parser assumes `files` is always `[{url,size}]`, requires `size`, or chokes on unknown keys → real servers (bare-string files, missing sizes, `headers`/`referrer`/`titledb` present) fail to parse.
**Why:** The format is community-reverse-engineered; servers vary (TinfoilWebServer, manual indexes, gdrive-backed).
**How to avoid:** Tolerant parse — accept both file shapes, default missing `size` to 0, ignore unknown keys, never hard-fail on a single bad entry. **Validate against a real user server early** (research flag) and keep a captured real sample as a doctest fixture.
**Warning signs:** Parser works on the docs example but a real server yields an empty catalog.

### Pitfall 2: OOM buffering content (and even large indexes/art) in a std::string
**What goes wrong:** `HttpResponse.body` accumulates the whole response (`http_client_curl.cpp:writeToString`). A multi-GB title kills the console; even a huge index/art can stress it.
**Why:** The existing client is buffer-only.
**How to avoid:** (a) Establish the **streaming/Range seam** for content (Phase 10). (b) **Cap** the index parse and art fetch — refuse responses over a sane limit (e.g. index ≤ 8 MiB, art ≤ 4 MiB) by checking `Content-Length` and/or aborting the write callback past the cap (the cap is itself a write-function decision; mirror `read_text_file`'s 1 MiB SD cap rationale).
**Warning signs:** Memory spikes on a big catalog; app dies fetching a large index.

### Pitfall 3: Recursive `directories` exhausts the console
**What goes wrong:** A server with deep/cyclic `directories` triggers unbounded fetches/allocation.
**Why:** Recurse-and-flatten (D-07) without bounds.
**How to avoid:** Enforce `recurse_plan` bounds (depth/entries/requests), track visited URLs to break cycles, surface a truncation note. (Pattern 3.)
**Warning signs:** Refresh never completes; request count climbs without end.

### Pitfall 4: Custom-header auth leaks across redirects (SRC-02)
**What goes wrong:** libcurl strips `Authorization` on cross-host redirects by default, BUT it does **not** strip custom headers set via `CURLOPT_HTTPHEADER`. A server can 30x-redirect to an attacker host and receive the user's custom auth header.
**Why:** `CURLOPT_FOLLOWLOCATION` is on (`http_client_curl.cpp:86`) and only `Authorization`/`Cookie` are auto-protected.
**How to avoid:** For the custom-header auth variant, either (a) disable auto-follow and handle redirects manually with a same-host check before re-attaching the header, or (b) re-attach the custom auth header only when the redirect target host matches the original. Never set `CURLOPT_UNRESTRICTED_AUTH`. For basic-auth-in-URL, the default strip is correct — keep it.
**Warning signs:** A misbehaving/malicious index URL that redirects off-host still sees the auth header.

### Pitfall 5: Cover art treated as required → broken-looking catalog
**What goes wrong:** Relying on titledb being reachable; entries render as blank squares (DLC often has no icon at all).
**Why:** titledb is bulk-regional, large, and can be unavailable; not every ID has an `iconUrl`.
**How to avoid:** Implement the full 3-tier fallback (titledb → libnx icon → name+kind placeholder), cache successes on SD, and make the catalog fully usable (name/size/kind/search) with zero art. Fetch the regional titledb once and cache (it is large — do not fetch per card).
**Warning signs:** A grid of blank squares when offline or when titledb is down.

### Pitfall 6: A default/seed source slips in (SRC-04 violation)
**What goes wrong:** A placeholder URL, example index, or "demo" source ends up bundled/enabled.
**Why:** Convenience during dev.
**How to avoid:** Source list persistence (`source_store`) must initialize empty; no fixture URL in shipped config; the empty-state UI is the default. Add a doctest/assertion that a fresh config yields zero sources.
**Warning signs:** Catalog appears before the user adds anything.

### Pitfall 7: Credentials end up in logs or in synced catalog data (SYNC-02)
**What goes wrong:** Logging a source URL with embedded basic-auth, or syncing credentials inside catalog payloads.
**Why:** URLs and configs flow through logging/sync paths.
**How to avoid:** Keep credentials out of `brls::Logger` output (the API already redacts `authorization`/`cookie` via pino — see `app.ts:24-27`; the *client* logs must do the same). The sync codec serializes config only; the credential goes in the dedicated `authSecretEnc` field, encrypted at rest. Never include catalog/content in the sync body.
**Warning signs:** A password visible in nxlink logs; catalog JSON in the API payload.

## Code Examples

### Tolerant index parse with nlohmann (handles both file shapes)
```cpp
// Source: blawar tinfoil custom_index schema + project nlohmann usage. HOST-DOCTEST.
#include <nlohmann/json.hpp>
ParsedIndex parse_index(const std::string& body) {
    ParsedIndex out;
    nlohmann::json j = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return out;          // ok stays false
    if (auto it = j.find("files"); it != j.end() && it->is_array()) {
        for (const auto& f : *it) {
            IndexFile e;
            if (f.is_string())       e.url = f.get<std::string>();
            else if (f.is_object())  { e.url = f.value("url", ""); e.size = f.value("size", 0ull); }
            if (!e.url.empty()) out.files.push_back(std::move(e)); // skip bad entries, don't fail
        }
    }
    if (auto it = j.find("directories"); it != j.end() && it->is_array())
        for (const auto& d : *it) if (d.is_string()) out.directories.push_back(d.get<std::string>());
    out.motd = j.value("success", j.value("error", std::string{}));
    out.ok = true;                                                 // parsed; emptiness is valid
    return out;
}
```

### Streaming/Range seam interface (designed here; Phase 10 consumes)
```cpp
// Source: libcurl CURLOPT_WRITEFUNCTION / CURLOPT_RANGE / CURLOPT_RESUME_FROM_LARGE.
// Add to IHttpClient WITHOUT touching the buffering request() path.
struct StreamRequest {
    std::string url;
    std::vector<std::pair<std::string,std::string>> headers;
    std::uint64_t rangeStart = 0;            // CURLOPT_RESUME_FROM_LARGE (resume offset)
    std::shared_ptr<std::atomic<bool>> cancelled;
    // sink: called with each chunk; return false to abort. NEVER buffers whole body.
    std::function<bool(const std::uint8_t* data, std::size_t len)> sink;
};
struct StreamResult { long status = 0; bool acceptsRanges = false; std::uint64_t totalSize = 0; bool ok = false; };
// Impl: CURLOPT_WRITEFUNCTION -> trampoline to `sink`; set CURLOPT_RANGE/RESUME_FROM_LARGE;
// read Accept-Ranges via a header callback (CURLOPT_HEADERFUNCTION) or HEAD probe.
StreamResult stream(const StreamRequest&);   // Phase 8 proves it with a small Range probe only.
```

### Range/Accept-Ranges probe (the only streaming validation done in Phase 8)
```cpp
// Phase 8 deliverable: prove the seam against a REAL target server, not build the install loop.
// 1) Issue a small ranged GET (e.g. bytes=0-1023) to a content URL from the linked server.
// 2) Confirm response is 206 Partial Content AND header "Accept-Ranges: bytes" (or 206 itself).
// 3) Fallback path: if server returns 200 (no range support), record acceptsRanges=false so
//    Phase 10 falls back to a full streamed GET (still streamed, just not resumable).
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Authorization` forwarded on all redirects | libcurl strips it cross-host by default | curl 7.58.0 (2018) | basic-auth-in-URL is safe by default; custom headers are NOT (Pitfall 4) |
| `titles.US.en.json` (full per-title) in titledb repo | Removed (bloat); use `US.en.json` regional files | per blawar/titledb README | Fetch the regional file, not a per-title endpoint |

**Deprecated/outdated:**
- Encrypted-index ("encrypted shop") Tinfoil variant — explicitly out of scope for this MVP.
- Per-title-ID cover-art endpoint — none exists publicly; titledb regional JSON is the lookup table.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | blawar/titledb regional JSON (`US.en.json`) carries `iconUrl`/`bannerUrl` per entry and is currently reachable | Standard Stack / D-02 | Cover-art tier 1 fails; mitigated by the 3-tier fallback making art optional. Validate at plan time. |
| A2 | node:crypto AES-256-GCM app-layer encryption is an acceptable at-rest scheme for SYNC-02 | Standard Stack / Security | Planner may prefer pgcrypto/KMS; both satisfy SYNC-02. Needs decision, not a blocker. |
| A3 | Recurse bounds depth=3 / entries=50k / requests=256 are sane for a console | Pattern 3 / Pitfall 3 | Too low truncates legit catalogs; too high risks OOM. Tunable; validate against the real test server. |
| A4 | Tinfoil-named local files (`[TITLEID][vNN].nsp`) let us derive title ID for kind/grouping of SD entries | Pattern 5 | Non-conforming names group as Unknown until Phase 9 PFS0 parse — acceptable degraded UX. |
| A5 | DLC content-index extraction `(id & 0x1FFF) - 0x1000` is correct for grouping/display | Pattern 1 | Display-only; base-ID mask (HIGH confidence) still groups correctly even if index label is off. |
| A6 | Index ≤ 8 MiB / art ≤ 4 MiB caps are safe upper bounds | Pitfall 2 | A legit huge index could be rejected; raise the cap with a streamed/capped read if needed. |

## Open Questions

1. **Which real Tinfoil server to validate against?**
   - What we know: schema is community-documented; servers vary (TinfoilWebServer, manual).
   - What's unclear: the concrete server the user/tester will point at.
   - Recommendation: Plan a Wave-0 task to capture a real index JSON sample from the tester's own server and commit it as a doctest fixture BEFORE the UI is built (satisfies the research flag).

2. **At-rest credential scheme for SYNC-02 (server-side encrypted column vs app-layer AES-GCM)?**
   - What we know: either satisfies SYNC-02; full E2E is explicitly out of scope.
   - What's unclear: the project's preference for key location (API env vs DB extension).
   - Recommendation: Default to app-layer AES-256-GCM with a new `SOURCE_ENC_KEY` env var (matches the `JWT_SECRET` posture, no new infra). Surface as a planner decision.

3. **Does the target content server actually support `Accept-Ranges: bytes`?**
   - What we know: the streaming seam needs validation on a real server (research flag); Phase 10 consumes it.
   - What's unclear: server config of the tester's host.
   - Recommendation: The Range probe (Code Examples) is the phase deliverable; record the result so Phase 10 knows whether resumable downloads are available or it must fall back to full streamed GETs.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| libcurl + mbedTLS (Switch) | index/art fetch, streaming seam | ✓ (already linked) | devkitPro | — |
| nlohmann/json | all JSON parse | ✓ (`lib/json`) | vendored | — |
| doctest + host g++ (MSYS2) | `core/games/` gate | ✓ | per host-doctest-build memory | — |
| devkitPro Switch toolchain | `platform/`/UI compile gate | ✓ | per switch-build-native memory | — |
| A real Tinfoil server | schema + Range validation | ✗ (tester-provided) | — | Captured JSON fixture for parse tests; Range probe deferred to when server is reachable |
| blawar/titledb + eShop CDN | cover-art tier 1 | ✓ (public HTTPS) — confirm at plan time | live | 3-tier fallback (libnx icon → placeholder) |
| nxlink (real Switch) | UI/runtime verification | ✓ (per CLAUDE.md) | — | none — required for runtime confirmation |

**Missing dependencies with no fallback:** A real Tinfoil server for live end-to-end validation (the parser can be fully unit-tested on a captured fixture, but the research flag's "point at a real server" + the Range probe need the tester's host).
**Missing dependencies with fallback:** titledb/eShop CDN — the 3-tier cover-art fallback keeps the catalog fully functional if unavailable.

## Validation Architecture

> `.planning/config.json` not present in the read set; treating `nyquist_validation` as enabled (key absent = enabled).

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest (host) for `core/games/`; Vitest for the API route; on-hardware nxlink for UI/runtime |
| Config file | `tests/Makefile` (host); `api/` Vitest (existing); none for hardware |
| Quick run command | `cd tests && make test` (host doctest) |
| Full suite command | host: `cd tests && make test` · API: `cd api && vitest run` · UI: clean Switch build + nxlink run (CLAUDE.md) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CAT-02 | title_kind/base_title_id derivation | unit | `cd tests && make test` (test_title_id) | ❌ Wave 0 |
| SRC-01 | tolerant index parse (both file shapes, missing keys) | unit | `cd tests && make test` (test_index_parse) | ❌ Wave 0 |
| D-01/CAT-01 | flat files → grouped-by-base catalog | unit | `cd tests && make test` (test_catalog) | ❌ Wave 0 |
| D-04 | sort/filter/search transforms | unit | `cd tests && make test` (test_catalog_view) | ❌ Wave 0 |
| D-07 | recurse bounds + truncation flag + cycle guard | unit | `cd tests && make test` (test_recurse_plan) | ❌ Wave 0 |
| SYNC-01 | source-link sync codec (config only, no secrets in catalog) | unit | `cd tests && make test` (test_source_link) | ❌ Wave 0 |
| SYNC-01/02 | owner-scoped /sources route, JWT, config-only, encrypted-at-rest | integration | `cd api && vitest run` (sources route) | ❌ Wave 0 |
| SRC-02 | custom-header NOT leaked on cross-host redirect | manual/host | host probe or manual redirect test | ❌ Wave 0 (hard to automate on host) |
| CAT-01/SRC-03 | grid renders art/name/size/kind; local SD source | manual (hardware) | nxlink run per CLAUDE.md | n/a (hardware) |
| streaming seam | Range probe: 206 + Accept-Ranges on real server | manual (hardware/network) | nxlink run against real server | n/a |

### Sampling Rate
- **Per task commit:** `cd tests && make test` (the relevant `core/games/` unit).
- **Per wave merge:** full host doctest suite + `cd api && vitest run`.
- **Phase gate:** green host suite + green API suite + on-hardware nxlink confirmation (catalog renders, source links, syncs) before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tests/test_title_id.cpp` — CAT-02 (base/update/DLC + base mask)
- [ ] `tests/test_index_parse.cpp` — SRC-01 (+ real-server JSON fixture)
- [ ] `tests/test_catalog.cpp` — D-01 grouping
- [ ] `tests/test_catalog_view.cpp` — D-04 sort/filter/search
- [ ] `tests/test_recurse_plan.cpp` — D-07 bounds
- [ ] `tests/test_source_link.cpp` — SYNC-01 codec
- [ ] Add new `core/games/*.cpp` + needed `platform/games/*.cpp` to `tests/Makefile` SRCS (mirror how cheat_store/app_settings are listed)
- [ ] API: `sources` route Vitest spec (mirror existing saves route tests)
- [ ] Captured real Tinfoil index JSON fixture (tester-provided)

## Security Domain

> `security_enforcement` not found in the read set; treating as enabled.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | yes (API) | Existing JWT (`app.authenticate`) — reuse, do not re-implement |
| V3 Session Management | yes (API) | Existing access/refresh-token scheme + revocation (`plugins/auth.ts`) |
| V4 Access Control | yes | Owner-scoping: every Prisma query `where:{userId}`; `@@index([userId])`; 401 when `userIdFromRequest` is empty (SYNC-02) |
| V5 Input Validation | yes | zod on the route body (mirror `savePutFieldsSchema`); tolerant+bounded index parse on device; cap JSON size |
| V6 Cryptography | yes | At-rest credential encryption (AES-256-GCM via node:crypto OR pgcrypto) — never hand-roll; fail-closed TLS via `apply_curl_tls` |
| V7/V9 Comms | yes | Fail-closed TLS (`apply_curl_tls`, `tls_policy`); strip `Authorization` on cross-host redirect (curl default); guard custom-header redirect leak |

### Known Threat Patterns for {Switch client + Tinfoil index + cloud config sync}
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Custom auth header leaked to redirect host (SRC-02) | Information Disclosure | Same-host check before re-attaching custom header; never `CURLOPT_UNRESTRICTED_AUTH` (Pitfall 4) |
| MITM serving forged index/art over downgraded TLS | Tampering / Spoofing | Fail-closed TLS already enforced (`tls_policy` default Verify); keep it for index + art |
| Pathological/huge index → console OOM/DoS | Denial of Service | Cap index/art size (Pitfall 2); recurse bounds (Pitfall 3) |
| Credential leak in logs | Information Disclosure | Keep creds out of `brls::Logger`; API pino redacts `authorization`/`cookie` (`app.ts`) (Pitfall 7) |
| Cross-account read of synced source config | Elevation / Info Disclosure | Owner-scoping `where:{userId}` + `@@unique`/`@@index([userId])` (SYNC-02) |
| Credentials readable at rest in cloud DB | Information Disclosure | Encrypt the credential column at rest; config-only model (no content) |
| Credentials readable on SD (plaintext) | Information Disclosure | Accepted limitation (SD is plaintext); minimize exposure, keep out of logs/synced catalog; E2E deferred |
| Default/bundled source enabling content access (policy/legal) | — (policy) | SRC-04: empty source list, nothing bundled (Pitfall 6) |

## Sources

### Primary (HIGH confidence)
- `source/platform/http_client_curl.cpp`, `http_client.hpp`, `curl_tls.hpp`, `tls_policy.hpp` — HTTP/TLS seam + buffering OOM + cancel hook
- `source/platform/saves/http_cloud_save_client.{cpp,hpp}`, `api/src/routes/saves.ts`, `api/src/app.ts`, `api/src/plugins/auth.ts`, `api/prisma/schema.prisma` — the cloud-sync trio to mirror (SYNC-01/02)
- `source/platform/cheat_store.cpp` (`read_text_file`/`write_text_file`/`dir_*`), `source/platform/app_settings.cpp` — SD cache + config persistence patterns
- `source/platform/title_service_switch.cpp`, `source/platform/title.hpp` — installed-title icon (D-02 tier 2)
- `source/app/theme_browser_activity.cpp`, `resources/xml/activity/home.xml`, `source/app/home_activity.cpp` — grid Activity + Home card patterns
- `tests/Makefile`, `tests/test_cheat_db.cpp`, `source/core/saves/{cloud_save_json.hpp,save_sync.cpp}` — `core/` doctest pattern
- switchbrew Title list (program-ID structure) — title-ID bitmask (CAT-02), corroborated by NX_Game_Info

### Secondary (MEDIUM confidence)
- blawar.github.io/tinfoil/custom_index (+ github mirror) — Tinfoil index JSON schema (community doc, not formal spec)
- github.com/blawar/titledb — regional cover-art JSON (`iconUrl` → eShop CDN); no per-title endpoint
- curl.se CURLOPT_UNRESTRICTED_AUTH / CURLOPT_FOLLOWLOCATION docs — redirect Authorization stripping (default) + custom-header non-stripping

### Tertiary (LOW confidence)
- GBAtemp / community threads on title-ID derivation and titledb usage — used only to cross-check the bitmask consensus (verified against switchbrew)

## Metadata

**Confidence breakdown:**
- Standard stack (reuse, no new deps): HIGH — every facility verified in-tree
- Title-ID bitmask (CAT-02): HIGH — switchbrew + multiple tools agree
- Cloud sync pattern (SYNC-01/02): HIGH — near-mechanical clone of an existing trio
- Catalog grid / Activities: HIGH — direct structural clone of `theme_browser_activity`
- Tinfoil index schema (SRC-01): MEDIUM — community-documented; must validate on a real server (research flag)
- Cover-art source (D-02): MEDIUM — titledb is public but has availability/licensing risk; 3-tier fallback mitigates
- Recurse bounds (D-07) / size caps: MEDIUM — judgment values, tunable
- Streaming/Range seam: MEDIUM — libcurl mechanics are HIGH; the real-server `Accept-Ranges` validation is pending hardware/network

**Research date:** 2026-06-06
**Valid until:** ~2026-07-06 for codebase analogs (stable); ~2026-06-20 for the Tinfoil/titledb external resources (community-maintained, can move) — re-verify reachability at plan time.
