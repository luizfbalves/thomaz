# Phase 1: Privileged Extraction Spike - Research

**Researched:** 2026-06-04
**Domain:** On-device, keyless-to-user firmware layout extraction on Nintendo Switch (libnx privileged FS/SPL chain + hactool/mbedtls port under CMake/devkitA64)
**Confidence:** HIGH for the mechanism, port surface, and build wiring (verified against the pinned exelix source, the devkitPro PKGBUILD, and the current libnx fs.h). The two hardware-only unknowns (firmware key derivation, takeover permission sufficiency) remain validation targets by design — that is the point of the spike.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Phase 1 produces **real, production-shaped, keeper code** — the actual port of `key_loader` (BIS mount + `lr` resolve + SPL key derivation) and the `hactool` in-memory NCA RomFS extractor. Phase 2 **extends** this (more titles/szs); it does **not** replace or rewrite it. The "spike" framing is about *scope* (one szs to validate hardware unknowns), not code quality or disposability.
- **D-02:** Extract exactly **one** szs: `ResidentMenu.szs` from the qlaunch title `0100000000001000`. One title → one file is the thinnest slice that still exercises the full BIS→lr→SPL→hactool chain.
- **D-03:** Write the extracted szs to the **canonical** location the rest of the app already consumes — `base_layout_dir()/ResidentMenu.szs` (`/themes/systemData/ResidentMenu.szs`) — NOT a throwaway dir.
- **D-04:** The spike uses and documents the **hbloader title override** path (hold-`R` while launching an installed game). A forwarder NSP is deferred to Phase 4 (TAKEOVER-03). Phase 1 only needs *a* working Application-mode launch path.
- **D-05:** Detect applet vs Application mode at runtime (`appletGetAppletType() != AppletType_Application`). In applet mode, show a clear "relaunch via title takeover" message and exit cleanly — no crash, no silent `fsOpenBisFileSystem` failure (TAKEOVER-01).
- **D-06:** Re-vendor the **hactool fork** AND a **custom `libmbedtls.a` built with `MBEDTLS_CMAC_C`** (reversing the Phase B exclusion). Build mbedtls **from source in CI/Docker (devkitA64)** under the existing CMake build — do **not** commit a prebuilt `.a` blob. Translate exelix's Makefile `LIBS`/`LIBDIRS`/`INCLUDES` wiring into CMake target wiring. Re-add hactool/mbedtls attribution to `THIRD_PARTY.md`.
- **D-07:** Pin the public SPL key **sources** from a specific, recorded Atmosphère release (version + commit). Record provenance — the Atmosphère source version AND the firmware version the spike actually ran against — in BOTH `THIRD_PARTY.md` and the user-facing title-takeover doc.
- **D-08:** Extraction entry point lives in `platform/themes/*_switch.cpp` (real impl) with a `platform/themes/*_fake.cpp` desktop no-op. No BIS/SPL/hactool symbols may be pulled into the desktop target — the desktop build must compile and link green.

### Claude's Discretion
- Exact wording of the applet-mode message and the surface used (default: a user-facing Borealis dialog, consistent with existing app dialogs, rather than only a log line).
- Internal file/function/class names for the ported `key_loader`/`hactool` code.
- Whether to port the optional `RomfsCache` now or defer to Phase 2 (default: defer the cache).

### Deferred Ideas (OUT OF SCOPE)
- **Forwarder NSP** (installable Home-menu icon, Application mode) → Phase 4 / TAKEOVER-03 (optional).
- **Full extraction** — all qlaunch szs (Entrance/Flaunch/Set/Notification/common) plus Psl and MyPage → Phase 2.
- **Theme UI action** ("Extrair layouts do firmware"), `base_missing` unblock, already-extracted state, firmware-version recording in UI, success/failure messaging → Phase 3.
- **`RomfsCache` port** → likely unnecessary for the spike; revisit in Phase 2.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EXTRACT-04 | Extraction succeeds without a user-supplied `prod.keys` (NCA header key derived on-device via SPL). | Verified mechanism: `splCryptoGenerateAesKek`/`splCryptoGenerateAesKey` derive the per-console header key from PUBLIC key sources on-device (see Architecture Patterns → Privileged Key Derivation Chain). No `prod.keys` involved. The header key is loaded directly into the hactool keyset (`settings->keyset.header_key`). |
| TAKEOVER-01 | If extraction is attempted in applet mode, the user sees a clear "relaunch via title takeover" message instead of a crash/silent failure. | Verified: privileged services (raw BIS/SPL/lr/pmdmnt) fail in applet mode. Detect via `appletGetAppletType() != AppletType_Application` and short-circuit BEFORE any `fsOpenBisFileSystem` call (see Architecture Patterns → Applet-vs-Application Gate). |
| TAKEOVER-02 | The required title-takeover launch path for privileged FS/SPL access is documented for users. | Verified: hbloader **title override** = hold-`R` while launching an installed game, keep holding through the Nintendo logo → relaunches hbloader-hosted NRO in Application mode with full memory + broad FS permissions (see Code Examples → Title-Takeover Doc Skeleton). |
</phase_requirements>

## Summary

The extraction mechanism (exelix "Option A") is a **proven, in-production** path, not theory: mount the raw BIS System partition, resolve the qlaunch title's NCA path through the Location Resolver (`lr`), derive the NCA header key on-device through SPL from **public** key sources (no `prod.keys`), then decrypt and extract the RomFS in-memory with a bundled hactool fork. The Phase 1 spike ports this faithfully but narrows scope to one file (`ResidentMenu.szs`, title `0100000000001000`) to validate the two hardware-only unknowns before Phase 2 builds the full engine on top.

Three things drive every planning decision. **First, the build wiring is the riskiest non-hardware task:** thomaz already links the devkitPro `switch-mbedtls` portlib (v2.28.10) for curl's HTTPS path, but that portlib is built **without** `MBEDTLS_CMAC_C` (verified in the devkitPro PKGBUILD — the Switch build omits it, unlike the 3DS build). hactool needs CMAC. So Phase 1 must build a **second, CMAC-enabled mbedtls from source** and ensure *that* one wins on the static link line without breaking curl. **Second, the switch/fake split is real but the existing CMakeLists `GLOB_RECURSE`s every `source/**/*.cpp`** into one target — the desktop-vs-switch selection is done by wrapping each file's entire body in `#ifdef __SWITCH__` / `#ifndef __SWITCH__` (the established `save_service_switch.cpp` / `save_service_fake.cpp` pattern), NOT by CMake source exclusion. The `*_fake.cpp` no-op must therefore pull in zero BIS/SPL/hactool headers. **Third, the entry point must gate on applet mode before touching any privileged service** — `appletGetAppletType() != AppletType_Application` short-circuits to a Borealis dialog, otherwise `fsOpenBisFileSystem` fails opaquely.

The output target already exists: `cfw_paths::base_szs_path("ResidentMenu")` resolves to exactly `/themes/systemData/ResidentMenu.szs`, the flat path `theme_install::base_present_for()` reads. The spike writes there directly, so a successful run is immediately reusable (D-03). Note exelix's UI writes to a subdir layout (`themes/systemData/extracted/{qlaunch,...}/`) — Phase 1 must adapt the write to thomaz's flat layout.

**Primary recommendation:** Vendor the hactool fork (49 files) under `lib/hactool/` and a CMAC-enabled mbedtls **2.28.10** (matching the portlib version to avoid symbol drift on curl's link path), wire both as CMake static libs built only when `PLATFORM_SWITCH`, port `key_loader` + a minimal `hactool` filter wrapper into `platform/themes/firmware_extract_switch.cpp` with a `firmware_extract_fake.cpp` no-op, gate the entry point on `AppletType_Application`, and write `ResidentMenu.szs` to `base_szs_path("ResidentMenu")`.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Raw BIS System mount | Switch/Desktop I/O (`platform/themes/*_switch.cpp`) | — | libnx privileged FS; only valid in Application mode. Desktop = no-op. |
| Title→NCA path resolve (`lr`) | Switch I/O (`*_switch.cpp`) | — | libnx `lr` service; privileged, hardware-only. |
| SPL header-key derivation | Switch I/O (`*_switch.cpp`) | — | Uses hardware key slots via SPL; cannot run off-console. |
| NCA→RomFS decrypt + filter | Vendored lib (`lib/hactool/`) invoked from `*_switch.cpp` | — | Pure C crypto/parsing in the hactool fork; driven by the platform layer. |
| Title-ID/szs target mapping | Pure core / `cfw_paths` (`platform/themes/cfw_paths.cpp`) | — | Already host-testable; `target_map("ResidentMenu")` exists. NOT a privileged concern. |
| Output path resolution | `platform/themes/cfw_paths.cpp` | — | `base_szs_path()` already gives the canonical `/themes/systemData/ResidentMenu.szs`. |
| Applet-vs-Application gate | Switch I/O (`*_switch.cpp`) | App layer (Borealis dialog) | `appletGetAppletType()` is a runtime check; the message surface is a Borealis dialog (UI tier). |
| Desktop no-op facade | Platform layer (`*_fake.cpp`) | — | Keeps desktop green; returns a "not supported on desktop" result. |
| mbedtls CMAC build | Build system (CMake + devkitA64) | CI/Docker | Reproducible from-source build; not committed as a blob (D-06). |

## Standard Stack

This phase **ports vendored C/C++ source**; it does not install ecosystem packages. The "stack" is the vendored libraries and the libnx services they call.

### Core (vendored / re-vendored)
| Component | Version / Pin | Purpose | Why This |
|-----------|---------------|---------|----------|
| hactool fork | exelix `SwitchThemesNX/Libs/hactool` @ `2618b0c` | In-memory NCA→RomFS decrypt + filename filter | This exact fork adds the in-memory extraction + filter callbacks the keyless flow depends on. Upstream SciresM hactool writes to disk; this fork dumps to a buffer. [CITED: exelix Libs/hactool/README] |
| mbedtls (CMAC build) | **2.28.10** built with `MBEDTLS_CMAC_C` | AES-CMAC for hactool key/NCA verification | Must match the portlib version (2.28.10) so it doesn't clash with curl's mbedtls on the static link line; the portlib build omits `MBEDTLS_CMAC_C`. [VERIFIED: devkitPro pacman-packages PKGBUILD] |
| `key_loader.cpp` (`__SWITCH__` branch) | exelix @ `2618b0c` | BIS mount + `lr` resolve + SPL key derivation | The privileged chain; ported faithfully per D-01. [CITED: exelix key_loader.cpp] |
| `hactool.cpp` wrapper | exelix @ `2618b0c` | C++ glue: builds filter list, drives `nca_process`, captures dumped files | Bridges the C hactool lib to a `std::unordered_map<std::string, std::vector<u8>>` result. [CITED: exelix hactool.cpp] |
| SPL public key sources | Pinned Atmosphère release (D-07) | `header_kek_source`, `header_key_source`, `key_area_key_application_source` | PUBLIC sources copied from Atmosphère's FS crypto config; SPL turns them into per-console keys. NOT secret. [CITED: exelix key_loader.cpp; switchbrew NCA] |

### Supporting (already present — do not re-add)
| Component | Version | Purpose | Note |
|-----------|---------|---------|------|
| libnx (devkitA64) | from `devkitpro/devkita64` CI image | `fsOpenBisFileSystem`, `lr*`, `spl*`, `pmdmnt*`, `appletGetAppletType` | Already linked (`-lnx`). No new portlib install. |
| `SwitchThemesCommon/Common.{cpp,hpp}` | already vendored at `lib/switchthemes/` | `ThemeTargetInfo` title-ID/szs tables | Tables already in tree; Phase 2 reconciles them with `cfw_paths::target_map()`. Phase 1 hardcodes the one target. |
| `cfw_paths` | `source/platform/themes/cfw_paths.cpp` | `base_szs_path("ResidentMenu")` = output path | Reuse directly. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Re-vendoring hactool+mbedtls | `fsOpenFileSystemWithId` keyless mount | EXTRACTION.md confirms exelix does NOT use this and it's explicitly deferred to a future "Option B" fallback. Out of scope for Phase 1 (locked to Option A). |
| Building mbedtls from source | Committing a prebuilt `libmbedtls.a` | Explicitly forbidden by D-06 (reproducibility). |
| Separate CMAC mbedtls build | Patching the portlib in place | Portlib is shared with curl; rebuilding it risks the HTTPS path and isn't reproducible in CI. Build a second, isolated lib. |

**Installation:** No package installs. Re-vendoring is `git`-tracked source under `lib/`. mbedtls source is fetched/pinned at a known 2.28.10 tag and built by CMake (see Architecture Patterns → CMake Wiring).

**Version verification:**
- mbedtls portlib version confirmed **2.28.10** via the devkitPro Switch PKGBUILD (`pkgver=2.28.10`), built with `aarch64-none-elf-cmake`, `-DENABLE_PROGRAMS=FALSE -DENABLE_TESTING=FALSE`, config sets `MBEDTLS_ENTROPY_HARDWARE_ALT` + `MBEDTLS_NO_PLATFORM_ENTROPY`, unsets `MBEDTLS_SELF_TEST`, and **does NOT set `MBEDTLS_CMAC_C`** (the 3DS PKGBUILD does). [VERIFIED: devkitPro pacman-packages]
- libnx `fsOpenBisFileSystem(FsFileSystem *out, FsBisPartitionId partitionId, const char *string)` with `FsBisPartitionId_System = 31` confirmed against current `fs.h`. The `FsBisStorageId`→`FsBisPartitionId` rename landed in libnx v3.0.0; exelix @2618b0c already uses the new name, so no API drift on that call. [VERIFIED: switchbrew libnx fs.h docs]

## Package Legitimacy Audit

> This phase installs **no** registry packages. All external code is vendored C/C++ source from pinned, version-controlled origins. The legitimacy concern here is *source provenance*, not registry slopsquatting.

| Source | Origin | Pin | Provenance check | Disposition |
|--------|--------|-----|------------------|-------------|
| hactool fork | github.com/exelix11/SwitchThemeInjector `SwitchThemesNX/Libs/hactool` | commit `2618b0c31e007d019757dc4095eca08b4a89e3f5` | Same upstream as already-vendored `lib/switchthemes/` (GPLv2); fork of SciresM/hactool | Approved — vendor at pinned commit |
| mbedtls | github.com/Mbed-TLS/mbedtls | tag `mbedtls-2.28.10` (match portlib) | Official upstream; same version devkitPro ships | Approved — build from source, pinned tag |
| SPL key sources | Atmosphère (Atmosphere-NX) FS crypto config | pinned release+commit (D-07) | PUBLIC key sources; official Atmosphère release | Approved — record version in THIRD_PARTY.md + takeover doc |

**Packages removed due to slopcheck [SLOP] verdict:** none (no registry packages).
**Packages flagged as suspicious [SUS]:** none.

*slopcheck/npm/pip are not applicable — there is no package registry surface in this phase. The planner should still gate the mbedtls source-tag pin and the Atmosphère release pin behind explicit, recorded version numbers (not "latest").*

## Architecture Patterns

### System Architecture Diagram

```text
  [User holds R + launches a game]  ──hbloader title override──▶  thomaz runs in APPLICATION mode
                                                                          │
                                                                          ▼
                          ┌──────────────────────────────────────────────────────────┐
                          │  platform/themes/firmware_extract_switch.cpp (real impl)   │
                          └──────────────────────────────────────────────────────────┘
                                                          │
                  appletGetAppletType() ─── != Application ──▶  Borealis dialog "relaunch via
                          │ == Application                       title takeover" → return cleanly
                          ▼                                       (TAKEOVER-01)
        ┌─────────────────────────────────────┐
        │ 1. pmdmntInitialize                 │
        │    splInitialize / splCryptoInit    │   service init order (verified)
        │    fsOpenBisFileSystem(System,31)   │──▶ encrypted NCAs on /System partition
        │    fsdevMountDevice("System", …)    │
        └─────────────────────────────────────┘
                          │
                          ▼
        ┌─────────────────────────────────────┐
        │ 2. lrInitialize                     │   title 0100000000001000
        │    lrOpenLocationResolver(BuiltInSys)│──▶ "@SystemContent://…nca"
        │    lrLrResolveProgramPath(id,path)  │      rewrite → "System:/Contents/…nca"
        └─────────────────────────────────────┘
                          │
                          ▼
        ┌─────────────────────────────────────┐
        │ 3. splCryptoGenerateAesKek(         │   PUBLIC key sources (no prod.keys)
        │       header_kek_source) → tempkek  │
        │    splCryptoGenerateAesKey(tempkek, │──▶ per-console header_key (32 bytes)
        │       header_key_source) → header_key│      (EXTRACT-04)
        └─────────────────────────────────────┘
                          │ memcpy header_key → hactool keyset
                          ▼
        ┌─────────────────────────────────────┐
        │ 4. hactool fork (lib/hactool/)      │   nca_ctx_t + ACTION_INFO|EXTRACT|MEMORYONLY
        │    romfs_filter = FileFilterFunction│──▶ keep only "/lyt/ResidentMenu.szs"
        │    extraction_file_stream_cb =      │      OnFileDumped → in-memory buffer
        │       OnFileDumped                  │
        └─────────────────────────────────────┘
                          │
                          ▼
        write buffer → cfw_paths::base_szs_path("ResidentMenu")
                       == /themes/systemData/ResidentMenu.szs   (D-03)
                          │
                          ▼
        theme_install::base_present_for(["ResidentMenu"]) now returns true
```

Desktop path: `firmware_extract_fake.cpp` returns `{ok:false, error:"extraction is Switch-only"}` and pulls in **zero** libnx/hactool/mbedtls headers.

### Component Responsibilities
| File (proposed) | Responsibility | Platform |
|------|----------------|----------|
| `source/platform/themes/firmware_extract.hpp` | Public interface: `ExtractResult extract_base_layout(const std::string& target)` + `struct ExtractResult{bool ok; std::string error; ...}` | both |
| `source/platform/themes/firmware_extract_switch.cpp` | Real impl: applet gate → port of `key_loader` ctx → hactool wrapper → write to `base_szs_path` | `#ifdef __SWITCH__` |
| `source/platform/themes/firmware_extract_fake.cpp` | No-op returning a clear "not supported on desktop" result | `#ifndef __SWITCH__` |
| `lib/hactool/**` | Vendored 49-file hactool fork (built as CMake static lib, Switch only) | Switch only |
| `lib/mbedtls/**` (or fetched at configure) | CMAC-enabled mbedtls 2.28.10 source (built as CMake static lib, Switch only) | Switch only |

### Recommended Project Structure
```
source/platform/themes/
├── firmware_extract.hpp           # interface (both platforms)
├── firmware_extract_switch.cpp    # real impl  (#ifdef __SWITCH__)
├── firmware_extract_fake.cpp      # desktop no-op (#ifndef __SWITCH__)
├── cfw_paths.{hpp,cpp}            # EXISTING — output path source of truth
└── theme_install.{hpp,cpp}       # EXISTING — downstream consumer (Phase 3 unblocks)
lib/
├── hactool/                       # re-vendored fork (49 files) — CMake static lib
│   ├── include/  source/  LICENSE
└── mbedtls/                       # CMAC build (or fetched via CMake at configure) — static lib
```

### Pattern 1: switch/fake split via whole-file `#ifdef` (NOT CMake exclusion)
**What:** thomaz's root `CMakeLists.txt` does `file(GLOB_RECURSE MAIN_SRC source/*.cpp)` — every cpp is compiled into the one target on every platform. The real/fake selection is done by wrapping the **entire body** of each file in a platform guard, so the wrong-platform file compiles to an empty translation unit.
**When to use:** Always, for the `*_switch.cpp` / `*_fake.cpp` pair. This is the established pattern.
**Example (verified from `save_service_switch.cpp` / `save_service_fake.cpp`):**
```cpp
// firmware_extract_switch.cpp
#include "platform/themes/firmware_extract.hpp"
#ifdef __SWITCH__
#include <switch.h>           // ← libnx headers ONLY inside the guard
// ... real impl ...
#endif

// firmware_extract_fake.cpp
#include "platform/themes/firmware_extract.hpp"
#ifndef __SWITCH__
namespace thomaz {
ExtractResult extract_base_layout(const std::string&) {
    return {false, "Firmware extraction is only available on Switch."};
}
} // namespace thomaz
#endif
```
**Critical for D-08:** No `#include <switch.h>`, no hactool/mbedtls header, may appear OUTSIDE a `#ifdef __SWITCH__` guard. The interface header (`firmware_extract.hpp`) must be platform-neutral (no libnx types in the signature — use `std::string`/`std::vector<unsigned char>`).

### Pattern 2: Privileged Key Derivation Chain (EXTRACT-04)
**What:** Service init order matters and is verified from the reference. Init: `pmdmntInitialize()` → `splInitialize()` → `splCryptoInitialize()` → `fsOpenBisFileSystem(&sys, FsBisPartitionId_System, "")` → `fsdevMountDevice("System", sys)`. Exit (reverse-ish): `fsdevUnmountDevice("System")`, `fsFsClose(&sys)`, `pmdmntExit()`, `splCryptoExit()`, `splExit()`.
**Key derivation (no prod.keys):**
```cpp
// Source: exelix key_loader.cpp __SWITCH__ branch @ 2618b0c [CITED]
splCryptoGenerateAesKek(header_kek_source, 0, 0, tempheaderkek);
splCryptoGenerateAesKey(tempheaderkek, header_key_source,        header_key);
splCryptoGenerateAesKey(tempheaderkek, header_key_source + 0x10, header_key + 0x10);
// header_key (0x20 bytes) is then memcpy'd into the hactool keyset:
//   memcpy(settings->keyset.header_key, header_key, sizeof(header_key));
```
Key source sizes: `header_key_source[0x20]`, `header_kek_source[0x10]`, `key_area_key_application_source[0x10]`. These are PUBLIC (from Atmosphère FS crypto config) — pin per D-07.

### Pattern 3: Applet-vs-Application Gate (TAKEOVER-01) — runs FIRST
**What:** Check applet type before any privileged call. In applet mode, the BIS/SPL/lr/pmdmnt services fail; without the gate, `fsOpenBisFileSystem` fails opaquely.
```cpp
// Source: pattern from exelix PlatformSwitch.cpp [CITED] + libnx applet.h
if (appletGetAppletType() != AppletType_Application) {
    return {false, "Relaunch thomaz via title takeover (hold R while opening a game) to extract."};
}
```
The discretionary surface (D-05) is a Borealis dialog matching existing app dialogs.

### Pattern 4: hactool in-memory filtered extraction
**What:** The fork adds a filename filter and a per-file dump callback so RomFS files land in a buffer, not on disk.
```cpp
// Source: exelix hactool.cpp @ 2618b0c [CITED]
nca_ctx->tool_ctx->action = ACTION_INFO | ACTION_EXTRACT | ACTION_MEMORYONLY;
nca_ctx->tool_ctx->settings.romfs_filter = FileFilterFunction;          // keep only /lyt/ResidentMenu.szs
nca_ctx->tool_ctx->settings.extraction_file_stream_cb = OnFileDumped;    // (void*, const char* name, unsigned char* data, size_t len)
// public entry: ExtractFiles(u64 contentId, std::vector<std::string> files)
//   -> std::unordered_map<std::string, std::vector<u8>>
nca_init(nca_ctx); nca_process(nca_ctx); nca_free_section_contexts(nca_ctx);
```
For the spike, the filter list is the single entry `/lyt/ResidentMenu.szs`.

### CMake Wiring (translating exelix's Makefile)
Exelix Makefile wiring (verified): `LIBS = -lhactool $(CURDIR)/../Libs/mbedtls/lib/libmbedtls.a -lstdc++fs -lcurl -lz -lglfw3 -lEGL -lglapi -ldrm_nouveau -lnx -lm`; `LIBDIRS = $(PORTLIBS) $(LIBNX) Libs/hactool Libs/mbedtls`. **The GLFW/EGL/glapi/drm_nouveau libs do NOT transfer** — thomaz uses deko3d (`USE_DEKO3D=ON` in CI), and Borealis already owns the graphics link line. Only `hactool` + the CMAC mbedtls are new.

Translation to thomaz's `CMakeLists.txt` (inside the existing `if (PLATFORM_SWITCH)` block):
```cmake
# Build CMAC-enabled mbedtls 2.28.10 from source as a static lib (Switch only).
# Either add_subdirectory() a pinned mbedtls checkout under lib/mbedtls with
# MBEDTLS_CMAC_C enabled in its config, or ExternalProject/FetchContent at a
# pinned tag. Must NOT replace the portlib mbedtls curl links against — give it
# a distinct target and ensure it precedes the portlib on the static link line.
add_subdirectory(lib/hactool)          # produces a `hactool` static target
# hactool's includes:
list(APPEND APP_PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/lib/hactool/include)
# Link order on the GNU static linker matters. The CMAC mbedtls must resolve
# hactool's CMAC symbols; keep curl's mbedtls for the HTTPS path. Verify no
# duplicate-symbol or "undefined reference to mbedtls_cipher_cmac" errors.
list(APPEND APP_PLATFORM_LIB hactool thomaz_mbedtls_cmac)   # before curl/mbedtls
```
**Link-order landmine:** the existing comment in `CMakeLists.txt` already documents that static GNU link order is load-bearing (`curl -> mbedtls -> z`, archive before its filters). Adding a second mbedtls is the highest-risk integration point — see Pitfall 1.

### Anti-Patterns to Avoid
- **Putting libnx/hactool includes in the shared header or fake impl.** Breaks the desktop build (violates D-08). Keep the interface header type-neutral.
- **Rebuilding/replacing the portlib mbedtls in place.** It's shared with curl; build an isolated second lib instead.
- **Calling `fsOpenBisFileSystem` before the applet check.** Produces the opaque failure TAKEOVER-01 exists to prevent.
- **Writing to a `/thomaz_spike/` dir.** Violates D-03; write to `base_szs_path("ResidentMenu")`.
- **Pinning mbedtls/Atmosphère to "latest"/HEAD.** Violates D-06/D-07 reproducibility; use explicit tags/commits.
- **Vendoring SciresM upstream hactool instead of the exelix fork.** Upstream writes to disk and lacks the in-memory filter/callback the keyless flow needs.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| NCA decryption + RomFS parse | A custom NCA/RomFS reader | The exelix hactool fork | NCA crypto (AES-XTS sectors, header key, IVFC RomFS) is deep and error-prone; the fork is in-production. |
| AES-CMAC | A hand-rolled CMAC | `MBEDTLS_CMAC_C` in mbedtls | Crypto primitive; never hand-roll. |
| Per-console key derivation | Bundling decrypted keys / asking for prod.keys | SPL (`splCrypto*`) on-device | SPL uses hardware key slots; keyless-to-user (EXTRACT-04) and avoids shipping copyrighted keys. |
| Title→NCA path resolution | Scanning the System partition manually | `lr` Location Resolver | `lr` is the supported resolution path; manual scanning is fragile across firmware. |
| Output path | New path constant | `cfw_paths::base_szs_path("ResidentMenu")` | Already the canonical path the consumer reads. |
| Title-ID/szs tables | New enum | `ThemeTargetInfo` (already vendored) | Tables already in tree at `lib/switchthemes/Common.cpp`. |

**Key insight:** Every privileged step here has an exact, in-production reference at a pinned commit. The value of Phase 1 is faithful porting + hardware validation, not reinvention.

## Runtime State Inventory

> This is a code/build phase that **produces** a new file; it does not rename or migrate existing runtime state. Most categories are N/A, but the build/artifact category is load-bearing.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — the spike writes ONE new file (`/themes/systemData/ResidentMenu.szs`); it reads, but does not mutate, the System BIS partition. | Write new file only. |
| Live service config | None — no external service config touched. The hbloader title-override behavior is user-side launch, not stored thomaz config. | Document the launch method (TAKEOVER-02); no config write. |
| OS-registered state | **Title takeover is an Atmosphère/hbloader launch mode, not a registration.** No `override_config.ini` edits are written *by* thomaz in Phase 1 (forwarder/registration is Phase 4). | None for Phase 1 — document the manual hold-R path only. |
| Secrets/env vars | None. The SPL key *sources* are PUBLIC and compiled into the binary; no secret, no env var, no `prod.keys`. | Record source provenance in THIRD_PARTY.md (D-07). |
| Build artifacts | **The portlib `libmbedtls.a` lacks `MBEDTLS_CMAC_C`; a new CMAC-enabled `libmbedtls.a` must be built and must coexist on the link line with the portlib one curl uses.** Adding `lib/hactool` introduces a new static lib + ~49 source files to the build graph. CI image is `devkitpro/devkita64` (no dkp-pacman step). | New CMake static-lib targets; verify CI build still produces `thomaz.nro` with `-DUSE_DEKO3D=ON`. |

## Common Pitfalls

### Pitfall 1: Two mbedtls libraries on one static link line
**What goes wrong:** curl links the portlib mbedtls (no CMAC); hactool needs CMAC. If the CMAC build doesn't take precedence, you get `undefined reference to mbedtls_cipher_cmac*`. If both define the same symbols and the linker pulls objects from both, you get duplicate-symbol errors or — worse — silent ABI mismatch if versions differ.
**Why it happens:** GNU static linker resolves left-to-right and pulls whole archive members on first unresolved symbol; order is load-bearing (thomaz's own CMakeLists already documents this for curl/archive).
**How to avoid:** Build the CMAC mbedtls at the **same version (2.28.10)** as the portlib so symbols are ABI-compatible, place it **before** the portlib `mbedtls` on the link line, and confirm the CMAC build is otherwise config-compatible (entropy alt set, self-test off — match the PKGBUILD). Prefer a single mbedtls if feasible: a CMAC-enabled 2.28.10 can serve curl too, eliminating the duplication — evaluate dropping the portlib mbedtls from the link line entirely.
**Warning signs:** `undefined reference to mbedtls_cipher_cmac_*`; duplicate-symbol errors; curl HTTPS suddenly failing on hardware after the build change.

### Pitfall 2: Firmware key-source mismatch (HARDWARE — highest risk, by design)
**What goes wrong:** The pinned public key sources fail to derive a valid header key on the target firmware → garbage decrypt → empty/invalid `ResidentMenu.szs`.
**Why it happens:** Newer master keygens may require updated header-key sources; the spike runs against one specific firmware.
**How to avoid:** This is exactly the unknown the spike validates. Pin the Atmosphère release the sources came from (D-07) and **record the firmware version the spike ran against** (`setsysGetFirmwareVersion()`). If derivation fails, the fix is updated sources from a newer Atmosphère release — not a code redesign.
**Warning signs:** hactool reports a bad NCA magic/header; output szs is empty or not valid SARC/`Yaz0`/`SZS`.

### Pitfall 3: Privileged call before the applet gate
**What goes wrong:** `fsOpenBisFileSystem` returns a libnx `Result` error in applet mode; if unchecked, the app either crashes or silently produces no file (the exact TAKEOVER-01 failure).
**Why it happens:** Applet mode lacks the FS/SPL permissions title takeover grants.
**How to avoid:** Check `appletGetAppletType() != AppletType_Application` and return the dialog BEFORE service init. Also check the `Result` of every libnx call and surface a clean error.
**Warning signs:** Works in Application mode, fails/empty in album-launched applet mode.

### Pitfall 4: Desktop target pulls in a Switch symbol (breaks D-08 / Success Criterion #2)
**What goes wrong:** Desktop build fails to compile/link because a libnx/hactool/mbedtls header or symbol leaked outside `#ifdef __SWITCH__`.
**Why it happens:** The CMake `GLOB_RECURSE` compiles `firmware_extract_switch.cpp` on desktop too — only the whole-file guard keeps it empty.
**How to avoid:** Mirror `save_service_switch.cpp` exactly: every include and every line of the real impl inside the guard; interface header carries no libnx types. Verify with a desktop build (`-DUSE_SDL2=ON`).
**Warning signs:** Desktop CMake/link errors referencing `fs`/`spl`/`lr`/`nca_*`/`mbedtls_cmac`.

### Pitfall 5: `lr` resolve path-rewrite / signature drift across firmware
**What goes wrong:** `lrLrResolveProgramPath` returns an unexpected path form, or the `@SystemContent://` → `System:/Contents/` rewrite doesn't match, yielding a "file not found" before hactool even runs.
**Why it happens:** Firmware/libnx differences in the resolver path format.
**How to avoid:** Port the rewrite exactly from the reference; log the raw resolved path on hardware during the spike so a mismatch is visible. EXTRACTION.md flags this as a hardware-only open question.
**Warning signs:** NCA path resolves but `System:/Contents/...nca` open fails.

### Pitfall 6: Writing to exelix's subdir layout instead of thomaz's flat layout
**What goes wrong:** File written to `themes/systemData/extracted/qlaunch/ResidentMenu.szs` (exelix's layout) instead of `/themes/systemData/ResidentMenu.szs` (thomaz flat); `base_present_for(["ResidentMenu"])` then still returns false.
**Why it happens:** The reference `NcaDumpPage.cpp` writes to `extracted/{qlaunch,...}/`.
**How to avoid:** Ignore exelix's output path; write to `cfw_paths::base_szs_path("ResidentMenu")` (D-03). `ensure_parent_dirs` first (the helper already exists in `theme_install.cpp`).
**Warning signs:** Extraction "succeeds" but a later apply still reports `base layouts missing`.

## Code Examples

### Output path (reuse existing — verified in tree)
```cpp
// cfw_paths.cpp (existing) — gives exactly /themes/systemData/ResidentMenu.szs on Switch
std::string out = thomaz::base_szs_path("ResidentMenu");   // base_layout_dir() + "/ResidentMenu.szs"
// base_layout_dir() == "/themes/systemData" on Switch
```

### Consumer that unblocks once the file exists (existing — verified in tree)
```cpp
// theme_install.cpp (existing): once /themes/systemData/ResidentMenu.szs is present,
// base_present_for(["ResidentMenu"]) returns true (uses ::stat + S_ISREG).
bool ok = thomaz::base_present_for({"ResidentMenu"});
```

### Firmware version recording for provenance (D-07)
```cpp
// Source: libnx setsys + exelix Common.hpp usage [CITED]
SetSysFirmwareVersion fw{};
setsysGetFirmwareVersion(&fw);   // fw.major/minor/micro → record in THIRD_PARTY.md + takeover doc
```

### Title-Takeover Doc Skeleton (TAKEOVER-02)
```text
To extract firmware layouts, thomaz must run in Application mode (title takeover):
1. From the Switch Home menu, HOLD the R button.
2. While holding R, launch any installed GAME (not the Album/Gallery).
3. Keep holding R through the Nintendo logo. The Homebrew menu opens in Application mode.
4. Launch thomaz from there, then run "Extrair layouts do firmware".
(Album-launched / applet mode lacks the FS/SPL permissions and will show a relaunch prompt.)
Record: Atmosphère <version/commit> key sources; firmware <major.minor.micro> the spike validated.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `FsBisStorageId` | `FsBisPartitionId` (e.g. `_System = 31`) | libnx v3.0.0 | exelix @2618b0c already uses the new name — no port change needed. |
| Ship/import `prod.keys` | On-device SPL derivation from public sources | exelix Option A (in production) | Keyless to the user (EXTRACT-04). |
| hactool writes to disk | exelix fork: in-memory buffer + filter callback | exelix fork | Enables capturing one szs without dumping a full RomFS to SD. |

**Deprecated/outdated:**
- The earlier research hypothesis that extraction used `fsOpenFileSystemWithId`/`romfsMountFromStorage` — EXTRACTION.md confirms those symbols do NOT appear in the repo; the real path is BIS+lr+SPL+hactool.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | A single CMAC-enabled mbedtls 2.28.10 *could* replace the portlib mbedtls for curl too (eliminating the dual-lib problem). | Pitfall 1 | If curl needs a portlib-specific patch (e.g. the `csrngGetRandomBytes` entropy glue), reusing one lib may break HTTPS. Mitigation: keep two libs if unsure; planner should treat "single lib" as an optimization to validate, not a given. |
| A2 | The hactool fork compiles cleanly under the current devkitA64 toolchain with `-march=armv8-a+crc+crypto`. | Standard Stack | Toolchain drift since exelix's build could surface `-Werror` warnings (fork builds with `-Wall -Werror`). Mitigation: may need to relax `-Werror` for the vendored lib. |
| A3 | The exact field names `extraction_file_stream_cb` / `romfs_filter` and `ExtractFiles` signature are as summarized. | Pattern 4 | Field/function names are from a doc-summary of the fork, not a line-by-line read. Planner/implementer must confirm against the vendored `settings.h` + `hactool.cpp` at port time. |
| A4 | No `override_config.ini` write is needed in Phase 1 (manual hold-R is sufficient). | Runtime State Inventory | If the target Atmosphère config blocks default title override, the user may need a config note. Mitigation: documentation-only fix in the takeover doc. |

**Note:** A1–A4 are flagged for the planner. None block planning; all are confirm-at-implementation items. The two *headline* unknowns (firmware key derivation A2-of-Pitfall-2, takeover permission sufficiency) are the spike's deliberate hardware validation targets, not assumptions to resolve before planning.

## Open Questions

1. **Do the pinned public key sources derive a valid header key on the target firmware?** (highest risk, hardware-only)
   - What we know: mechanism is proven in production on the firmwares exelix supported.
   - What's unclear: the *specific* firmware the spike runs on may need newer sources.
   - Recommendation: this IS the spike's job; pin + record both the Atmosphère source version and the run firmware (D-07). If it fails, update sources from a newer Atmosphère release.
2. **Single vs dual mbedtls on the link line?** (see A1)
   - Recommendation: start with two isolated libs (safe), evaluate consolidation as a follow-up.
3. **`lrLrResolveProgramPath` path-form stability across firmware.**
   - Recommendation: log the raw resolved path on the spike run; the rewrite is a one-line fix if it drifts.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| devkitA64 toolchain | Switch build | ✓ (CI: `devkitpro/devkita64` image) | image-pinned | — |
| libnx (`fs`/`spl`/`lr`/`pmdmnt`/`applet`) | privileged chain | ✓ | bundled in image | — |
| switch-mbedtls portlib | curl HTTPS (existing) | ✓ | 2.28.10 | — |
| `MBEDTLS_CMAC_C` build | hactool | ✗ (portlib omits it) | — | **No fallback — must build CMAC mbedtls from source (D-06)** |
| CMake ≥3.10 | build | ✓ | bundled | — |
| Real Switch + title takeover + Atmosphère | hardware validation (success criterion #1/#3/#4) | ✗ (dev machine) | — | **No fallback — hardware run is the spike's deliverable; cannot be validated in CI** |
| Desktop compiler (`-DUSE_SDL2=ON`) | desktop-green check (#2) | ✓ | system | — |

**Missing dependencies with no fallback:**
- `MBEDTLS_CMAC_C` — must be produced by building mbedtls from source (the locked D-06 plan).
- Hardware validation — Phase 1's success criteria #1, #3, #4 are only verifiable on a real console launched via title takeover. The build/compile criteria (#2) and the applet-gate code path are verifiable off-hardware; the actual extraction result is not.

**Missing dependencies with fallback:** none.

## Security Domain

> `security_enforcement` not set in config.json → treated as enabled. This phase handles cryptographic key derivation and copyrighted firmware data, so the security notes below are load-bearing.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth surface in this phase. |
| V3 Session Management | no | — |
| V4 Access Control | yes (OS-level) | Privileged FS/SPL access is granted by title takeover, not by thomaz; thomaz must fail-closed in applet mode (TAKEOVER-01). |
| V5 Input Validation | yes | Validate the resolved NCA path and the hactool output (non-empty, valid szs) before writing; check every libnx `Result`. |
| V6 Cryptography | yes | Use `MBEDTLS_CMAC_C` (never hand-roll CMAC); derive keys via SPL hardware slots (never bundle decrypted keys). |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Shipping decrypted Nintendo keys | Information Disclosure / Legal | Use SPL public *sources* only; keys derived on-device, never embedded. |
| Distributing extracted Nintendo `.szs` | Copyright | The extracted szs is copyrighted Nintendo data — written to the user's SD only, never uploaded/distributed. |
| Opaque failure in applet mode | Denial of Service (silent) | Fail-closed with a clear dialog before any privileged call. |
| Writing garbage on bad decrypt | Tampering / Integrity | Validate hactool output (size/magic) before writing to the canonical path; do not overwrite a good file with empty bytes. |
| Unchecked libnx `Result` | Tampering / availability | Check and surface every service `Result`. |

## Sources

### Primary (HIGH confidence)
- `.planning/research/EXTRACTION.md` — verified Option A mechanism, title-ID/szs tables, hardware open questions, exelix permalinks @ `2618b0c`.
- exelix11/SwitchThemeInjector `SwitchThemesNX/source/SwitchTools/key_loader.cpp` @ `2618b0c` — service init/exit order, key derivation, key-source sizes, NCA path rewrite. https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/SwitchThemesNX/source/SwitchTools/key_loader.cpp
- exelix11 `hactool.cpp` + `Libs/hactool/` (Makefile, README, 49-file list) @ `2618b0c` — in-memory extraction, filter/callback, action flags, fork build notes.
- exelix11 `SwitchThemesNX/Makefile` @ `2618b0c` — `LIBS`/`LIBDIRS`/`INCLUDES`, custom `libmbedtls.a` note ("portlibs one compiled without MBEDTLS_CMAC_C").
- devkitPro pacman-packages `switch/mbedtls/PKGBUILD` — mbedtls 2.28.10, config flags, **no `MBEDTLS_CMAC_C`** (3DS sets it, Switch doesn't). https://github.com/devkitPro/pacman-packages/blob/master/switch/mbedtls/PKGBUILD
- switchbrew libnx `fs.h` reference — `fsOpenBisFileSystem` signature, `FsBisPartitionId_System = 31`, `FsBisStorageId`→`FsBisPartitionId` rename (v3.0.0). https://switchbrew.github.io/libnx/fs_8h.html
- thomaz codebase (read directly): `CMakeLists.txt` (GLOB_RECURSE + link-order comment), `source/platform/themes/cfw_paths.{hpp,cpp}`, `theme_install.{hpp,cpp}`, `save_service_switch.cpp`/`save_service_fake.cpp` (split pattern), `source/main.cpp` (`#ifdef __SWITCH__` service wiring), `lib/switchthemes/Common.{cpp,hpp}` (target tables), `lib/switchthemes/README.md` (what Phase B did/didn't remove), `.github/workflows/build.yml` (CI image + deko3d), `THIRD_PARTY.md`.

### Secondary (MEDIUM confidence)
- GBAtemp threads on title override / applet vs Application mode — hold-R launch mechanics, full memory access via override. https://gbatemp.net/threads/what-in-the-nine-hells-is-title-override.605707/
- switchbrew NCA wiki + nstool SWITCH_KEYS — key-source provenance (FS .rodata; Atmosphère mirrors these). https://switchbrew.org/wiki/NCA

### Tertiary (LOW confidence)
- WebFetch doc-summaries of `hactool.cpp` field names (`extraction_file_stream_cb`, `romfs_filter`, `ExtractFiles` signature) — confirm against vendored source at port time (A3).

## Metadata

**Confidence breakdown:**
- Standard stack / vendoring: HIGH — verified against pinned exelix source + devkitPro PKGBUILD + in-tree files.
- Architecture / switch-fake split / CMake wiring: HIGH — split pattern and GLOB behavior read directly from thomaz; Makefile→CMake translation grounded in the actual Makefile and CMakeLists.
- Pitfalls: HIGH for build/applet/path pitfalls (verified); the firmware key-source pitfall is HIGH-confidence *as a risk* (it is the spike's explicit validation target).
- hactool fork API field names: MEDIUM — from doc summary, flagged A3.

**Research date:** 2026-06-04
**Valid until:** ~2026-07-04 (30 days; pinned commits/versions are stable, but libnx/devkitA64 image updates could shift the toolchain — re-verify the CMAC build if the CI image bumps).
