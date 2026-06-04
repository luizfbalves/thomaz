<!-- refreshed: 2026-06-04 -->
# Architecture

**Analysis Date:** 2026-06-04

## System Overview

```text
┌──────────────────────────────────────────────────────────────────────┐
│                  NRO / Desktop Binary  (Borealis UI)                 │
│  source/app/*_activity.{cpp,hpp}   (one *Activity per screen)        │
│  resources/xml/activity/*.xml      (UI layout declarations)          │
└────────────┬───────────────────────────────────┬────────────────────┘
             │ injects interfaces                 │ calls core logic
             ▼                                    ▼
┌────────────────────────────┐    ┌───────────────────────────────────┐
│  Platform Abstraction      │    │  Pure Core Logic  (no I/O)        │
│  source/platform/**        │    │  source/core/**                   │
│  - IHttpClient             │    │  - cheat_repository.hpp/.cpp      │
│  - ISaveService            │    │  - mod_browse / mod_install       │
│  - ITitleService           │    │  - saves/save_sync                │
│  - IFeedClient             │    │  - sysmod/sysmod_scan             │
│  - ICloudSaveClient        │    │  - themes/themezer_browse         │
│  - cheat_store / mod_store │    │  - feed/feed_json                 │
└────────────────────────────┘    └───────────────────────────────────┘
             │                                    │
             ▼                                    ▼
┌────────────────────────┐   ┌──────────────────────────────────────────┐
│  Switch/Desktop I/O    │   │  External Services                       │
│  libnx  (saves,titles) │   │  switch-cheats-db  (cheats JSON)         │
│  libcurl (HTTP)        │   │  GameBanana apiv11 (mods)                │
│  libarchive (zip/7z)   │   │  Themezer API      (themes)              │
│  Atmosphère FS paths   │   │  thomaz API        (cloud saves + feed)  │
└────────────────────────┘   └──────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│  thomaz API  (Node.js / Fastify — separate process, api/)            │
│  api/src/routes/{auth,saves,feed,posts,users}.ts                     │
│  api/src/plugins/{auth,db}.ts                                        │
│  api/src/lib/{save-storage, auth-tokens, refresh-tokens, ...}.ts     │
│  → PostgreSQL via Prisma  (api/prisma/schema.prisma)                 │
└──────────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | Key Files |
|-----------|----------------|-----------|
| `source/app/*_activity` | Borealis Activity per screen; wires UI events to platform/core calls | `source/app/home_activity.cpp`, `game_list_activity.cpp`, etc. |
| `source/core/` | Pure, testable business logic (no libcurl, no libnx) | `source/core/cheat_repository.cpp`, `source/core/mods/mod_install.cpp` |
| `source/platform/` | Adapters that implement interfaces; switch vs. fake impls | `source/platform/http_client_curl.cpp`, `source/platform/save_service_switch.cpp` |
| `resources/xml/` | Borealis XML UI layouts — loaded at runtime via `CONTENT_FROM_XML_RES` | `resources/xml/activity/home.xml` |
| `lib/borealis/` | UI framework (vendored fork of xfangfang/borealis) | `lib/borealis/library/` |
| `lib/switchthemes/` | Vendored exelix NX theme engine (GPLv2); used by `theme_install` | `lib/switchthemes/NXTheme.cpp` |
| `lib/json/` | nlohmann/json header-only library | `lib/json/` |
| `api/src/` | Cloud backend: auth, cloud saves, community feed | `api/src/app.ts`, `api/src/routes/` |
| `tests/` | Host-side doctest suite (runs on desktop, no Switch hardware needed) | `tests/test_cheat_repository.cpp`, etc. |

## Pattern Overview

**Overall:** Clean architecture with injected interfaces — core logic is kept pure and dependency-free; all I/O and Switch-specific APIs are behind abstract base classes.

**Key Characteristics:**
- Every service that touches hardware or network is represented as a pure virtual interface (`IHttpClient`, `ISaveService`, `ITitleService`, `IFeedClient`, `ICloudSaveClient`)
- Platform impls come in pairs: a real `*_switch.cpp` and a `*_fake.cpp` for desktop development
- Compile-time platform selection via `#ifdef __SWITCH__` in `main.cpp`; the rest of the code is platform-agnostic
- Borealis `Activity` subclasses own no business logic — they only compose platform + core calls
- XML layouts declare UI structure; C++ `onContentAvailable()` wires click/gesture handlers

## Layers

**App Layer (`source/app/`):**
- Purpose: Screen activities and UI widgets
- Location: `source/app/`
- Contains: `*Activity` subclasses, UI helpers (`app_header`, `animated_box`, `game_panel`)
- Depends on: Borealis, platform interfaces, core types
- Used by: `main.cpp` (entry point pushes `HomeActivity`)

**Core Layer (`source/core/`):**
- Purpose: Pure business logic — parsing, validation, path resolution, HTTP orchestration (via injected `UrlFetcher`)
- Location: `source/core/`
- Contains: feature sub-namespaces: `core/mods/`, `core/saves/`, `core/sysmod/`, `core/themes/`, `core/feed/`
- Depends on: nlohmann/json, standard library only
- Used by: `source/platform/` orchestrators and the test suite

**Platform Layer (`source/platform/`):**
- Purpose: Concrete implementations of interfaces + platform orchestration
- Location: `source/platform/`
- Contains: `IHttpClient` → `HttpClientCurl`; `ISaveService` → `SaveServiceSwitch` / `SaveServiceFake`; feature sub-dirs: `platform/mods/`, `platform/saves/`, `platform/sysmod/`, `platform/themes/`, `platform/feed/`
- Depends on: core layer, libcurl, libarchive, libnx (Switch only)
- Used by: `source/app/` (via injected pointers)

**API Layer (`api/src/`):**
- Purpose: Cloud backend for saves and community feed
- Location: `api/src/`
- Contains: Fastify routes, plugins (JWT auth, Prisma), lib utilities
- Depends on: PostgreSQL (Prisma), JWT, bcrypt, zod
- Used by: NRO client via `ICloudSaveClient` / `IFeedClient` HTTP calls

## Data Flow

### Cheat Apply Flow (primary path)

1. User selects game in `GameListActivity` → calls `fetch_cheat_set(title_id, version, fetcher)` (`source/core/cheat_repository.cpp`)
2. `cheat_repository` uses injected `UrlFetcher` (wraps `IHttpClient::get`) to fetch JSON from switch-cheats-db
3. `cheat_db.cpp` parses the JSON; `build_id.cpp` resolves the correct build_id
4. Result (`CheatSet`) returned to `CheatDetailActivity` (`source/app/cheat_detail_activity.cpp`)
5. Activity calls `cheat_store::write_cheats()` (`source/platform/cheat_store.cpp`) to write `.txt` files to `/atmosphere/contents/<tid>/cheats/` on the SD card

### Mod Install Flow

1. `ModBrowserActivity` → `mod_browse::search_page()` (`source/core/mods/mod_browse.cpp`) fetches GameBanana apiv11
2. `ModDetailActivity` → `mod_actions::install()` (`source/platform/mods/mod_actions.cpp`)
3. `mod_download.cpp` fetches archive via `IHttpClient`; `libarchive_extractor.cpp` unpacks
4. Files written to `/atmosphere/contents/<tid>/romfs/` per `mod_paths.cpp`

### Cloud Save Sync Flow

1. `SaveDetailActivity` triggers upload/download via `ICloudSaveClient`
2. `HttpCloudSaveClient` (`source/platform/saves/http_cloud_save_client.cpp`) calls thomaz API (`api/src/routes/saves.ts`)
3. Blob stored in `api/uploads/` and tracked in Postgres `SaveSlot` table
4. On restore: `ISaveService::importPackageAsBackup()` → `save_backup_io.cpp` → libnx save mount

### Theme Apply Flow

1. `ThemeBrowserActivity` → `themezer_browse.cpp` queries Themezer GraphQL API
2. `ThemeDetailActivity` → `theme_download.cpp` downloads `.nxtheme` file
3. `theme_install.cpp` calls `lib/switchthemes` engine (exelix) to patch firmware files
4. `active_theme_store.cpp` records the applied theme ID; `system_reboot.cpp` triggers reboot

**State Management:**
- No global mutable state; services are instantiated once in `main.cpp` and passed down as raw pointers (owner is `main`)
- `AppSettings` (`source/platform/app_settings.cpp`) reads/writes a JSON config file on SD
- Auth session persisted to disk via `auth_store.cpp` (`source/platform/feed/auth_store.cpp`)

## Key Abstractions

**`IHttpClient` (`source/platform/http_client.hpp`):**
- Purpose: HTTP request/response abstraction
- Implementations: `HttpClientCurl` (real), injected `UrlFetcher` lambda in core (thin wrapper)
- Used by: all activities needing network access

**`ISaveService` (`source/platform/save_service.hpp`):**
- Purpose: Read/write game save data
- Implementations: `SaveServiceSwitch` (libnx), `SaveServiceFake` (desktop NOP)

**`ITitleService` (`source/platform/title.hpp` declares `InstalledTitle`):**
- Implementations: `TitleServiceSwitch` (libnx), `TitleServiceFake`

**`IFeedClient` (`source/platform/feed/feed_client.hpp`):**
- Purpose: Community feed read/write
- Implementations: `HttpFeedClient`, `FakeFeedClient`

**`ICloudSaveClient` (`source/platform/saves/cloud_save_client.hpp`):**
- Purpose: Upload/download save blobs
- Implementations: `HttpCloudSaveClient`, `FakeCloudSaveClient`

## Entry Points

**NRO/Desktop binary (`main.cpp` — project root, compiled via `GLOB_RECURSE`):**
- Location: `main.cpp` (root of repo)
- Triggers: devkitPro `nro` launch (Switch) or direct binary execution (desktop)
- Responsibilities: Borealis init, platform impl selection (`#ifdef __SWITCH__`), construct all services, push `HomeActivity`

**API Server (`api/src/index.ts`):**
- Location: `api/src/index.ts`
- Triggers: `node dist/index.js` via PM2 on Lightsail
- Responsibilities: `buildApp()` → listen on configured port

**Test runner (`tests/Makefile`):**
- Location: `tests/`
- Triggers: `make` in `tests/` on desktop
- Responsibilities: doctest host suite, covers all core and platform units

## Architectural Constraints

- **Threading:** Single-threaded event loop (Borealis main thread); network calls issued synchronously from click handlers (Borealis runs them in the UI thread — blocking is acceptable on Switch where the OS schedules NRO threads)
- **Global state:** `thomaz::set_self_path()` (`source/platform/self_update.cpp`) stores the launch argv[0] in a module-level string; `brls::Application` is a Borealis singleton
- **Circular imports:** None detected; dependency direction is strictly app → platform → core
- **Platform guard:** `#ifdef __SWITCH__` used only in `main.cpp` and `*_fake`/`*_switch` pairs — all other files are platform-agnostic C++20
- **C++ standard:** C++20 required (std::span, std::format in `lib/switchthemes`); the NRO and desktop targets both build with `-std=c++20`

## Anti-Patterns

### Direct platform calls from Activity

**What happens:** Activities could call libnx or file system APIs directly.
**Why it's wrong:** Breaks desktop build and makes unit testing impossible.
**Do this instead:** Call through the injected interface pointer (e.g. `this->saveService->backup()`).

### Adding business logic to platform layer

**What happens:** Logic could be added to `*_store.cpp` / `*_actions.cpp` instead of `core/`.
**Why it's wrong:** The `core/` layer is the only part covered by the host test suite; logic in `platform/` is untested on desktop.
**Do this instead:** Add new parsing/orchestration logic in `source/core/<feature>/`; keep `platform/` as thin orchestration that delegates to core.

## Error Handling

**Strategy:** Return value based — no exceptions in core or platform code.

**Patterns:**
- Core functions return a struct with a status enum (e.g. `FetchStatus::Ok`, `FetchStatus::NetworkError`)
- Platform functions return `bool` + set `std::string* outError` on failure (e.g. `ISaveService::backup`)
- API routes return `{ ok: false, error: "..." }` JSON with appropriate HTTP status codes
- Activities display Borealis dialogs on error; no crash expected for network failures

## Cross-Cutting Concerns

**Logging:** `brls::Logger::info/warn/error()` (Borealis logging) in NRO code; `console.log` in API
**Validation:** Input validation in `core/` parsing functions; Zod schemas in API routes
**Authentication:** JWT (`@fastify/jwt`) on API; token stored locally by `auth_store.cpp` and sent as Bearer header by `HttpFeedClient` / `HttpCloudSaveClient`

---

*Architecture analysis: 2026-06-04*
