# Codebase Structure

**Analysis Date:** 2026-06-04

## Directory Layout

```
thomaz/                          # repo root
├── main.cpp                     # NRO/desktop binary entry point
├── CMakeLists.txt               # single build file, dual-target (Switch + desktop)
├── source/                      # all C++ application source
│   ├── app/                     # Borealis Activities and UI widgets
│   ├── core/                    # pure, platform-free business logic
│   │   ├── feed/                # community feed JSON parsing
│   │   ├── mods/                # GameBanana types, URLs, browse, install logic
│   │   ├── saves/               # cloud save packaging, sync, JSON
│   │   ├── sysmod/              # sysmodule scanning, paths, toolbox JSON
│   │   └── themes/              # Themezer API types, queries, browse logic
│   └── platform/                # concrete service implementations
│       ├── feed/                # IFeedClient impls (HTTP + fake)
│       ├── mods/                # mod download, install, libarchive extractor
│       ├── saves/               # ICloudSaveClient impls, save_backup_io, sync_store
│       ├── sysmod/              # ISysmodStore impls (real + fake), system_reboot
│       ├── system/              # reboot helpers
│       └── themes/              # theme download, install, active_theme_store, paths
├── resources/                   # runtime assets (bundled as romfs on Switch)
│   ├── xml/
│   │   ├── activity/            # one .xml per Activity (screen layout)
│   │   ├── cells/               # recycler cell layouts
│   │   ├── tabs/                # tab layouts
│   │   └── views/               # reusable view layouts
│   ├── i18n/                    # locale string files (en-US, pt-BR, fr, ru, zh-Hans)
│   ├── font/                    # bundled fonts
│   ├── img/                     # icons and images
│   ├── shaders/                 # deko3d shaders (Switch GPU)
│   └── cacert.pem               # CA bundle for curl TLS
├── lib/                         # vendored third-party libraries
│   ├── borealis/                # UI framework (xfangfang/borealis fork)
│   │   └── library/             # Borealis headers + CMake targets
│   ├── switchthemes/            # exelix NX theme engine (GPLv2, C++20)
│   ├── json/                    # nlohmann/json (header-only)
│   └── doctest/                 # doctest test framework (header-only)
├── tests/                       # host-side test suite (runs on desktop)
│   ├── test_main.cpp            # doctest runner entry
│   ├── test_*.cpp               # one test file per tested unit
│   └── Makefile                 # build + run tests
├── api/                         # Node.js cloud backend
│   ├── src/
│   │   ├── index.ts             # process entry (listen)
│   │   ├── app.ts               # buildApp() — Fastify instance factory
│   │   ├── config.ts            # env validation (loadConfig)
│   │   ├── routes/              # auth.ts, feed.ts, posts.ts, saves.ts, users.ts
│   │   ├── plugins/             # auth.ts (JWT), db.ts (Prisma singleton)
│   │   └── lib/                 # auth-tokens, refresh-tokens, save-storage, serializers, …
│   ├── prisma/
│   │   ├── schema.prisma        # data model
│   │   └── migrations/          # migration SQL files
│   ├── test/                    # API integration tests
│   ├── uploads/                 # save blob files (runtime, gitignored)
│   └── package.json
├── docs/                        # project documentation + screenshots
├── scripts/                     # helper shell scripts
├── .github/workflows/           # CI definitions
├── build-desktop/               # CMake build output (desktop, gitignored)
├── build-switch/                # CMake build output (Switch NRO, gitignored)
└── .planning/                   # GSD planning artefacts
    └── codebase/                # codebase map documents (this directory)
```

## Directory Purposes

**`source/app/`:**
- Purpose: One `*Activity` per application screen; wires XML layouts to platform/core logic
- Contains: `*_activity.{cpp,hpp}` pairs, UI helper widgets (`game_panel`, `animated_box`, `app_header`)
- Key files: `source/app/home_activity.cpp`, `source/app/game_list_activity.cpp`, `source/app/theme_browser_activity.cpp`

**`source/core/`:**
- Purpose: Pure C++ business logic — JSON parsing, path computation, HTTP orchestration via injected lambdas
- Contains: Feature sub-namespaces under `core/mods/`, `core/saves/`, `core/sysmod/`, `core/themes/`, `core/feed/`
- Key files: `source/core/cheat_repository.cpp`, `source/core/mods/mod_install.cpp`, `source/core/saves/save_sync.cpp`

**`source/platform/`:**
- Purpose: Concrete implementations of all abstract service interfaces; Switch vs. fake pairs
- Contains: `*_switch.cpp` (real hardware), `*_fake.cpp` (desktop stubs), feature orchestrators
- Key files: `source/platform/http_client_curl.cpp`, `source/platform/save_service_switch.cpp`, `source/platform/saves/http_cloud_save_client.cpp`

**`resources/xml/activity/`:**
- Purpose: Borealis XML layout files — one per screen; loaded by `CONTENT_FROM_XML_RES` macro at runtime
- Key files: `resources/xml/activity/home.xml`, `resources/xml/activity/game_list.xml`

**`lib/borealis/`:**
- Purpose: Vendored Borealis UI framework (CMake subproject via `add_subdirectory`)
- Generated: No — vendored source
- Committed: Yes

**`lib/switchthemes/`:**
- Purpose: Vendored exelix NX theme engine; compiled via `GLOB_RECURSE ENGINE_SRC` in `CMakeLists.txt`
- Committed: Yes

**`api/src/routes/`:**
- Purpose: Fastify route handlers — one file per resource (`auth`, `saves`, `feed`, `posts`, `users`)

**`api/src/plugins/`:**
- Purpose: Fastify plugins that decorate the instance — `auth.ts` (JWT + `authenticate` decorator), `db.ts` (Prisma client singleton)

**`api/src/lib/`:**
- Purpose: Shared utility functions used by routes — token generation, save blob I/O, serializers, cursor pagination

**`tests/`:**
- Purpose: Host-side doctest unit tests; build with a plain `make` on Linux/macOS, no Switch hardware required
- Key files: `tests/Makefile`, `tests/test_main.cpp`

## Key File Locations

**Entry Points:**
- `main.cpp`: NRO/desktop binary — platform detection, service wiring, pushes `HomeActivity`
- `api/src/index.ts`: API process — calls `buildApp()` then listens
- `tests/test_main.cpp`: doctest runner for host tests

**Build Configuration:**
- `CMakeLists.txt`: Dual-target build (Switch NRO + desktop), Borealis subdirectory, libcurl/libarchive linking
- `api/package.json`: API Node.js dependencies and scripts

**Core Logic:**
- `source/core/cheat_repository.cpp`: Cheat fetch + resolution pipeline
- `source/core/mods/mod_install.cpp`: Mod installation logic
- `source/core/saves/save_sync.cpp`: Cloud save sync state machine
- `source/core/themes/themezer_browse.cpp`: Themezer API query/parse

**Platform Interfaces:**
- `source/platform/http_client.hpp`: `IHttpClient` — HTTP abstraction
- `source/platform/save_service.hpp`: `ISaveService` — save read/write abstraction
- `source/platform/title.hpp`: `InstalledTitle` type + `ITitleService`

**API Data Layer:**
- `api/prisma/schema.prisma`: Database schema (Users, SaveSlots, Posts, RefreshTokens)
- `api/src/lib/save-storage.ts`: Save blob file I/O (`api/uploads/`)
- `api/src/lib/auth-tokens.ts`: JWT creation helpers

**Settings:**
- `source/platform/app_settings.cpp`: Reads/writes JSON config on SD card

## Naming Conventions

**Files:**
- C++ source: `snake_case.cpp` / `snake_case.hpp` — one class or one feature unit per pair
- Interface headers: `i_<noun>.hpp` is NOT used — interface is named `I<Noun>` but the file is `<noun>.hpp` (e.g. `save_service.hpp` contains `ISaveService`)
- Switch impl: `<noun>_switch.{cpp,hpp}`
- Desktop fake: `<noun>_fake.{cpp,hpp}`
- HTTP impl: `http_<noun>.{cpp,hpp}` or `<noun>_curl.{cpp,hpp}`
- Activities: `<feature>_activity.{cpp,hpp}`
- API TypeScript: `kebab-case.ts`

**Directories:**
- `source/core/<feature>/` — one sub-directory per domain feature when multiple files are needed
- `source/platform/<feature>/` — mirrors the core sub-directory structure

**Namespaces (C++):**
- `thomaz` — top-level namespace for the application
- `thomaz::core` — pure logic namespace
- Borealis uses `brls::`

**XML IDs:**
- Borealis view IDs use camelCase (e.g. `cheatsCard`, `modsCard`)

## Where to Add New Code

**New feature with network + UI (e.g. new browser screen):**
1. Data types: `source/core/<feature>/<feature>_types.hpp`
2. JSON parsing: `source/core/<feature>/<feature>_json.{cpp,hpp}`
3. API URL helpers: `source/core/<feature>/<feature>_urls.{cpp,hpp}`
4. Browse/query logic: `source/core/<feature>/<feature>_browse.{cpp,hpp}`
5. Platform interface: `source/platform/<feature>/<feature>_client.hpp` (abstract class)
6. Real impl: `source/platform/<feature>/http_<feature>_client.{cpp,hpp}`
7. Fake impl: `source/platform/<feature>/fake_<feature>_client.{cpp,hpp}`
8. Activity: `source/app/<feature>_activity.{cpp,hpp}`
9. XML layout: `resources/xml/activity/<feature>.xml`
10. Tests: `tests/test_<feature>_browse.cpp`, `tests/test_<feature>_json.cpp`
11. Wire in `main.cpp`: construct real or fake client, inject into `HomeActivity`

**New platform service (Switch-only operation):**
- Interface: `source/platform/<service>.hpp`
- Switch impl: `source/platform/<service>_switch.cpp`
- Fake impl: `source/platform/<service>_fake.cpp`
- Guard in `main.cpp`: `#ifdef __SWITCH__` to select impl

**New API endpoint:**
- Add handler in `api/src/routes/<resource>.ts`
- Add schema migration in `api/prisma/migrations/`
- Add lib helpers in `api/src/lib/` if needed

**New pure-logic unit:**
- Implementation: `source/core/<feature>/<unit>.{cpp,hpp}`
- Test: `tests/test_<unit>.cpp`

## Special Directories

**`build-desktop/` and `build-switch/`:**
- Purpose: CMake out-of-source build outputs
- Generated: Yes
- Committed: No (gitignored with hyphenated variants)

**`lib/borealis/`:**
- Purpose: Full vendored Borealis UI framework source
- Generated: No
- Committed: Yes (git submodule or copied)

**`api/uploads/`:**
- Purpose: Save blob binary files stored by the cloud API
- Generated: Yes (runtime)
- Committed: No

**`api/dist/`:**
- Purpose: TypeScript compile output
- Generated: Yes (`tsc`)
- Committed: No (gitignored)

**`thomaz-cache/`:**
- Purpose: Local cache of downloaded cheats/DB index (desktop dev)
- Generated: Yes
- Committed: No

---

*Structure analysis: 2026-06-04*
