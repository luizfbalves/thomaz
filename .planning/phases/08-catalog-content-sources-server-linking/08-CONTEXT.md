# Phase 8: Catalog, Content Sources & Server Linking - Context

**Gathered:** 2026-06-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 8 delivers the **read-only, zero-install foundation** of the v1.2 game-management
pillar. A user can:

- **Link a content source** they own — a server URL returning a Tinfoil-style JSON index
  (`files[{url,size}]`, `directories`), including auth-gated servers (basic-auth-in-URL,
  custom header, or referrer gate) — or pick **local NSP/NSZ files on the SD card**.
- **Browse the source as a catalog**: a cover-art grid grouped by base title, each entry
  tagged base/update/DLC by deriving the kind from the 64-bit title ID, with search, sort,
  and content filters.
- **One-tap sync the server-link configuration** to their thomaz cloud account and have it
  restored on another console signed into the same account — **config only, never content**.

This phase also establishes the **streaming/Range HTTP seam** that Phase 10's install engine
will consume (the existing curl client buffers whole responses in a `std::string` and would
OOM on multi-GB titles).

**Explicitly NOT in this phase:** no NCM writes, no downloads-to-install, no install queue,
no uninstall, no installed-title management, no update/DLC detection for *installed* titles.
Those are Phases 9–11.

**Requirements covered:** SRC-01, SRC-02, SRC-03, SRC-04, CAT-01, CAT-02, SYNC-01, SYNC-02
(see `.planning/REQUIREMENTS.md`).

</domain>

<decisions>
## Implementation Decisions

### Catalog look & cover art (discussed)
- **D-01 — Grouped by base title.** Present the flat Tinfoil file list as one cover-art card
  *per base game* (mask the low title-ID bits to derive the base title ID). Updates and DLC for
  a title are nested in that title's **detail view**, not shown as separate top-level cards.
  Requires title-ID grouping + base/update/DLC kind-derivation logic in **pure `core/games/`**
  (host-doctest-gated).
- **D-02 — Cover art: online → local → placeholder (3-tier).** For a catalog entry, try an
  **online titledb** (map the 64-bit title ID → cover/icon via a title-metadata endpoint)
  first; if unreachable/missing, fall back to the **installed libnx icon** (when that title is
  already installed); else a **name+kind placeholder**. Art is cached on SD. The art source is
  **metadata only — never content** (does not change the "thomaz hosts no content" posture).
  **Open item for planning:** the exact titledb/icon endpoint is unconfirmed — default to a
  public title-ID→icon source; confirm at plan time. Keep fail-closed TLS for it like every
  other network call.
- **D-03 — Art grid default, compact list toggle.** Default to a cover-art grid (model the
  existing theme/mod browser grids); provide a button to switch to a compact text list for
  fast scanning/searching of large catalogs.
- **D-04 — Search + sort + content filter.** Live text search by **name and title ID**, sort by
  **name / size**, and content-filter chips: **has update / has DLC / base-only** — all derived
  from what the index offers per grouped title. No installed-state cross-reference in this phase
  (update-available badges for *installed* titles are Phase 11).

### Catalog freshness / fetch (discussed)
- **D-05 — Per-source SD cache + auto-refresh-if-stale.** On open, render the cached index
  **instantly** from SD (per source), then refresh in the background. Manual **Refresh** is
  always available. Mirrors the existing cheat-db on-SD index cache pattern.
- **D-06 — Refresh aggressiveness: every open when online.** On each open, paint the cache for
  an instant first frame, then **always** kick a background refresh **if online**. When offline,
  show the cached index (with an offline/stale note). Cache primarily serves instant render +
  offline browse.
- **D-07 — Nested `directories`: recurse + flatten, bounded.** Follow sub-directory sub-indexes,
  fetch each, and **merge everything into the one unified grouped catalog** (consistent with
  D-01, not a folder browser). Enforce a **safety bound** — max recursion depth + max total
  entries/requests — and surface a note if the bound is hit (protects a console against a
  pathological/huge server).

### Claude's Discretion
The user opted **not** to discuss these two areas and delegated them to sensible defaults.
Planner/researcher may refine, but these are the working assumptions:

- **Source management & navigation (discretion).**
  - Entry point: add a new **"Games" (or "Content") card** to the Home screen, alongside the
    existing `themesCard / savesCard / modsCard / cheatsCard / systemCard` pattern.
  - Model: a **list of multiple linkable sources** (not a single active server). The source list
    is **empty by default** (SRC-04 — nothing bundled/enabled). Add-source flow = URL entry +
    optional auth fields (basic-auth-in-URL / custom header / referrer).
  - **Local SD** appears as a **peer "source"** in the same list (a pseudo-source), browsed
    through the **same catalog/detail surface** as remote servers (SRC-03). USB-HDD is out of
    scope (deferred → GAME-F02).
- **Credential security (discretion), satisfying SYNC-02 "protected at rest."**
  - On device: store source config (incl. credentials) under the existing `app_settings`
    config location (`/switch/thomaz/config/...`). Note the SD card is plaintext-readable;
    at minimum keep credentials out of logs and out of any synced *catalog* data.
  - In cloud: **owner-scoped** record (one Prisma model + one route, mirroring the cloud-saves
    `SaveSlot` + `api/routes/saves` + JWT pattern), with credentials **encrypted at rest**
    server-side. **Config only — never catalog or content bytes** (SYNC-01). Full E2E
    (device-held key) is **not** required for MVP but may be noted as a future hardening item.
  - Planner should choose the concrete at-rest scheme (server-side encrypted column vs.
    app-layer encryption) during planning; either satisfies SYNC-02.

### Locked upstream constraints (do NOT re-litigate — from PROJECT.md / ROADMAP / research)
- **No default source/index/keys bundled or enabled** — source list starts empty (SRC-04).
- **Cloud API stores config only, never content** — enforce in the Prisma model + route schema
  (SYNC-01). Content hosting/proxying via the thomaz API is an explicit anti-pattern.
- **Content bytes NEVER flow through `IHttpClient::request` / `HttpResponse.body`** (OOM on
  multi-GB titles). Establish a **streaming/Range** seam in this phase; validate
  `Accept-Ranges: bytes` + a streaming/Range fallback against a real target server. This seam is
  consumed by Phase 10.
- **Fail-closed TLS**; **strip `Authorization` on cross-host redirects**; **cap the index JSON
  parse size**.
- **Encrypted-index ("encrypted shop") Tinfoil variant → out of scope for MVP.**
- New decidable logic lives in pure **`core/games/`** under the doctest gate; **`platform/games/`**
  stays thin Switch orchestration; no exceptions — return-value error handling.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` § "Phase 8: Catalog, Content Sources & Server Linking" — goal, 5
  success criteria, research flag (Tinfoil index is community-reverse-engineered; streaming
  seam; strip Authorization on redirects).
- `.planning/REQUIREMENTS.md` — SRC-01..04, CAT-01/02, SYNC-01/02 (and the v1.2 Out-of-Scope
  table: no bundled server, no content hosting, no keys, XCI/USB deferred).
- `.planning/PROJECT.md` — milestone framing, legal/responsibility boundary, Key Decisions
  table, Constraints (verification gates, clean-architecture rule).
- `.planning/STATE.md` § "Blockers/Concerns" → Phase 8 — carried research pitfalls
  (Tinfoil schema, TLS, redirect Authorization stripping, JSON size cap, streaming seam).

### Codebase patterns to reuse / mirror
- `.planning/codebase/ARCHITECTURE.md` — clean-architecture layering (app → platform → core),
  `IHttpClient` abstraction, error-handling strategy (return values, no exceptions), the
  anti-patterns ("no business logic in platform layer", "no direct platform calls from
  Activity").
- `.planning/codebase/INTEGRATIONS.md` — Thomaz API base URL + JWT auth scheme, the SD-card
  index cache pattern (cheat-db), Prisma schema models.
- `source/platform/http_client_curl.{h,cpp}` — the existing curl client (buffers whole response;
  the streaming/Range seam extends from here).
- `source/platform/saves/{sync_store,http_cloud_save_client}.cpp` + `api/src/routes/saves.ts`
  + `api/prisma/schema.prisma` (`SaveSlot`) — the cloud-sync pattern to mirror for SYNC-01/02.
- `source/app/theme_browser_activity.cpp`, `source/app/mod_browser_activity.cpp`,
  `source/app/game_list_activity.cpp` — existing cover-art / thumbnail grid + remote-browse UI
  to model the catalog grid on.
- `source/platform/title_service_switch.{cpp,hpp}` + `source/platform/title.hpp`
  (`InstalledTitle`) — installed-title + libnx icon access for the local-icon fallback (D-02).
- `source/platform/app_settings.cpp` — JSON/text config on SD under `/switch/thomaz/config/`;
  where source-list + credentials persist on device.
- `resources/xml/activity/home.xml` + `source/app/home_activity.cpp` — Home card pattern for the
  new "Games" entry point.

### No external specs
- The Tinfoil JSON index format is **community-reverse-engineered, not formally specified** —
  there is no authoritative doc to cite. Researcher must point the parser at a **real user
  server early** and validate against it. The titledb/icon source for D-02 is likewise
  unconfirmed and to be selected during research/planning.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Cover-art / remote-browse grid UI** — `theme_browser_activity` / `mod_browser_activity`
  render remote thumbnail grids; `game_list_activity` renders an installed-title icon grid.
  The catalog grid (D-03) should model these rather than inventing a new widget.
- **`IHttpClient` / `HttpClientCurl`** — index fetch (capped) and the art fetch use this;
  the **streaming/Range** extension for content (Phase 10) starts from `http_client_curl`.
- **Cloud-sync trio** — `sync_store` + `http_cloud_save_client` + `api/routes/saves` + Prisma
  `SaveSlot`: the exact owner-scoped, JWT-authed sync pattern to mirror for SYNC-01/02
  (one new Prisma model + one new route, config-only).
- **`app_settings`** — on-SD JSON/text config under `/switch/thomaz/config/`; persists the
  source list + credentials on device.
- **cheat-db SD index cache** — established precedent for caching a multi-MB JSON index on SD
  (D-05/D-06 caching strategy).
- **`title_service_switch` / `InstalledTitle`** — libnx installed-title icons for the D-02
  local-icon fallback.

### Established Patterns
- **Clean architecture:** all parsing/derivation (index parse, title-ID→kind, base-title
  grouping, recurse/flatten, sort/filter, server-link codec) goes in pure **`core/games/`**
  (doctest-gated); **`platform/games/`** is thin Switch orchestration. Activities own no
  business logic.
- **Error handling:** return-value/status-enum based; no exceptions in core/platform. Activities
  show Borealis dialogs on failure.
- **Auth:** JWT Bearer via the existing `auth_store` token; reuse for the SYNC route.
- **Verification gates:** host doctest suite (`tests/Makefile`) is the in-phase gate for all
  `core/games/` logic; the Switch build (devkitPro) is the compile gate for `platform/`/UI.

### Integration Points
- **Home screen** — new "Games"/"Content" card in `home.xml` + `home_activity.cpp`.
- **API** — exactly one new Prisma model + one new Fastify route under `api/src/routes/`
  (config only), wired through the existing `authenticate` plugin.
- **HTTP** — `http_client_curl` gains the streaming/Range capability (seam for Phase 10).

</code_context>

<specifics>
## Specific Ideas

- The cover-art catalog is the **headline / core value** ("smoother, cover-art-rich than stock
  installers") — the user prioritized making art appear for *every* catalog entry (even
  not-installed) via the online-titledb path, accepting the extra moving parts.
- Grouped-by-base-title with updates/DLC nested in the detail view (preview the user approved):
  a grid of one card per game; the detail lists Base / Update vN / DLC rows with sizes.
- Catalog must feel **instant** on open (cache-first paint) while staying fresh (always-refresh
  when online) — the user chose the most-fresh option over minimizing re-downloads.

</specifics>

<deferred>
## Deferred Ideas

- **Full E2E credential encryption** (device-held key so the cloud can't read credentials) —
  not required for SYNC-02 MVP; possible future hardening.
- **USB-HDD mass-storage source** — out of scope for v1.2 (GAME-F02).
- **XCI/XCZ (gamecard image) sources** — out of scope for v1.2 (GAME-F01).
- **Encrypted-index ("encrypted shop") Tinfoil variant** — out of scope for MVP per research flag.
- **Update/DLC "available" badges for installed titles** — belongs in Phase 11 (needs
  installed-state cross-reference + ncm/ns); Phase 8 only knows what the server offers.

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 8-Catalog, Content Sources & Server Linking*
*Context gathered: 2026-06-06*
