# Architecture Research

**Domain:** Switch game-management client (Tinfoil-style installer) integrated into an existing clean-architecture homebrew hub + cloud API
**Researched:** 2026-06-06
**Confidence:** HIGH (existing codebase patterns read directly from source; install pipeline + index schema verified against libnx headers, Tinfoil docs, and Goldleaf reference implementation)

> **Scope note for the roadmapper:** This is a *subsequent-milestone integration* study, not a greenfield architecture. The existing layering (`core/` pure + host-tested, `platform/` thin Switch orchestration, Borealis `*Activity` on a `ThomazActivity`/`runAsync` base, Fastify+Prisma API) is **reused as-is**. Below, every component is tagged **NEW** (build it), **MODIFIED** (extend an existing file), or **REUSE** (call it unchanged). The hard architectural decisions are: (1) where the install pipeline lives so it stays cancelable and resumable, (2) where the download/install **queue state** lives, and (3) the API's role — **config sync only, never content**.

---

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│  app/ (Borealis activities — UI thread; ThomazActivity + runAsync)         │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────────┐  │
│  │ GameStore    │ │ Catalog      │ │ InstalledMgr │ │ DownloadQueue    │  │
│  │ Activity NEW │ │ Detail   NEW │ │ Activity NEW │ │ Activity     NEW │  │
│  │ (browse cat) │ │ (base/upd/   │ │ (installed + │ │ (progress/cancel │  │
│  │              │ │  DLC + queue)│ │  free space) │ │  /resume list)   │  │
│  └──────┬───────┘ └──────┬───────┘ └──────┬───────┘ └────────┬─────────┘  │
│         │                │                │                   │            │
│         └────────────────┴───────┬────────┴───────────────────┘            │
│                                   │ (runAsync workers; copy data, no `this`)│
├───────────────────────────────────┼────────────────────────────────────────┤
│  platform/games/ (NEW dir — thin Switch orchestration, return-value errors) │
│  ┌─────────────────────┐ ┌────────────────────────┐ ┌───────────────────┐  │
│  │ content_source      │ │ nsp_installer_switch    │ │ install_queue     │  │
│  │  - remote (curl)REUSE│ │  (libnx ncm + ns)   NEW │ │  _runner      NEW │  │
│  │  - local (fs)    NEW │ │  PFS0→NCA→placeholder→  │ │ (drives queue,    │  │
│  │ catalog_fetch    NEW │ │  register→meta→record   │ │  one job at a time│  │
│  │ installed_query  NEW │ │  CANCELABLE/RESUMABLE   │ │  off UI thread)   │  │
│  └──────────┬──────────┘ └───────────┬────────────┘ └─────────┬─────────┘  │
│             │                        │                         │            │
│  REUSE: http_client_curl, fs_util, title_service_switch, http_auth_client   │
├─────────────┼────────────────────────┼─────────────────────────┼──────────┤
│  core/games/ (NEW dir — pure, host-tested via doctest, no libnx, no curl)   │
│  ┌─────────────────┐ ┌────────────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │ catalog_model + │ │ install_planner│ │ queue_state  │ │ update_dlc   │  │
│  │ index_parser NEW│ │ (what NCAs, NEW│ │ machine  NEW │ │ _diff    NEW │  │
│  │ (Tinfoil JSON)  │ │  dest storage) │ │ (job states, │ │ (installed   │  │
│  │                 │ │ pfs0_parse NEW │ │  transitions)│ │  vs catalog) │  │
│  └─────────────────┘ └────────────────┘ └──────────────┘ └──────────────┘  │
├─────────────────────────────────────────────────────────────────────────── ┤
│  Persistence (client-side)        │  Cloud API (Fastify + Prisma + Postgres) │
│  ┌───────────────────────────┐    │  ┌──────────────────────────────────┐   │
│  │ queue journal (SD JSON)NEW│    │  │ /content-servers routes      NEW │   │
│  │ server-link config (SD)NEW│◄──►│  │ ContentServer Prisma model   NEW │   │
│  └───────────────────────────┘ one│  │  *** CONFIG ONLY — NO CONTENT *** │   │
│                            -tap sync └──────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────── ┘
```

### Component Responsibilities

| Component | Layer | New/Mod/Reuse | Responsibility |
|-----------|-------|---------------|----------------|
| `index_parser` | core/games | **NEW** | Parse Tinfoil-style JSON index (`files[].url/.size`, `directories[]`, `titledb{}`, `headers[]`, `success`, `referrer`) into a `Catalog` model. Pure string→struct. |
| `catalog_model` | core/games | **NEW** | `CatalogEntry { title_id, kind(Base/Update/DLC), version, url, size, name, icon_url }`; derive title_id + kind from filename `[0100...][v131072]` tags or `titledb`. |
| `pfs0_parse` | core/games | **NEW** | Parse PFS0/NSP header → list of `{name, offset, size}` NCA/cnmt/tik entries from a byte range. Pure; host-testable against fixtures. |
| `install_planner` | core/games | **NEW** | Given parsed NSP entries + target storage (NAND `BuiltInUser` vs SD `SdCard`), produce an ordered `InstallPlan` (which NCAs, content meta entry, ticket) and free-space requirement. Pure. |
| `update_dlc_diff` | core/games | **NEW** | Diff installed titles (versions, owned DLC) against catalog entries → "update available", "new DLC". Pure; mirrors existing `update.cpp`/cheat-version-resolution style. |
| `queue_state` | core/games | **NEW** | The job **state machine**: `Queued→Downloading→Installing→Done`/`Failed`/`Canceled`/`Paused`, with serialize/deserialize for the journal. Pure (mirrors `save_sync_state.cpp`). |
| `content_source` | platform/games | **NEW (remote=REUSE curl)** | Abstraction `IContentSource { listCatalog(); openRange(url, offset) }`. Remote impl wraps `http_client_curl` (Range requests + injected `cancelled`); local impl reads SD/USB files via `fs_util`. |
| `catalog_fetch` | platform/games | **NEW** | Fetch raw index over curl (with optional `headers`/basic-auth from config), hand bytes to `core::index_parser`. |
| `nsp_installer_switch` | platform/games | **NEW** | The libnx pipeline: `ncmContentStorageCreatePlaceHolder → WritePlaceHolder (chunked) → Register → ncmContentMetaDatabaseSet/Commit → nsPushApplicationRecord`. Chunked write loop is the **cancel + resume** point. `#ifdef __SWITCH__`. |
| `installed_query` | platform/games | **MODIFIED (extends title_service)** | Installed bases + **updates + DLC + versions**, plus NAND/SD free space (`nsCountApplicationContentMeta`, `nsListApplicationContentMetaStatus`, `ncmContentMetaDatabaseListContentMeta`, fs free-space). Builds on existing `title_service_switch.cpp`. |
| `install_queue_runner` | platform/games | **NEW** | Owns the queue at app scope; runs one job at a time off the UI thread; threads `cancelled`; checkpoints to the journal between/within steps. |
| `*Activity` set | app/ | **NEW (×4)** | Store/browse, catalog detail, installed manager, queue screen — all extend `ThomazActivity`. |
| `content_server config sync` | platform + api | **NEW** | One-tap push/pull of the server-link config to the user's cloud account. **Config only.** |

---

## Recommended Project Structure

New directories mirror the existing per-domain split (`core/<domain>` + `platform/<domain>` + `app/*_activity`). Nothing else moves.

```
source/
├── core/
│   └── games/                      # NEW — pure, host-tested (doctest), no libnx/curl
│       ├── catalog_model.hpp       # CatalogEntry, ContentKind, Catalog
│       ├── index_parser.cpp/.hpp   # Tinfoil JSON index -> Catalog (nlohmann, non-throwing)
│       ├── filename_tags.cpp/.hpp  # extract [titleid]/[vXXXX] from filenames (titledb fallback)
│       ├── pfs0_parse.cpp/.hpp     # NSP/PFS0 header -> entry table (offset/size/name)
│       ├── install_planner.cpp/.hpp# entries + storage target -> InstallPlan + free-space need
│       ├── update_dlc_diff.cpp/.hpp# installed vs catalog -> available updates/DLC
│       ├── queue_state.cpp/.hpp    # job state machine + journal (de)serialize
│       └── server_link.cpp/.hpp    # ServerLinkConfig model + JSON codec (synced to cloud)
├── platform/
│   └── games/                      # NEW — thin Switch orchestration, return-value errors
│       ├── content_source.hpp      # IContentSource (listCatalog / openRange)
│       ├── remote_content_source.cpp   # wraps http_client_curl (+ Range, + auth headers)
│       ├── local_content_source.cpp    # SD/USB file reader via fs_util
│       ├── catalog_fetch.cpp/.hpp  # fetch index bytes -> core::index_parser
│       ├── nsp_installer_switch.cpp/.hpp   # libnx ncm+ns install pipeline (#ifdef __SWITCH__)
│       ├── installed_query_switch.cpp/.hpp # installed updates/DLC + free space (extends title svc)
│       ├── install_queue_runner.cpp/.hpp   # app-scope queue owner + runner thread
│       ├── queue_journal.cpp/.hpp  # journal read/write on SD (uses fs_util)
│       └── server_link_store.cpp/.hpp      # local config persistence + cloud sync client
└── app/
    ├── game_store_activity.cpp/.hpp        # NEW — browse linked catalog (cover-art grid)
    ├── catalog_detail_activity.cpp/.hpp    # NEW — base/update/DLC actions -> enqueue
    ├── installed_manager_activity.cpp/.hpp # NEW — installed list, versions, DLC, free space, uninstall
    ├── download_queue_activity.cpp/.hpp    # NEW — progress/cancel/resume per job
    └── server_link_activity.cpp/.hpp       # NEW — add/edit server + one-tap "sync to account"
api/
├── prisma/schema.prisma            # MODIFIED — add ContentServer model
└── src/routes/content-servers.ts   # NEW — CRUD for server-link config (auth-gated, config only)
tests/                              # MODIFIED — register new core/games doctest TUs
resources/i18n/                     # MODIFIED — pt-BR/en-US strings + new activity XML
```

### Structure Rationale

- **`core/games/`:** Everything decidable without hardware — index parsing, PFS0 layout, install planning, the queue state machine, update/DLC diffing — goes here so it is covered by the doctest gate (the project's primary automated gate). This is the largest *de-risking* lever: the install **decision logic** is testable on the host even though the install **execution** (libnx) is not.
- **`platform/games/`:** Stays thin — it only performs IO the core layer planned: curl byte ranges, file reads, libnx `ncm`/`ns` calls, SD journal writes. No business logic, matching the existing `mod_actions.cpp` / `title_service_switch.cpp` discipline.
- **Per-domain folders, not a monolith:** Matches the five existing pillars (`cheats`, `mods`, `themes`, `saves`, `sysmod`). The roadmapper can phase this folder-by-folder.
- **API addition is one model + one route file:** Deliberately minimal — the API's only new job is storing a small config row per user.

---

## Architectural Patterns

### Pattern 1: Two content sources behind one interface (remote curl / local file)

**What:** `IContentSource` exposes `listCatalog()` and `openRange(url_or_path, offset, len)`. The remote impl issues HTTP Range requests via the **existing** `http_client_curl` (adding `Range:` headers + config auth headers + the `cancelled` flag); the local impl `pread`s a file on SD/USB. The installer and queue runner consume bytes through this interface and never know the origin.

**When to use:** Both install paths (remote-server install and local-file install) go through the same planner + installer. Only the byte-fetch differs.

**Trade-offs:** One abstraction adds a vtable hop, but it collapses two install flows into one pipeline and one set of tests. Resumability requires the source support offset reads — trivial for local files, and `Range:` for HTTP (verify the user's server honors `Accept-Ranges: bytes`; fall back to restart-from-zero if not).

**Example:**
```cpp
// platform/games/content_source.hpp
struct ContentChunk { const uint8_t* data; size_t len; bool eof; };
class IContentSource {
  public:
    virtual ~IContentSource() = default;
    virtual std::optional<core::Catalog> listCatalog(std::string* err) = 0;
    // Reads up to `cap` bytes at `offset`; remote = HTTP Range, local = pread.
    virtual long readRange(const std::string& locator, uint64_t offset,
                           uint8_t* buf, size_t cap,
                           std::shared_ptr<std::atomic<bool>> cancelled) = 0;
};
```

### Pattern 2: NSP install pipeline as a resumable, cancelable chunked loop (libnx ncm + ns)

**What:** The Switch NSP install is the established Goldleaf/Tinfoil pipeline, expressed against libnx `ncm`/`ns`:
1. `core::pfs0_parse` the NSP header → entry table (NCAs, `*.cnmt.nca`, ticket).
2. `core::install_planner` picks target `NcmStorageId` (SdCard or BuiltInUser) and the install order.
3. For each NCA: `ncmContentStorageCreatePlaceHolder` → **loop** `readRange()` from the content source and `ncmContentStorageWritePlaceHolder(offset, chunk)` → `ncmContentStorageRegister`.
4. Build the content-meta record from the cnmt and `ncmContentMetaDatabaseSet` + `Commit`.
5. `nsPushApplicationRecord` so the title appears on the home menu.

The **per-chunk write loop in step 3 is the single cancel + resume point**: check `cancelled->load()` each chunk (mirrors `mod_download.cpp`'s xferinfo abort); persist `{ncaId, bytesWritten}` to the journal so an interrupted NCA resumes mid-placeholder.

**When to use:** All installs (base/update/DLC), both sources.

**Trade-offs:** This is the genuinely new, hardware-only, highest-risk code — it cannot run under the host doctest gate, so the surrounding *decisions* (planner, pfs0_parse, queue_state) must be pushed into `core/` and tested there, leaving `nsp_installer_switch.cpp` as a thin, auditable orchestration. Installing unsigned/foreign content is the user's responsibility (matches the milestone's framing); thomaz performs the mechanism, not validation of entitlement.

**Example:**
```cpp
// platform/games/nsp_installer_switch.cpp  (#ifdef __SWITCH__)
for (auto off = job.resumeOffset; off < nca.size; off += kChunk) {
    if (cancelled->load()) return InstallOutcome::Canceled;        // CONC-03 style
    long n = source.readRange(nca.locator, nca.fileOffset + off, buf, kChunk, cancelled);
    if (n <= 0) return InstallOutcome::TransportError;
    if (R_FAILED(ncmContentStorageWritePlaceHolder(&cs, &phId, off, buf, n)))
        return InstallOutcome::WriteError;
    journal.checkpoint(job.id, nca.id, off + n);                    // resume point
    progress(off + n, nca.size);
}
```

### Pattern 3: App-scoped queue runner + on-SD journal (state lives below the UI)

**What:** The download/install **queue state does NOT live in any activity** — activities are popped/destroyed freely and must not own long-running work (the whole point of the `ThomazActivity`/`alive` design). Instead:
- A single `InstallQueueRunner` is owned at **app scope** (created in `main()` alongside the `IHttpClient`, like the existing app-lifetime HTTP client), runs **one job at a time** on its own worker, and is the source of truth for in-memory job state.
- The **journal** (`core::queue_state` serialized to an SD JSON file via `queue_journal`) is the durable source of truth, checkpointed at each state transition and each NCA chunk. On app launch the runner rehydrates from the journal → in-progress jobs resume, finished jobs are pruned.
- Activities are **views/controllers**: they enqueue jobs, subscribe to runner callbacks (delivered to the UI thread via `brls::sync`), and render progress. `DownloadQueueActivity` shows the live list; closing it does not stop the runner.

**When to use:** This is the core integration decision for "how does the resumable queue fit the runAsync model." `runAsync` is for *activity-scoped* one-shot async (fetch a catalog, resolve files) and is wrong for *app-scoped, survives-navigation* work. The queue runner is the complement: it owns the long task; `runAsync`-style activities observe it.

**Trade-offs:** A second long-lived async owner beyond `runAsync` — must be documented (like `cloudBusy`'s atomic contract) so future edits don't accidentally tie a job's lifetime to an activity. Single-job serialization keeps NAND/`ncm` writes safe and progress legible; parallel installs are an explicit anti-feature (see below). The runner posts UI updates only through `brls::sync` + an `alive`-guarded subscriber list so a popped activity never receives a callback into freed memory (reuse `core::run_if_alive`).

**Example:**
```cpp
// platform/games/install_queue_runner.hpp — owned by main(), app lifetime
class InstallQueueRunner {
  public:
    void enqueue(core::QueueJob job);          // appends + journals + wakes runner
    void cancel(std::string jobId);            // flips that job's cancelled flag
    void subscribe(std::shared_ptr<std::atomic<bool>> alive,
                   std::function<void(const core::QueueSnapshot&)> onChange);
  private:
    std::thread worker_;                       // drains one job at a time
    std::mutex mtx_;                           // guards queue + subscribers
    core::QueueState state_;                   // pure model; persisted via queue_journal
};
```

### Pattern 4: One-tap config sync — API stores config, never content (mirror cloud-saves auth flow)

**What:** The server-link config (`ServerLinkConfig { name, url, authType, username, secret? }`) is persisted locally on SD (`server_link_store`) and, on one tap, synced to the cloud account via a new `/content-servers` route, reusing the **existing** `HttpAuthClient` Bearer token. This is structurally identical to how cloud saves sync metadata (`http_cloud_save_client.cpp` → `/saves`), minus the blob. **No NSP/NCA/catalog bytes ever touch the API** — only the small config record. Roadmap MUST keep it that way: the API is a config address book, not a CDN.

**When to use:** "Link once, available on every console signed into the account."

**Trade-offs:** Storing a server URL + optional credentials server-side is a small secret-at-rest surface. Recommend: store the secret encrypted (or store only non-secret link + let the client hold the credential), and **never** add a proxy/download endpoint. The contract must be explicit in the route and the Prisma comment: config only.

**Example (API):**
```prisma
/// User-linked content server CONFIG ONLY. The API never stores or proxies game content.
model ContentServer {
  id        String   @id @default(cuid())
  userId    String
  user      User     @relation(fields: [userId], references: [id], onDelete: Cascade)
  name      String
  url        String          // index/base URL the CLIENT fetches from directly
  authType   String  @default("none") // "none" | "basic" | "header"
  authSecret String?         // optional, encrypted at rest; NEVER game content
  createdAt  DateTime @default(now())
  updatedAt  DateTime @updatedAt
  @@index([userId])
}
```

---

## Data Flow

### Remote-server install flow

```
ServerLinkActivity (link + one-tap sync) ──► /content-servers PUT (cloud, REUSE Bearer)
        │
GameStoreActivity.onContentAvailable
        │ runAsync worker:
        ▼
remote_content_source.listCatalog ──curl GET index──► core::index_parser ──► Catalog
        │ (onSync) cover-art grid
        ▼
CatalogDetailActivity: user taps "Install base / update / DLC"
        │ enqueue(QueueJob{entry})
        ▼
InstallQueueRunner (app scope, 1 job at a time)
        │  Downloading: readRange(HTTP Range) ─┐ chunked, cancelable, journaled
        │  Installing : pfs0_parse ─► install_planner ─► nsp_installer_switch
        │                 ncm placeholder→write(chunk loop = resume pt)→register
        │                 →content-meta DB→nsPushApplicationRecord
        ▼  checkpoints ──► queue_journal (SD JSON)  ◄── rehydrate on next launch
DownloadQueueActivity subscribes ──brls::sync (alive-guarded)──► progress/cancel UI
```

### Local-file install flow

Identical from `enqueue` onward — only the source differs:
```
InstalledManager/file-picker ─► local_content_source (SD/USB pread)
        └─► SAME planner ─► SAME nsp_installer_switch ─► SAME journal
```

### Update/DLC auto-detect flow

```
installed_query_switch (versions + owned DLC + free space)
        │
        └─► core::update_dlc_diff(installed, catalog) ─► badges:
             "Update v1.0.2 available", "3 new DLC"  (rendered in store/installed views)
```

### State management

```
queue_journal (SD JSON)  ← durable truth, checkpointed every transition + every NCA chunk
        ▲ rehydrate on launch / persist on change
InstallQueueRunner.state_ (in-memory core::QueueState) ← live truth, 1 worker
        │ subscribe + brls::sync (alive-guarded)
Activities (views only — never own job lifetime)
```

---

## Scaling Considerations

This is a single-console client; "scale" means catalog size, library size, and download size — not concurrent users.

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Small catalog (<200 entries) / small library | Parse whole index in memory; list installed eagerly (existing `listInstalled` is fine). |
| Large catalog (1000s) / large library | Lazy-load cover-art (already a known roadmap item for game list); paginate the parsed catalog in the grid; stream-parse the index rather than buffering the whole JSON if indexes get large. Cache the parsed catalog + titledb to SD to avoid re-fetch on every open. |
| Many queued / huge installs | Keep single-job serialization (NAND safety); journal must be append-light (checkpoint coalescing) so SD writes don't dominate. Free-space pre-check in `install_planner` before download starts to fail fast. |

### Scaling Priorities

1. **First bottleneck — cover-art + icon memory on browse.** Reuse the existing lazy-icon plan from the game list; do not hold all JPEGs in memory.
2. **Second bottleneck — index re-parse on every open.** Cache parsed catalog keyed by server+etag/last-modified.

---

## Anti-Patterns

### Anti-Pattern 1: Owning the queue inside an Activity

**What people do:** Kick off the download/install from `runAsync` inside `CatalogDetailActivity` and track progress in activity members.
**Why it's wrong:** Activities are destroyed on navigation; the `alive`/`cancelled` guards exist precisely to *abort* activity-scoped work on teardown. A multi-GB install tied to an activity dies when the user backs out, and cannot resume.
**Do this instead:** Enqueue into the app-scoped `InstallQueueRunner`; activities only observe via `alive`-guarded subscriptions.

### Anti-Pattern 2: Putting parse/plan/diff logic in `platform/`

**What people do:** Parse the Tinfoil JSON, compute the install plan, or diff updates inside the `#ifdef __SWITCH__` files.
**Why it's wrong:** It escapes the doctest gate — the only automated gate — leaving the riskiest decisions untested, and couples logic to libnx.
**Do this instead:** All decidable logic in `core/games/`; `platform/games/` only does the IO that core planned.

### Anti-Pattern 3: Routing content through the cloud API

**What people do:** Add an API endpoint that lists/proxies/caches the user's NSPs "for convenience."
**Why it's wrong:** Turns thomaz from a client into a content host — exactly the line the milestone forbids — and creates a legal/storage liability on the live Lightsail service.
**Do this instead:** API stores only `ContentServer` config rows; the **client** fetches content directly from the user's server. Enforce in the route schema and Prisma doc-comment.

### Anti-Pattern 4: Throwing exceptions / blocking the UI thread in the installer

**What people do:** Throw on libnx errors, or run the chunk loop on the UI thread.
**Why it's wrong:** Violates the project's return-value-error rule and freezes Borealis during multi-GB writes.
**Do this instead:** Return-value `InstallOutcome`; run the loop in the queue runner's worker; deliver progress via `brls::sync`.

---

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| User content server (HTTP/HTTPS) | `remote_content_source` over **existing** `http_client_curl`; GET index + Range GET content; optional `headers`/basic-auth from config | Verify `Accept-Ranges: bytes` for resume; respect the project's **fail-closed TLS** policy (`curl_tls`) — unauthenticated HTTPS downloads are refused, same as existing transports. HTTP (plaintext) is the user's explicit choice. |
| Local SD/USB | `local_content_source` via `fs_util` pread | USB requires the title to be reachable on a mounted FS; SD is always present. |
| thomaz cloud API | `/content-servers` over **existing** `HttpAuthClient` Bearer token | Config sync only. Mirrors `/saves` auth, minus blob. |
| Switch system (ncm/ns) | `nsp_installer_switch` + `installed_query_switch` via libnx | `nsInitialize`/`ncmInitialize` lifecycle like existing `NsTitleService::init/exit`; hardware-only, outside doctest. |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| app ↔ queue runner | enqueue/cancel calls + `alive`-guarded `brls::sync` callbacks | Runner is app-scoped; activities never own jobs. |
| platform/games ↔ core/games | platform calls pure core functions, passes back IO results | Same discipline as `mod_actions.cpp` → `core::plan_install`. |
| installer ↔ content source | `IContentSource::readRange` (offset-based) | Single seam that makes remote and local install one pipeline. |
| installed_query ↔ existing title_service | extends/coexists with `title_service_switch.cpp` | Reuse NACP/control-data reads already implemented; add content-meta + free-space queries. |

---

## Dependency-Ordered Build Sequence

Ordered so every step is verifiable (host doctest where possible) and the tree stays buildable. Phases 1–3 are almost entirely host-testable `core/`; risk concentrates in Phases 5–6 (hardware).

1. **Catalog + index parsing (core, MODIFIED tests)** — `catalog_model`, `index_parser`, `filename_tags`. Pure, fully doctest-covered. *Depends on: nothing.* Unblocks browse.
2. **Content sources (platform)** — `IContentSource`, `remote_content_source` (REUSE curl + Range), `local_content_source`, `catalog_fetch`. *Depends on: 1.* First on-device fetch of a real index.
3. **Store + catalog browse UI (app)** — `GameStoreActivity` (cover grid) + `CatalogDetailActivity` (read-only first). *Depends on: 1, 2.* Now the catalog is visible on hardware — early validation with no install risk.
4. **Queue state machine + journal (core + platform)** — `queue_state` (pure, doctest), `queue_journal`, `InstallQueueRunner` skeleton wired into `main()` with a **no-op/download-only** job. *Depends on: 1.* Validates resume/persistence with downloads before touching ncm.
5. **PFS0 parse + install planner (core)** — `pfs0_parse`, `install_planner`. Pure, doctest against NSP-header fixtures. *Depends on: 1.* De-risks the install decision logic on the host before any hardware code.
6. **NSP installer (platform, HARDWARE)** — `nsp_installer_switch` driven by 4+5; wire the chunked, cancelable, journaled write loop. *Depends on: 2, 4, 5.* Highest risk; isolated and thin by design. **Roadmap flag: this phase needs the most on-hardware validation.**
7. **Installed query + uninstall + free space (platform/app, HARDWARE)** — `installed_query_switch`, `InstalledManagerActivity`, uninstall via `ns`. *Depends on: 6 (shares ncm/ns lifecycle).*
8. **Update/DLC diff (core + UI badges)** — `update_dlc_diff` (pure), surfaced in store + installed views. *Depends on: 1, 7.*
9. **Download queue UI (app)** — `DownloadQueueActivity` progress/cancel/resume, subscribing to the runner. *Depends on: 4, 6.*
10. **Server-link config + one-tap cloud sync (core + platform + API)** — `server_link` model/codec (core, doctest), `server_link_store`, `ServerLinkActivity`, `ContentServer` Prisma model + `/content-servers` route (REUSE auth, mirror `/saves`, Vitest). *Depends on: 1 (config feeds the catalog fetch); can begin in parallel after 1 since it shares no hardware code.* **Config only — no content.**

**Critical-path note:** 1 → 2 → {4, 5} → 6 is the spine. 3, 8, 9, 10 are UI/sync leaves that parallelize. Put hardware risk (6, 7) after the host-tested foundations (1, 4, 5) so failures are install-mechanism bugs, not logic bugs.

---

## Sources

- Existing thomaz source (read directly): `source/app/thomaz_activity.hpp` (runAsync/alive/cancelled), `source/platform/http_client.hpp` + `http_client_curl.cpp` (curl + xferinfo cancel), `source/platform/mods/mod_download.cpp` (chunked download + abort + Content-Length check), `source/platform/mods/mod_actions.cpp` (plan→stage→install discipline), `source/platform/title_service_switch.cpp` (NACP/control-data, ns lifecycle), `source/platform/saves/http_cloud_save_client.cpp` + `api/src/routes/saves.ts` + `api/prisma/schema.prisma` (cloud sync pattern to mirror), `source/core/saves/save_sync_state.cpp` (state serialize pattern). — HIGH
- [libnx ncm.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/ncm.h) — `ncmContentStorageCreatePlaceHolder/WritePlaceHolder/Register`, `ncmContentMetaDatabaseSet/Commit`. — HIGH
- [libnx ncm reference](https://switchbrew.github.io/libnx/ncm_8h.html) and [ns.h reference](https://switchbrew.github.io/libnx/ns_8h.html) — content-meta status + application-record APIs (`nsPushApplicationRecord`, `nsListApplicationContentMetaStatus`). — HIGH
- [Tinfoil Custom Index docs](https://blawar.github.io/tinfoil/custom_index/) — JSON index schema (`files[].url/.size`, `directories`, `titledb`, `headers`, `referrer`, `success`, auth fields). — HIGH
- [Goldleaf nsp_Installer.hpp](https://github.com/blawar/goldbricks/blob/master/Goldleaf/Include/nsp/nsp_Installer.hpp) and [Goldleaf repo](https://github.com/crc-32/Goldleaf) — reference NSP install pipeline (PFS0 → NCA → ContentMeta → record). — HIGH (reference implementation, pattern verification)

---
*Architecture research for: Switch game-management client integration into thomaz*
*Researched: 2026-06-06*
