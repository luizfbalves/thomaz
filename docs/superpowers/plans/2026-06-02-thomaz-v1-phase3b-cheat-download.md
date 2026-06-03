# thomaz v1 — Phase 3b: Cheat download (HTTPS) + cheat detail UI — Plan

> Unblocks the real cheat-detail/toggle screen. Reuses the already-host-tested pure
> core (`db_paths`, `cheat_db`, `build_id`, `cheat_txt`). Verified by host tests +
> Switch CI build green; runtime fetch only verifiable on hardware.

**Goal:** Open a game → fetch its cheats from the open-source **switch-cheats-db** over
HTTPS, resolve the right `build_id` for the installed version, show the cheats as
toggles, and write the enabled set to the Atmosphère SD path so they apply on next boot.

## Key de-risk (RESOLVED)
The `devkitpro/devkita64` CI image installs the **`switch-portlibs`** meta-package, which
**already includes `switch-curl` + `switch-mbedtls` + `switch-zlib`**. So libcurl-with-TLS
is present in CI with **no `dkp-pacman` step** → zero Cloudflare-403 risk. Desktop links
system libcurl (`libcurl4-openssl-dev`).

---

## Waves

### Wave A — HTTP plumbing + orchestration core (de-risk, CI-verifiable)
**A1. `source/platform/http_client.hpp`** — `IHttpClient` + `HttpResponse{long status; std::string body; bool ok()}`; `get(url)`.
**A2. `source/platform/http_client_curl.{hpp,cpp}`** — `CurlHttpClient : IHttpClient` using libcurl easy API.
  - One write callback appending to `std::string`. `CURLOPT_FOLLOWLOCATION=1`, UA string, ~10s timeouts.
  - v1: `CURLOPT_SSL_VERIFYPEER=0` + `CURLOPT_SSL_VERIFYHOST=0` (downloading public cheat text; avoids shipping a CA bundle). TODO: ship `cacert.pem` in romfs + `CURLOPT_CAINFO` to re-enable verification.
  - `#ifdef __SWITCH__`: `socketInitializeDefault()` in a ctor/`init()`, `socketExit()` in dtor. Desktop: no socket init.
  - `curl_global_init/cleanup` guarded once.
**A3. `source/core/cheat_repository.{hpp,cpp}`** (pure, host-testable — NO libcurl):
  - `using UrlFetcher = std::function<std::optional<std::string>(const std::string& url)>;` (nullopt = network/HTTP failure).
  - `struct CheatSet { Resolution resolution; std::vector<Cheat> cheats; std::string sd_path; std::string title; };`
  - `enum class FetchStatus { Ok, NetworkError, NotInDb };` + `struct FetchResult { FetchStatus status; CheatSet set; };`
  - `FetchResult fetch_cheat_set(uint64_t title_id, uint32_t version, const UrlFetcher& fetch);`
    1. `fetch(versions_url)` + `fetch(cheats_url)`; either nullopt → `NetworkError`.
    2. `parse_versions`, `build_ids_with_cheats`, `resolve_build_id`. `NotInDb` → status `NotInDb`.
    3. `parse_db_cheats(cheats_json, resolution.build_id)`, `sd_cheat_path(title_id, build_id)` → `Ok`.
**A4. CMake**: PLATFORM_SWITCH → append `curl mbedtls mbedx509 mbedcrypto z` to `APP_PLATFORM_LIB`
   (portlib include/lib paths come from the devkitA64 toolchain). PLATFORM_DESKTOP → `find_package(CURL REQUIRED)` + link `CURL::libcurl`.
**A5. Host tests** (`tests/`): `fetch_cheat_set` with a fake `UrlFetcher` over canned JSON — Ok / NotInDb / NetworkError / fallback-older-build paths.
**A6. CI/scripts**: desktop deps note += `libcurl4-openssl-dev`. CI needs no change (portlibs already present).
**Verify A:** host suite green; Switch CI `thomaz.nro` green (proves curl links); desktop build green.

### Wave B — SD write (Atmosphère contents)
**B1. `source/platform/cheat_store.{hpp,cpp}`** — `bool write_cheats(const std::string& sd_path, const std::string& body)` and `std::set<std::string> read_enabled(const std::string& sd_path)` (parse existing file via `parse_txt`, collect non-master names present). Creates parent dirs (`/atmosphere/contents/<tid>/cheats/`). Plain `std::filesystem`/`FILE*` — works on both platforms (desktop writes under CWD-relative or a sandbox dir for testing; Switch writes real SD path). Guard the absolute-path root only if needed.
**B2.** On Switch the SD root is just the normal fs (`/atmosphere/...`); ensure dirs created with `mkdir -p` semantics.

### Wave C — Cheat detail UI
**C1. `resources/xml/activity/cheat_detail.xml`** — AppletFrame(title = game name) → ScrollingFrame → column Box (id `cheatListBox`) + a status `Label` (id `statusLabel`) for loading/empty/error, + a footer hint for the save/apply action.
**C2. `source/app/cheat_detail_activity.{hpp,cpp}`** — ctor takes `InstalledTitle` + `IHttpClient*`.
  - `onContentAvailable`: show "loading…", run `fetch_cheat_set` (sync v1; spinner acceptable), then:
    - `NetworkError` → status `@i18n/thomaz/cheats/error_network`.
    - `NotInDb` → status `@i18n/thomaz/cheats/none`.
    - `Ok` → for each non-master `Cheat`, a `brls::ToggleListItem` (or BooleanCell) pre-checked from `read_enabled`. Master cheats always applied (not shown as toggles, or shown disabled-checked). Show a banner if `resolution.source == FallbackOlderBuild` (`@i18n/thomaz/cheats/fallback`).
  - Save: collect checked names → `serialize_txt(cheats, enabled)` → `cheat_store::write_cheats(set.sd_path, body)` → toast `@i18n/thomaz/cheats/saved`. Bind to a button/action (e.g. registerAction A or a "Salvar" ListItem at the bottom).
**C3.** `game_list_activity.cpp`: replace the coming-soon toast with `pushActivity(new CheatDetailActivity(title, httpClient))`. Thread an `IHttpClient*` from main → HomeActivity → GameListActivity → CheatDetailActivity (same ownership pattern as `ITitleService*`).
**C4. i18n** (both locales) under `cheats`: `loading`, `none`, `error_network`, `fallback`, `saved`, `save`, plus a count subtitle.

---

## Done = definition
- Host suite green incl. `fetch_cheat_set` cases.
- Switch CI `thomaz.nro` green with libcurl linked.
- Desktop build green (system libcurl).
- Opening a game in the list pushes a cheat-detail screen that fetches, lists toggles,
  persists the enabled set to the SD cheat path, and handles network/none/fallback states.

## Out of scope (later)
- Async fetch with a real spinner/cancel; ret/ cache to SD.
- CA-bundle verification (cacert.pem in romfs).
- Per-game "has cheats / enabled" badges on the game list (needs a cheap availability probe or cache).
- Game icons.
