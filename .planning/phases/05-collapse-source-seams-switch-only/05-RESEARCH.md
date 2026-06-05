# Phase 5: Collapse Source Seams to Switch-Only — Research

**Researched:** 2026-06-05
**Domain:** C++ preprocessor seam collapse / desktop stub removal (Nintendo Switch homebrew)
**Confidence:** HIGH — every claim below is grounded in an actual file path and grep/read result

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SIMPL-01 | Delete the five desktop stub pairs from `source/platform/` | Section 1 maps each pair: exact paths confirmed, switch counterparts identified |
| SIMPL-02 | Every `#ifdef __SWITCH__ … #else … #endif` desktop branch removed; `*_switch` impls sole behind their interfaces | Section 2 catalogs every seam and classifies it (must collapse / may stay) |
| SIMPL-03 | No `source/` file references a deleted stub or desktop-only symbol | Section 3 maps all usage sites; section 4 confirms `PLATFORM_DESKTOP`/SDL2/GLFW absent from `source/` today |
</phase_requirements>

---

## Summary

Phase 5 is a mechanical deletion-and-simplification pass with no logic changes. The entire work surface is:

1. **Delete 10 files** (5 `.cpp` + 5 `.hpp` stub pairs).
2. **Edit 3 source files** to remove the factory/include seams that referenced the stubs (`main.cpp`, `home_activity.cpp`), and the one `.hpp` comment that still names a deleted fake.
3. **Collapse 14 additional `#ifdef`/`#else` path seams** in 12 source files where an `#ifdef __SWITCH__` block has an `#else` desktop path (path strings, no-op function bodies, networkReady assignment, qlaunch warning, `localtime_r` vs no-op, etc.) — remove only the desktop branch; keep the Switch path unconditional.
4. **Leave 10+ `#ifdef __SWITCH__` guards alone** where the guard wraps Switch-only code with no `#else` — the requirement is to remove desktop branches, not to strip all guards.
5. **Touch nothing in `tests/`** — the doctest suite does not compile any of the deleted files and will pass unchanged.
6. **Touch nothing in `CMakeLists.txt`** in this phase — the build-system strip is Phase 6 scope.

The host doctest suite (`make -C tests test`) compiles from an explicit source list that does not include any of the 5 deleted stub pairs. It will remain green after deletions with zero changes to `tests/Makefile`.

**Primary recommendation:** Delete the 10 stub files, then edit the 3 consumer files, then sweep the 12 remaining seam files to drop `#else` desktop branches. Verify with `grep -rn 'ifndef __SWITCH__\|#else' source/` (should show only `_WIN32`/`localtime_r` branches and `#else` inside `#ifdef __SWITCH__`-only blocks after collapse) and `make -C tests test`.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Title listing | Switch platform | — | `NsTitleService` (libnx ns) is the sole impl after `FakeTitleService` deleted |
| Save backup/restore | Switch platform | — | `NsSaveService` (libnx acc/fs) is the sole impl after `FakeSaveService` deleted |
| Auth client | API/network platform | — | `HttpAuthClient` is sole impl; `FakeAuthClient` was desktop-only, never selected at runtime on Switch |
| Firmware extraction | Switch platform | — | `firmware_extract_switch.cpp` is sole impl after `firmware_extract_fake.cpp` deleted |
| Sysmodule store | Switch platform | doctest host | `SysmoduleStore` is runtime; `FakeSysmoduleStore` was desktop GUI only |
| Cloud save client | API/network platform | doctest host | `HttpCloudSaveClient` is runtime; `FakeCloudSaveClient` (RETAINED) is doctest double only |
| Path strings (SD card vs local) | Switch platform | — | Every `#else` path (e.g. `thomaz-cache/…`) removed; Switch SD paths stay unconditional |

---

## Section 1: The Five Desktop Stub Pairs — Exact Paths and Inventory

### 1A. `save_service_fake.{cpp,hpp}`
- **Delete:** `source/platform/save_service_fake.cpp`, `source/platform/save_service_fake.hpp`
- **Interface:** `source/platform/save_service.hpp` (`ISaveService`)
- **Switch counterpart:** `source/platform/save_service_switch.{cpp,hpp}` (`NsSaveService`)
- **Guard style:** Both files wrapped in `#ifndef __SWITCH__` … `#endif` — on Switch they compile to nothing today; after deletion the CMake glob simply won't find them
- **Usage sites (all in `main.cpp`):**
  - Line 13: `#include "platform/save_service_fake.hpp"` inside `#else` block (lines 11-14)
  - Line 85: `auto saveService = std::make_unique<thomaz::FakeSaveService>();` inside `#else` block (lines 83-86)
- **`tests/Makefile`:** Not referenced
- **Comment in interface to update:** `save_service.hpp` line 11-12: "Switch impl uses libnx; the fake impl lets the full UI flow run on desktop without a console." → remove the "fake impl" half of the sentence

### 1B. `title_service_fake.{cpp,hpp}`
- **Delete:** `source/platform/title_service_fake.cpp`, `source/platform/title_service_fake.hpp`
- **Interface:** `source/platform/title.hpp` (forward-declared as `ITitleService` in `title.hpp` bottom comment) — actual interface is at the bottom of `title.hpp`
- **Switch counterpart:** `source/platform/title_service_switch.{cpp,hpp}` (`NsTitleService`)
- **Guard style:** `.hpp` wrapped in `#ifndef __SWITCH__`; `.cpp` wrapped in `#ifndef __SWITCH__`
- **Usage sites (all in `main.cpp`):**
  - Line 12: `#include "platform/title_service_fake.hpp"` inside `#else` block (lines 11-14)
  - Line 78: `auto titleService = std::make_unique<thomaz::FakeTitleService>();` inside `#else` block (lines 77-79)
- **`tests/Makefile`:** Not referenced
- **Note:** `title_service_switch.hpp` has NO `#ifdef __SWITCH__` guard but `title_service_switch.cpp` does (line 5). After deletion the CMake glob picks up `title_service_switch.cpp` which compiles to nothing on a non-Switch build — but this phase doesn't affect non-Switch builds since Phase 6 removes that path entirely.

### 1C. `fake_auth_client.{cpp,hpp}`
- **Delete:** `source/platform/fake_auth_client.cpp`, `source/platform/fake_auth_client.hpp`
- **Interface:** `source/platform/auth_client.hpp` (`IAuthClient`)
- **Switch counterpart:** `source/platform/http_auth_client.{cpp,hpp}` (`HttpAuthClient`) — used on both Switch and desktop via the factory in `main.cpp` (no `#ifdef` seam, always instantiated)
- **Guard style:** **Neither file has a `#ifndef __SWITCH__` guard.** The class compiles unconditionally but is never selected at runtime on Switch (main.cpp always uses `HttpAuthClient`). The files are never included anywhere except their own `.cpp` self-include. After deletion the CMake glob simply won't find them.
- **Usage sites:** None outside the stub files themselves. The comment in `auth_client.hpp` line 12 ("FakeAuthClient runs on the desktop") should be removed/updated.
- **`tests/Makefile`:** Not referenced
- **Key insight:** `FakeAuthClient` has no `#ifndef __SWITCH__` guard — it compiles on Switch today but is dead code (never instantiated). Deleting it is clean.

### 1D. `themes/firmware_extract_fake.cpp` (hpp-less)
- **Delete:** `source/platform/themes/firmware_extract_fake.cpp`
- **Interface:** `source/platform/themes/firmware_extract.hpp` (`ExtractAllResult extract_all_base_layouts()`)
- **Switch counterpart:** `source/platform/themes/firmware_extract_switch.cpp`
- **Guard style:** `.cpp` file wrapped in `#ifndef __SWITCH__` … `#endif` (lines 3 and 13). No corresponding `.hpp` to delete — the interface header is `firmware_extract.hpp` which remains.
- **Usage sites:** The comment in `firmware_extract.hpp` lines 45-47 ("Desktop behaviour (firmware_extract_fake.cpp): Returns {false, …}") must be removed/updated.
- **`tests/Makefile`:** Not referenced
- **Note:** `firmware_extract_switch.cpp` is entirely wrapped in `#ifdef __SWITCH__` (line 7 to line 207). After deleting the fake, the interface function `extract_all_base_layouts()` is defined only inside that guard — this is fine for the Switch build (Phase 6); for the host doctest build `tests/Makefile` does not compile `firmware_extract_switch.cpp` at all and never calls `extract_all_base_layouts()`, so no link issue arises.

### 1E. `sysmod/sysmod_store_fake.{cpp,hpp}`
- **Delete:** `source/platform/sysmod/sysmod_store_fake.cpp`, `source/platform/sysmod/sysmod_store_fake.hpp`
- **Interface:** `source/platform/sysmod/sysmod_store.hpp` (`ISysmoduleStore`)
- **Switch counterpart:** `source/platform/sysmod/sysmod_store.{cpp,hpp}` (`SysmoduleStore`) — no `_switch` suffix; the real store lives in `sysmod_store.{cpp,hpp}` itself
- **Guard style:** `.cpp` wrapped in `#ifndef __SWITCH__`; `.hpp` has no guard (class declaration always visible)
- **Usage sites:**
  - `source/app/home_activity.cpp` line 15: `#ifndef __SWITCH__` → `#include "platform/sysmod/sysmod_store_fake.hpp"` → `#endif`
  - `source/app/home_activity.cpp` lines 94-98: `#ifdef __SWITCH__` → `SysmoduleStore`, `#else` → `FakeSysmoduleStore`
- **`tests/Makefile`:** Not referenced (tests compile `sysmod_store.cpp`, not the fake)
- **Comment to update:** `sysmod_store.hpp` line 20: "the desktop fake (sysmod_store_fake) is in-memory." → remove the desktop fake mention

---

## Section 2: Complete Seam Catalog

The grep `grep -rn 'ifndef __SWITCH__\|#else' source/` produced 40+ matches. Below they are classified.

### Category A — "Must Collapse": `#else` desktop branch exists alongside `#ifdef __SWITCH__` Switch code

These are the seams SIMPL-02 requires to be removed. Action: keep the Switch block unconditionally (unwrap the `#ifdef __SWITCH__` guard and remove the `#else` + desktop block + `#endif`).

| File | Lines | What Switch path does | What desktop `#else` does | Action |
|------|-------|----------------------|--------------------------|--------|
| `source/main.cpp` | 7-14 | includes `<switch.h>`, `title_service_switch.hpp`, `save_service_switch.hpp` | includes `title_service_fake.hpp`, `save_service_fake.hpp` | Remove `#else` block and `#ifdef` wrapper; keep the 3 Switch includes unconditionally |
| `source/main.cpp` | 70-79 | `NsTitleService` + `init()` call | `FakeTitleService` | Remove `#ifdef` guard; keep Switch factory + `init()` unconditionally |
| `source/main.cpp` | 82-86 | `NsSaveService` | `FakeSaveService` | Remove `#ifdef` guard; keep Switch factory unconditionally |
| `source/app/home_activity.cpp` | 14-16 | (no Switch block; guard is `#ifndef __SWITCH__`) | `#include sysmod_store_fake.hpp` | Delete the entire 3-line `#ifndef…#endif` block |
| `source/app/home_activity.cpp` | 94-98 | `SysmoduleStore` | `FakeSysmoduleStore` | Remove `#ifdef` guard; keep Switch factory unconditionally |
| `source/platform/game_stats.cpp` | 10-56 | `pdmqryInitialize` + real stats | deterministic fake stats | Remove `#else` block; keep Switch implementation unconditionally (drop the `#ifdef __SWITCH__`/`#endif` wrapper) |
| `source/platform/app_settings.cpp` | 9-13 | `/switch/thomaz/config/locale.txt` | `thomaz-cache/locale.txt` | Remove `#else` branch; keep Switch path unconditionally |
| `source/platform/app_settings.cpp` | 25-29 | `/switch/thomaz/config/api_url.txt` | `thomaz-cache/api_url.txt` | Remove `#else` branch; keep Switch path unconditionally |
| `source/core/backup_store.cpp` | 46-50 | `/switch/thomaz/saves` | `thomaz-saves` | Remove `#else`; keep Switch path unconditionally |
| `source/core/mods/mod_paths.cpp` | 7-10 | `/switch/thomaz/mods` | `thomaz-mods` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/feed/auth_store.cpp` | 10-14 | `/switch/thomaz/config/session.txt` | `thomaz-cache/session.txt` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/saves/sync_store.cpp` | 11-14 | `/switch/thomaz/config/save_sync.txt` | `thomaz-cache/save_sync.txt` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/cheat_store.cpp` | 50-54 | `/switch/thomaz/cache/versions.json` | `thomaz-cache/versions.json` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/themes/cfw_paths.cpp` | 7-10 | `/atmosphere/contents` | `themes-out/contents` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/themes/cfw_paths.cpp` | 15-18 | `/themes/systemData` | `themes/systemData` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/themes/theme_paths.cpp` | 7-10 | `/themes` | `themes` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/themes/qlaunch_patches.cpp` | 16-19 | `romfs:/theme-patches` | `resources/theme-patches` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/themes/qlaunch_patches.cpp` | 26-29 | `/atmosphere/exefs_patches/thomaz_themes` | `themes-out/exefs_patches/thomaz_themes` | Remove `#else`; keep Switch path unconditionally |
| `source/platform/http_client_curl.cpp` | 32-42 | `networkReady = true` | `networkReady = true` (identical!) | Both branches assign `true` — remove the entire `#ifdef __SWITCH__ … #else … #endif` block; keep a single `networkReady = true;` |
| `source/platform/themes/theme_install.cpp` | 63-69 | warning push when `patches == 0` | `(void)patches;` | Remove `#else (void)patches` branch; keep Switch warning branch unconditionally |
| `source/platform/sysmod/system_reboot.cpp` | 3-31 | Switch `spsmShutdown` impl | `return false;` desktop no-op | Remove `#else` block; keep Switch impl only — entire `#ifdef __SWITCH__` wrapper gone, Switch code becomes unconditional |
| `source/platform/curl_tls.hpp` | 38-70 | romfs CA bundle path | system CA store | Remove `#else` block; keep Switch CA bundle path unconditionally |

**Total: 22 `#else`/`#ifndef` desktop branches to remove across 12 files.**

### Category B — "May Stay": `#ifdef __SWITCH__` with NO `#else` desktop branch

These guards wrap Switch-only code (libnx API calls) with no desktop alternative. The requirement is to remove desktop branches, not all guards. They may stay or be removed as taste dictates, but SIMPL-02 does not require touching them.

| File | Lines | What it guards | Recommendation |
|------|-------|----------------|----------------|
| `source/main.cpp` | 131-133 | `titleService->exit()` (only on Switch) | May stay; no `#else`, safe to keep |
| `source/platform/themes/theme_compat.cpp` | 15 | `#include <switch.h>` | May stay; no `#else` |
| `source/platform/themes/theme_compat.cpp` | 147-157 | `setsysInitialize` / `setsysGetFirmwareVersion` | May stay; no `#else` |
| `source/platform/themes/firmware_extract_switch.cpp` | 7, 207 | Entire file body | KEEP — these guards prevent host-build compile errors since this file uses libnx |
| `source/platform/themes/key_loader_switch.cpp` | 4 | Entire file body | KEEP |
| `source/platform/themes/nca_extract_switch.cpp` | 7 | Entire file body | KEEP |
| `source/platform/themes/qlaunch_patches.cpp` | 26 | (after collapsing inner branch, the outer `#ifdef` for `patches==0` warning remains) | May stay |
| `source/platform/save_service_switch.cpp` | 3 | Entire file body | KEEP |
| `source/platform/title_service_switch.cpp` | 5 | Entire file body | KEEP |
| `source/platform/system/reboot.cpp` | 3, 10 | `#include <switch.h>` and `spsmInitialize` block | May stay or collapse (inner `#ifdef __SWITCH__` with no `#else`; desktop comment "intentionally nothing" stays as a comment or is dropped) |

### Category C — Unrelated `#else` (NOT a `__SWITCH__` seam)

These `#else` lines appeared in the grep but are NOT `__SWITCH__` seams — do not touch them:

| File | Lines | What it is |
|------|-------|------------|
| `source/app/game_panel.cpp` | 71 | `#if defined(_WIN32)` → `localtime_s` vs `#else` → `localtime_r` — Windows/POSIX POSIX date formatting, unrelated to Switch |
| `source/platform/saves/save_backup_io.cpp` | 24 | Same `_WIN32`/`localtime_r` pattern |

---

## Section 3: The Retained Doctest Double

**`source/platform/saves/fake_cloud_save_client.{cpp,hpp}` — DO NOT DELETE.**

- It is the host-doctest test double, not a desktop GUI stub.
- `tests/Makefile` line 3 explicitly lists `../source/platform/saves/fake_cloud_save_client.cpp` in `SRCS`.
- `tests/test_fake_cloud_save_client.cpp` includes `"platform/saves/fake_cloud_save_client.hpp"` directly.
- It is in `source/platform/saves/` (subdirectory `saves/`), not in `source/platform/` root — distinct from all 5 stubs being deleted.
- It has **no** `#ifndef __SWITCH__` guard: it compiles unconditionally and is designed to do so.
- No relationship whatsoever to the 5 stubs. The `find source/platform -name '*_fake*'` success criterion requires only `saves/fake_cloud_save_client.*` to remain — this is exactly what will be left after deletions.

---

## Section 4: Build Files

### `CMakeLists.txt` — No source-list changes in Phase 5

CMake uses `file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)` (line 111). This means:

- **No explicit source list to edit** for the deleted stub files — deleting the `.cpp` files from disk is sufficient; CMake will not find them on the next configure.
- **No `PLATFORM_DESKTOP` reference** exists in `source/` (grep returned nothing) — `PLATFORM_DESKTOP` appears only in `CMakeLists.txt` itself, which is Phase 6 scope.
- The `elseif (PLATFORM_DESKTOP)` link branch (lines 99-101) and the `if (PLATFORM_DESKTOP)` packaging branch (lines 130-135) are Phase 6, not Phase 5.

**Phase 5 makes zero changes to `CMakeLists.txt`.**

### `tests/Makefile` — No changes required

`tests/Makefile` compiles an **explicit, hand-curated source list** (not a glob). None of the 5 stubs appear in it. The only `source/platform/` files it references that could be affected:

| Makefile entry | Phase 5 action | Safe? |
|---------------|----------------|-------|
| `../source/platform/saves/fake_cloud_save_client.cpp` | RETAINED | Yes |
| `../source/platform/cheat_store.cpp` | Seam collapsed (path string) | Yes — test still compiles, path becomes unconditional Switch value (tests do not call `index_cache_path()` expecting the desktop path; they work with filesystem-relative paths via tmpdir) |
| `../source/platform/app_settings.cpp` | Seam collapsed (2 path strings) | Yes — `locale_file()` and `api_url_file()` become Switch paths; the tests for `test_api_base_url.cpp` test `load_api_base_url()` logic, not file-path values |
| `../source/platform/themes/cfw_paths.cpp` | Seam collapsed (2 path strings) | Yes — `test_cfw_paths.cpp` tests `target_map`/`base_szs_path`/`output_szs_path` which are independent of the `layeredfs_root()`/`base_layout_dir()` paths |
| `../source/platform/themes/theme_paths.cpp` | Seam collapsed | Yes |
| `../source/platform/sysmod/sysmod_store.cpp` | No seam, no change | Yes |
| `../source/platform/saves/save_backup_io.cpp` | No `__SWITCH__` seam | Yes |

**WARNING:** After the path seams are collapsed, `cfw_paths.cpp::layeredfs_root()` returns `/atmosphere/contents` and `base_layout_dir()` returns `/themes/systemData` unconditionally. The host tests in `test_cfw_paths.cpp` that exercise `base_present_for()` call `::stat()` on those paths — they will return `false` (paths do not exist on the host) which is the same behavior as before (the desktop strings were also absent on the test host). No regression.

Similarly, `cheat_store.cpp::index_cache_path()` becomes `/switch/thomaz/cache/versions.json` — the one test in `test_smoke.cpp` or similar that reads/writes the index cache uses `write_text_file`/`read_text_file` which will simply fail to create the path on the host. All current test behavior of these files depends on the test setup (tmpdir or not-found behavior), not on the specific path string value. Confirm with a test run after collapse.

---

## Section 5: The Host Doctest Suite Contract

`make -C tests test` compiles: `tests/*.cpp` + `source/core/**/*.cpp` + a selection of `source/platform/` files (explicit list, see `tests/Makefile` line 3).

**None of the following are in the test compile list:**
- Any of the 5 stubs being deleted
- `source/main.cpp`
- `source/app/home_activity.cpp`
- `source/platform/game_stats.cpp`
- `source/platform/http_client_curl.cpp`
- `source/platform/themes/firmware_extract_*.cpp`
- `source/platform/sysmod/system_reboot.cpp`
- `source/platform/system/reboot.cpp`
- `source/platform/curl_tls.hpp` (header-only, only pulled in by `http_client_curl.cpp`)

**Are in the test compile list (seams collapsed but tests still pass):**
- `source/platform/cheat_store.cpp` — `index_cache_path()` seam collapsed
- `source/platform/app_settings.cpp` — `locale_file()` and `api_url_file()` seams collapsed
- `source/platform/themes/cfw_paths.cpp` — `layeredfs_root()` and `base_layout_dir()` seams collapsed
- `source/platform/themes/theme_paths.cpp` — `themes_root()` seam collapsed
- `source/platform/sysmod/sysmod_store.cpp` — no `__SWITCH__` seam, unchanged
- `source/platform/saves/fake_cloud_save_client.cpp` — retained, unchanged

The host build does not define `__SWITCH__`. After seam collapse, the path strings in the 4 platform files above return the Switch SD paths unconditionally. The tests do not assert on those specific path values — they test the logic wrapping them. The suite passes unchanged.

**Verification command:** `make -C tests test` from repo root.

---

## Section 6: Deletion/Edit Checklist (Ordered for Safety)

The safe execution order is: **remove references first, then delete files**.

### Step 1: Edit `source/main.cpp` (remove includes + factory seams)

Remove `#ifdef __SWITCH__` / `#else` / `#endif` wrappers:
- Lines 7-14: keep the 3 Switch `#include` lines unconditionally; delete `#ifdef __SWITCH__`, `#else`, the 2 fake includes, and `#endif`
- Lines 70-79: keep `auto titleService = std::make_unique<thomaz::NsTitleService>();` + `init()` block unconditionally; delete `#ifdef __SWITCH__`, `#else`, fake factory, `#endif`
- Lines 82-86: keep `auto saveService = std::make_unique<thomaz::NsSaveService>();` unconditionally; delete `#ifdef __SWITCH__`, `#else`, fake factory, `#endif`
- Line 131-133: `#ifdef __SWITCH__ titleService->exit(); #endif` — leave intact (no `#else`, SIMPL-02 does not require removing it)

### Step 2: Edit `source/app/home_activity.cpp` (remove sysmod_store_fake include + factory seam)

- Lines 14-16: delete the 3-line block `#ifndef __SWITCH__` / `#include "platform/sysmod/sysmod_store_fake.hpp"` / `#endif` entirely
- Lines 94-98: remove `#ifdef __SWITCH__` guard; keep `auto store = std::make_shared<SysmoduleStore>();` unconditionally; delete `#else` + fake factory + `#endif`

### Step 3: Delete the 10 stub files

```
rm source/platform/save_service_fake.cpp
rm source/platform/save_service_fake.hpp
rm source/platform/title_service_fake.cpp
rm source/platform/title_service_fake.hpp
rm source/platform/fake_auth_client.cpp
rm source/platform/fake_auth_client.hpp
rm source/platform/themes/firmware_extract_fake.cpp
rm source/platform/sysmod/sysmod_store_fake.cpp
rm source/platform/sysmod/sysmod_store_fake.hpp
```

(9 deletions: `firmware_extract_fake` has no `.hpp`)

### Step 4: Collapse remaining `#else` desktop path branches (12 files)

For each file in Category A above (excluding the already-handled `main.cpp`, `home_activity.cpp`), remove the `#else` desktop branch and unwrap the `#ifdef __SWITCH__` guard leaving the Switch path unconditional. Files:

1. `source/platform/game_stats.cpp` — remove `#else // desktop` block (lines 39-55) + `#endif`; drop `#ifdef __SWITCH__` wrapper from lines 10-36; Switch implementation becomes sole body of `query_game_stats`
2. `source/platform/app_settings.cpp` — collapse `locale_file()` and `api_url_file()` helper functions (4 seams: 2 functions × 1 `#else` each)
3. `source/core/backup_store.cpp` — collapse `saves_root()`
4. `source/core/mods/mod_paths.cpp` — collapse `mod_staging_root()`
5. `source/platform/feed/auth_store.cpp` — collapse `session_file()`
6. `source/platform/saves/sync_store.cpp` — collapse `sync_file()`
7. `source/platform/cheat_store.cpp` — collapse `index_cache_path()`
8. `source/platform/themes/cfw_paths.cpp` — collapse `layeredfs_root()` and `base_layout_dir()`
9. `source/platform/themes/theme_paths.cpp` — collapse `themes_root()`
10. `source/platform/themes/qlaunch_patches.cpp` — collapse `patch_src_dir()` and `exefs_patches_dir()`; leave the `#ifdef __SWITCH__` warning block in `install_theme` intact (no `#else` there, only `(void)patches` — which needs the `#else` removed)
11. `source/platform/http_client_curl.cpp` — collapse the `networkReady = true` / `networkReady = true` identity seam (lines 32-42) to a single `networkReady = true;`
12. `source/platform/themes/theme_install.cpp` — remove `#else (void)patches; #endif` (lines 67-69); keep Switch warning branch unconditionally
13. `source/platform/sysmod/system_reboot.cpp` — remove `#else return false; #endif` block; keep Switch `spsmShutdown` block unconditional
14. `source/platform/curl_tls.hpp` — remove `#else` system-CA block (lines 65-68); keep Switch CA-bundle block unconditional; this is a header used only by `http_client_curl.cpp` (not compiled by the test suite)

### Step 5: Update 4 interface/header comments

Minor comment-only edits to remove stale desktop references (SIMPL-03 requires no source file references a deleted stub):

- `source/platform/save_service.hpp` lines 11-12: remove "the fake impl lets the full UI flow run on desktop without a console"
- `source/platform/auth_client.hpp` line 12: remove "FakeAuthClient runs on the desktop;" from the comment
- `source/platform/sysmod/sysmod_store.hpp` line 20: remove "the desktop fake (sysmod_store_fake) is in-memory" from the comment
- `source/platform/themes/firmware_extract.hpp` lines 45-47: remove the "Desktop behaviour (firmware_extract_fake.cpp): …" comment block

---

## Section 7: Build-Order and Dependency Risks

### Risk 1: References before deletion
Deleting the stub files before editing the consumer files would leave `main.cpp` and `home_activity.cpp` with dangling `#include` directives. A re-configure or incremental build would fail.
**Mitigation:** Always do Step 1 and Step 2 (edit consumers) before Step 3 (delete files).

### Risk 2: CMake glob staleness
CMake's `GLOB_RECURSE` caches the file list at configure time. After deleting `.cpp` files, a `cmake ..` re-configure is required before the Switch build (Phase 6). In Phase 5 there is no Switch build verification gate — only `make -C tests test`. The test suite uses `make` (not CMake), so it is unaffected by CMake glob staleness.
**Mitigation:** Document that Phase 6 must run `cmake -S . -B build_switch` fresh after Phase 5 deletions.

### Risk 3: `save_service_switch.hpp` is inside `#ifdef __SWITCH__` guard
`source/platform/save_service_switch.hpp` has `#ifdef __SWITCH__` around its class declaration (line 3). After removing the `#else` block in `main.cpp`, the `#include "platform/save_service_switch.hpp"` in `main.cpp` will be unconditional. On a non-Switch host build, this would try to include the file but find it wrapped in `#ifdef __SWITCH__` — making `NsSaveService` invisible. However: (a) this is a Switch-only project after Phase 6; (b) the test suite does not compile `main.cpp`; (c) the only remaining "host build" in the contract is `make -C tests test` which does not include `main.cpp`.
**Mitigation:** Safe. The tests don't compile `main.cpp`. Phase 6 removes the desktop CMake path entirely. No action needed.

### Risk 4: `settings_activity.cpp` and `self_update.cpp` have working-tree modifications
Per the git status, both files are modified. Neither contains any `__SWITCH__` seam (confirmed by reading them above — `settings_activity.cpp` has no preprocessor guards; `self_update.cpp` has no preprocessor guards). Phase 5 does not touch either file.
**Mitigation:** No interaction. Safe.

### Risk 5: `CMakeLists.txt` has working-tree modifications
Per git status `CMakeLists.txt` is modified (likely from a prior session). Phase 5 makes no changes to it. The existing modifications are Phase 6 scope (or unrelated).
**Mitigation:** Do not touch `CMakeLists.txt` in Phase 5.

### Risk 6: `fake_auth_client` has no `#ifndef __SWITCH__` guard
Unlike the other 4 stubs, `fake_auth_client.{cpp,hpp}` are unconditionally compiled into the Switch build today (the glob picks them up; there is no guard to exclude them). Deleting them removes dead code from the Switch binary's compile — no behavior change.
**Mitigation:** Delete both files. No consumer references them outside their own files. The `IAuthClient` interface and `HttpAuthClient` implementation are unaffected.

---

## Common Pitfalls

### Pitfall 1: Deleting `saves/fake_cloud_save_client.*`
**What goes wrong:** Agent conflates "files with `_fake` in the name" with "stubs to delete." The success criterion explicitly states `find source/platform -name '*_fake*'` must return only `saves/fake_cloud_save_client.{cpp,hpp}`.
**Prevention:** The deletion list has exactly 9 file paths (not 11). `fake_cloud_save_client.*` is NOT on it.

### Pitfall 2: Collapsing `#ifdef __SWITCH__` guards that have no `#else`
**What goes wrong:** Agent removes a `#ifdef __SWITCH__` + code + `#endif` block where there is no `#else`, wiping out Switch-only code (e.g., `titleService->exit()`, `setsysInitialize` calls, entire `_switch.cpp` file bodies).
**Prevention:** Only remove a `#ifdef __SWITCH__` guard when it has an `#else` desktop branch. Guards without `#else` may stay. Category B list above catalogs which ones to leave alone.

### Pitfall 3: Forgetting to update the 4 interface comments (SIMPL-03 failure)
**What goes wrong:** Grep for `_fake` in `source/` returns hits in `save_service.hpp`, `auth_client.hpp`, `sysmod_store.hpp`, `firmware_extract.hpp` even after file deletions — SIMPL-03 check fails.
**Prevention:** Step 5 above covers all 4 comment edits. Run the verification grep after all edits.

### Pitfall 4: Expecting `tests/Makefile` changes to be needed
**What goes wrong:** Planner adds a task to edit `tests/Makefile` to remove references to the 5 fakes.
**Prevention:** The test Makefile never listed any of the 5 fakes. No test changes needed.

### Pitfall 5: Confusing `save_service_fake` location with `saves/`
**What goes wrong:** Looking for the fake at `source/platform/saves/save_service_fake.*` instead of `source/platform/save_service_fake.*`.
**Prevention:** `save_service_fake.*` is in `source/platform/` (root of platform, not in the `saves/` subdirectory). Confirmed by the file listing above.

---

## Validation Architecture

### Test Gate
`make -C tests test` — the sole in-phase verification gate. Must be green before this phase is considered done.

- **Run from:** repo root (the Makefile uses `make -C tests test`)
- **What it exercises:** ~40 doctest test files covering core logic, platform-neutral utilities, and the retained `fake_cloud_save_client`
- **Does not exercise:** any of the deleted fakes, `main.cpp`, `home_activity.cpp`, or the Switch libnx layer

### Post-Step Checkpoints

| After Step | Verification |
|------------|-------------|
| Steps 1-2 (edits to consumers) | `grep -rn 'save_service_fake\|title_service_fake\|sysmod_store_fake' source/` returns nothing outside the stub files themselves |
| Step 3 (file deletions) | `find source/platform -name '*_fake*'` returns only `saves/fake_cloud_save_client.{cpp,hpp}` |
| Step 4 (seam collapse) | `grep -rn 'ifndef __SWITCH__' source/` returns nothing; `grep -rn '#else' source/` returns only `_WIN32`/`localtime_r` lines and nothing inside `#ifdef __SWITCH__` blocks |
| Step 5 (comment cleanup) | `grep -rnE '_fake|desktop' source/` returns only `fake_cloud_save_client` references |
| All steps | `make -C tests test` passes; `grep -rnE 'PLATFORM_DESKTOP|SDL2|GLFW|_fake' source/` returns only `fake_cloud_save_client` references |

---

## Sources

All claims in this document are verified by direct file read or grep against the working tree. No training-data assumptions.

- `source/main.cpp` — read in full; seams at lines 7-14, 70-79, 82-86, 131-133 confirmed
- `source/app/home_activity.cpp` — read in full; seams at lines 14-16, 94-98 confirmed
- All 10 stub files — read in full; guard style and class names confirmed
- `CMakeLists.txt` — read in full; `GLOB_RECURSE` at line 111 confirmed; `PLATFORM_DESKTOP` appears only at lines 98-101, 130-135
- `tests/Makefile` — read in full; explicit source list confirmed; no fake stubs listed
- `grep -rn 'ifndef __SWITCH__\|#else\|ifdef __SWITCH__'` across `source/` — 40+ matches classified
- `grep -rn 'fake_auth_client\|FakeAuthClient'` across `source/` and `tests/` — no usage sites outside own files
- `grep -rnE 'PLATFORM_DESKTOP|SDL2|GLFW'` across `source/` — zero results

---

## RESEARCH COMPLETE

**Phase:** 5 — Collapse Source Seams to Switch-Only
**Confidence:** HIGH

### Key Findings

- The 5 stubs exist at exactly the paths SIMPL-01 names (with the note that `firmware_extract_fake` has no `.hpp`). All paths confirmed.
- `fake_auth_client.{cpp,hpp}` has NO `#ifndef __SWITCH__` guard — it's dead code on Switch today, not a conditional compile. Deletion is still clean.
- `CMakeLists.txt` requires zero changes in Phase 5 — the glob handles deletion implicitly.
- `tests/Makefile` requires zero changes — none of the 5 stubs appear in its explicit source list.
- 22 `#else` desktop branches must be collapsed across 12 files, plus the 3 seams in `main.cpp` and `home_activity.cpp` = 15 files total.
- `saves/fake_cloud_save_client.*` is safe — it is in a distinct subdirectory, has no `#ifndef __SWITCH__` guard, and is explicitly listed in `tests/Makefile` as a compiled source.

### File Created
`.planning/phases/05-collapse-source-seams-switch-only/05-RESEARCH.md`

### Confidence Assessment
| Area | Level | Reason |
|------|-------|--------|
| Stub file paths | HIGH | Every file read directly; paths confirmed by `find` |
| Seam catalog | HIGH | Full grep across `source/`; every `#else` inspected |
| Test suite safety | HIGH | `tests/Makefile` SRCS list read in full; no fakes referenced |
| Comment cleanup scope | HIGH | All 4 interface headers read; stale references enumerated |
| Build-file impact | HIGH | CMakeLists read in full; `GLOB_RECURSE` behavior confirmed |

### Ready for Planning
Research complete. Planner can now create PLAN.md files for Phase 5.
