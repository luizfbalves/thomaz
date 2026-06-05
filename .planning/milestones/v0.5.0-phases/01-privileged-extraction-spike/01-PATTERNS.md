# Phase 1: Privileged Extraction Spike - Pattern Map

**Mapped:** 2026-06-04
**Files analyzed:** 8 (5 new source/lib, 1 doc, 1 build, 1 provenance)
**Analogs found:** 8 / 8

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `source/platform/themes/firmware_extract.hpp` | interface header | request-response | `source/platform/save_service.hpp` | role-match |
| `source/platform/themes/firmware_extract_switch.cpp` | platform service (real) | file-I/O + transform | `source/platform/save_service_switch.cpp` | exact |
| `source/platform/themes/firmware_extract_fake.cpp` | platform service (no-op) | file-I/O | `source/platform/save_service_fake.cpp` | exact |
| `lib/hactool/**` | vendored lib | transform | `lib/switchthemes/**` (Phase B `2cdec9d` vendoring) | exact |
| `lib/mbedtls/**` (or FetchContent) | vendored lib (build) | transform | `lib/switchthemes/**` + portlib `mbedtls` wiring | role-match |
| `CMakeLists.txt` (modified) | config | — | existing `if(PLATFORM_SWITCH)` curl/mbedtls block + `ENGINE_SRC` glob | exact |
| `THIRD_PARTY.md` (modified) | provenance doc | — | existing `## SwitchThemeInjector` / `## miniz` entries | exact |
| Title-takeover doc (new, e.g. `docs/title-takeover.md`) | user doc | — | RESEARCH "Title-Takeover Doc Skeleton" (no in-tree analog) | research-pattern |

## Pattern Assignments

### `source/platform/themes/firmware_extract.hpp` (interface header)

**Analog:** `source/platform/save_service.hpp` (platform-neutral interface; no libnx types — exactly the D-08 / Pitfall-4 requirement)

**Pattern to copy** (`save_service.hpp:1-9`): `#pragma once`, `<string>`/`<vector>` only, `namespace thomaz`, signatures use `std::string` / `std::vector<std::uint8_t>` / `std::uint64_t` — never libnx types. The header is included by BOTH `_switch.cpp` and `_fake.cpp` and must stay neutral.

Concrete shape for this phase (from RESEARCH "Component Responsibilities"):
```cpp
#pragma once
#include <string>
namespace thomaz {
struct ExtractResult { bool ok; std::string error; };
ExtractResult extract_base_layout(const std::string& target);
}
```
Note: thomaz uses free functions in `namespace thomaz` for theme helpers (see `cfw_paths.hpp`), not always a virtual interface class. Prefer the free-function shape of `cfw_paths.hpp` over the `ISaveService` vtable here, since there is one impl per platform selected by `#ifdef`, not runtime injection.

---

### `source/platform/themes/firmware_extract_switch.cpp` (platform service, real)

**Analog:** `source/platform/save_service_switch.cpp` — the canonical whole-file-`#ifdef` privileged libnx service (RESEARCH Pattern 1).

**Whole-file guard pattern** (`save_service_switch.cpp:1-5`, `:320-322`):
```cpp
#include "platform/themes/firmware_extract.hpp"  // neutral header, OUTSIDE guard

#ifdef __SWITCH__
#include <switch.h>            // ← ALL libnx + hactool/mbedtls includes inside guard
// ... real impl ...
#endif // __SWITCH__
```
Every `#include <switch.h>`, every hactool/mbedtls header, and the entire impl body live inside the guard. On desktop this file compiles to an empty TU (the GLOB_RECURSE compiles it on every platform — only the guard keeps it empty). This is THE pattern that satisfies D-08.

**libnx service init/exit + `Result` checking** (`save_service_switch.cpp:171-185`): wrap each service in `*Initialize()` / `*Exit()`, gate work on `R_SUCCEEDED(...)`, unmount in the failure path. Mirror this for the Phase 1 chain (RESEARCH Pattern 2): `pmdmntInitialize` → `splInitialize` → `splCryptoInitialize` → `fsOpenBisFileSystem(&sys, FsBisPartitionId_System, "")` → `fsdevMountDevice("System", sys)`, with reverse-order teardown. Check `R_FAILED`/`R_SUCCEEDED` on every call (ASVS V5; Pitfall 3).

**Applet gate runs FIRST** (RESEARCH Pattern 3 — no in-tree analog, this is new for this file):
```cpp
if (appletGetAppletType() != AppletType_Application) {
    return {false, "Relaunch thomaz via title takeover (hold R while opening a game) to extract."};
}
```
Place BEFORE any service init, before `fsOpenBisFileSystem` (Pitfall 3).

**Output write — reuse, do not invent** (verified `cfw_paths.cpp:34-38`):
```cpp
std::string out = thomaz::base_szs_path("ResidentMenu"); // /themes/systemData/ResidentMenu.szs
```
Call `ensure_parent_dirs(out)` first — copy the helper verbatim from `theme_install.cpp:38-44` (mkdir -p, FAT-safe). Write via the `write_file` pattern at `theme_install.cpp:30-35` (`std::ofstream`, binary|trunc). Do NOT use exelix's `extracted/{qlaunch}/` subdir layout (Pitfall 6).

**Validate before writing** (ASVS V5 / Pitfall 2 + integrity threat): check the hactool output buffer is non-empty and looks like a valid szs (SARC/`Yaz0` magic) before overwriting the canonical path — mirror the "never write a partial/empty artifact" discipline in `save_service_switch.cpp:214-218` (`clear_tree` + remove on empty result).

---

### `source/platform/themes/firmware_extract_fake.cpp` (platform service, no-op)

**Analog:** `source/platform/save_service_fake.cpp` — the desktop no-op half of the split.

**Pattern to copy** (`save_service_fake.cpp:1-3`, `:91-93`):
```cpp
#include "platform/themes/firmware_extract.hpp"
#ifndef __SWITCH__
namespace thomaz {
ExtractResult extract_base_layout(const std::string&) {
    return {false, "Firmware extraction is only available on Switch."};
}
} // namespace thomaz
#endif // !__SWITCH__
```
ZERO libnx/hactool/mbedtls includes (D-08). The fake half may include only pure/core headers (as `save_service_fake.cpp` includes only `core/*` + `<ctime>`).

---

### `lib/hactool/**` (vendored GPLv2 fork)

**Analog:** `lib/switchthemes/**` — Phase B's established exelix-vendoring shape (commit `2cdec9d`), with `apply_facade.{hpp,cpp}` as the single thomaz-facing entry point.

**Facade pattern** (`lib/switchthemes/README.md:11-14`): all upstream-API knowledge is confined to one facade file; the app calls only `apply_facade`. Mirror this — the hactool C library is driven from a thin C++ wrapper (the ported `hactool.cpp` glue, RESEARCH Pattern 4) that exposes a single `ExtractFiles(...)` -> `std::unordered_map<std::string, std::vector<u8>>`; the rest of thomaz never sees `nca_ctx_t`. The `firmware_extract_switch.cpp` is to hactool what `apply_facade.cpp` is to the theme engine.

**Vendoring README pattern** (`lib/switchthemes/README.md:1-8`): create `lib/hactool/README.md` recording upstream URL, pinned commit `2618b0c31e007d019757dc4095eca08b4a89e3f5`, imported path (`SwitchThemesNX/Libs/hactool`), license (GPLv2), and any adaptations (e.g. relaxing `-Werror` for toolchain drift — Assumption A2). Keep the `LICENSE` file in the vendored tree.

---

### `lib/mbedtls/**` (CMAC-enabled build, from source — D-06)

**Analog:** no exact in-tree analog (switchthemes is header/cpp, not a CMake-built C lib). Closest is the portlib `mbedtls` link wiring in `CMakeLists.txt:54`. Use the RESEARCH "CMake Wiring" pattern. Pin tag `mbedtls-2.28.10` (match portlib to avoid ABI drift — Pitfall 1). Build with `MBEDTLS_CMAC_C` ON, `MBEDTLS_ENTROPY_HARDWARE_ALT`+`MBEDTLS_NO_PLATFORM_ENTROPY` ON, self-test OFF (match the devkitPro PKGBUILD). Do NOT commit a prebuilt `.a` (D-06).

---

### `CMakeLists.txt` (modified)

**Analog:** the existing `if (PLATFORM_SWITCH)` portlib block (`CMakeLists.txt:44-55`) and the `ENGINE_SRC` vendored-glob (`:71-75`).

**Link-order pattern — load-bearing** (`CMakeLists.txt:47-55`): the existing comment documents that the static GNU linker resolves left-to-right (`curl -> mbedtls -> z`, archive before its filter deps). Insert the new libs INTO this ordering. Per RESEARCH, append `hactool` and the CMAC mbedtls target BEFORE the portlib `mbedtls` on `APP_PLATFORM_LIB` so hactool's `mbedtls_cipher_cmac_*` symbols resolve against the CMAC build (Pitfall 1). Add new work inside the existing `if (PLATFORM_SWITCH)` guard only — desktop must not see hactool/mbedtls.

**Vendored-source inclusion pattern** (`:71-75`): hactool can either be `add_subdirectory(lib/hactool)` producing a static target (preferred per RESEARCH) or glob'd like `ENGINE_SRC`. Add its include dir to `APP_PLATFORM_INCLUDE` mirroring `:64-68` (`${CMAKE_CURRENT_SOURCE_DIR}/lib/hactool/include`). Verify the `.nro` still builds with `-DUSE_DEKO3D=ON` (`:95`) and desktop with `-DUSE_SDL2=ON`.

---

### `THIRD_PARTY.md` (modified)

**Analog:** existing `## SwitchThemeInjector` and `## miniz` entries (`THIRD_PARTY.md:3-17`).

**Pattern to copy** (`:3-7`): a `## <name>` block with `- Source:` URL, `- Files:` vendored path, `- License:`. Add:
- `## hactool (NCA/RomFS extraction)` — exelix fork, pinned commit `2618b0c...`, `lib/hactool/`, GPLv2.
- `## mbedtls (CMAC build)` — Mbed-TLS, tag `mbedtls-2.28.10`, Apache-2.0/GPLv2.
- `## SPL key sources` — Atmosphère release+commit (D-07) AND the firmware version the spike ran against (`setsysGetFirmwareVersion`), satisfying Success Criterion #4.

---

### Title-takeover doc (new)

**Analog:** none in-tree. Use the RESEARCH "Title-Takeover Doc Skeleton" (TAKEOVER-02) verbatim as the base. Must record the Atmosphère key-source version/commit and the validated firmware version (D-07) — the same provenance written into `THIRD_PARTY.md`.

## Shared Patterns

### switch/fake whole-file `#ifdef` split (D-08)
**Source:** `source/platform/save_service_switch.cpp:1-5,320-322` + `save_service_fake.cpp:1-3,91-93`
**Apply to:** `firmware_extract_switch.cpp`, `firmware_extract_fake.cpp`, `firmware_extract.hpp`
The GLOB_RECURSE (`CMakeLists.txt:69`) compiles every cpp on every platform; the whole-body guard is the ONLY selection mechanism. Interface header stays libnx-free.

### Privileged libnx service discipline (`Result` + init/exit + fail-closed)
**Source:** `save_service_switch.cpp:171-185,200-218`
**Apply to:** `firmware_extract_switch.cpp`
Check every `R_*` result, unmount/exit on every exit path, never leave a partial artifact, surface a human-readable error string.

### Canonical output path + mkdir-p + atomic-ish write
**Source:** `cfw_paths.cpp:34-38` (`base_szs_path`), `theme_install.cpp:38-44` (`ensure_parent_dirs`), `theme_install.cpp:30-35` (`write_file`)
**Apply to:** `firmware_extract_switch.cpp` output step (D-03; Pitfall 6)

### exelix vendoring + single-facade entry
**Source:** `lib/switchthemes/README.md:1-14`, `lib/switchthemes/apply_facade.{hpp,cpp}`, Phase B commit `2cdec9d`
**Apply to:** `lib/hactool/` (pinned-commit README, GPLv2 LICENSE, one C++ facade confining the C API)

### Vendored provenance entry
**Source:** `THIRD_PARTY.md:3-17`
**Apply to:** hactool, mbedtls, SPL key sources (+ firmware version, D-07)

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| Applet-vs-Application gate (in `firmware_extract_switch.cpp`) | runtime guard | — | No existing thomaz code calls `appletGetAppletType()`; new pattern from RESEARCH Pattern 3 + exelix `PlatformSwitch.cpp`. |
| SPL key-derivation chain (in `firmware_extract_switch.cpp`) | crypto | transform | No SPL usage in tree; port faithfully from exelix `key_loader.cpp` @ `2618b0c` (RESEARCH Pattern 2). |
| `lib/mbedtls` CMake-built C lib | vendored build | — | No precedent for a from-source CMake static lib in tree; use RESEARCH CMake Wiring + devkitPro PKGBUILD config. |
| Title-takeover user doc | doc | — | No user-doc precedent; use RESEARCH skeleton. |

## Metadata

**Analog search scope:** `source/platform/`, `source/platform/themes/`, `lib/`, `CMakeLists.txt`, `THIRD_PARTY.md`
**Files scanned:** save_service (switch/fake/hpp), cfw_paths (cpp/hpp), theme_install.cpp, CMakeLists.txt, THIRD_PARTY.md, lib/switchthemes (README + facade)
**Pattern extraction date:** 2026-06-04
