# Vendored: Mbed TLS 2.28.10 (CMAC-enabled build)

- Upstream: https://github.com/Mbed-TLS/mbedtls
- Pinned tag: mbedtls-2.28.10
- License: Apache-2.0 OR GPL-2.0-or-later (dual) — see `LICENSE`.

This copy of Mbed TLS 2.28.10 is built from source as a DISTINCT static
CMake target (`thomaz_mbedtls_cmac`) with `MBEDTLS_CMAC_C` enabled. It is
NOT a replacement for the devkitPro portlib mbedtls that libcurl links
against — both coexist on the static link line (see CMakeLists.txt wiring
and Pitfall 1 in 01-RESEARCH.md).

## Why a second mbedtls?

The devkitPro `switch-mbedtls` portlib (version 2.28.10) is built **without**
`MBEDTLS_CMAC_C` (verified against the devkitPro pacman-packages Switch PKGBUILD).
The 3DS PKGBUILD does enable it; the Switch one does not. hactool needs
`mbedtls_cipher_cmac_*` symbols for NCA header verification. Patching the
portlib in place would risk breaking the curl HTTPS path and is not reproducible
in CI. The safe path is a second, isolated CMAC-enabled build at the same
version (2.28.10 — matching the portlib to avoid ABI symbol drift on the
static link line).

## CMAC Config Delta vs portlib

The build config is applied via `thomaz_cmac_config.h` (passed as
`MBEDTLS_USER_CONFIG_FILE`), which overrides only the listed symbols:

| Symbol | This build | portlib |
|--------|-----------|---------|
| `MBEDTLS_CMAC_C` | **ON** | off |
| `MBEDTLS_ENTROPY_HARDWARE_ALT` | **ON** | ON |
| `MBEDTLS_NO_PLATFORM_ENTROPY` | **ON** | ON |
| `MBEDTLS_SELF_TEST` | **off** | off |

All other settings follow the mbedtls 2.28.10 default `config.h`.

## Build

This library is built from source by CMake (`lib/mbedtls/CMakeLists.txt`)
inside the `if (PLATFORM_SWITCH)` block. No prebuilt `.a` blob is committed
(per D-06 — reproducibility). The CMake target is named `thomaz_mbedtls_cmac`
and is placed BEFORE the portlib `mbedtls`/`mbedx509`/`mbedcrypto` on
`APP_PLATFORM_LIB` so hactool's CMAC symbols resolve correctly (link-order
is load-bearing on the GNU static linker).

Build flags: `-DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF`, static lib only,
no shared lib, no self-test binaries.

## Assumption A1 follow-up note

A single CMAC-enabled mbedtls 2.28.10 could later replace the portlib
mbedtls for curl too (eliminating the dual-lib configuration). Phase 1
keeps two isolated builds as the safe path — the portlib may include
platform-specific entropy glue (e.g. `csrngGetRandomBytes`) not present
in this source tree. Consolidation to a single CMAC-enabled lib is deferred
as a follow-up for Phase 2 once hardware validation confirms the curl HTTPS
path remains intact.

## Files

- `include/` — Mbed TLS public headers (`mbedtls/*.h`, `psa/*.h`)
- `library/` — C source files (96 files, crypto + x509 + TLS)
- `3rdparty/` — bundled third-party code (everest, p256-m)
- `LICENSE` — Apache-2.0 OR GPL-2.0-or-later
- `thomaz_cmac_config.h` — CMAC-enabled user config (override header)
- `CMakeLists.txt` — thomaz CMake wiring producing `thomaz_mbedtls_cmac`
