---
phase: 01-privileged-extraction-spike
plan: 01
subsystem: build/vendoring
tags: [hactool, mbedtls, cmac, cmake, vendoring, gplv2, attribution]
dependency_graph:
  requires: []
  provides: [hactool-static-target, thomaz_mbedtls_cmac-static-target, cmac-build-config]
  affects: [CMakeLists.txt, THIRD_PARTY.md]
tech_stack:
  added:
    - "hactool fork (exelix11) @ commit 2618b0c — C static lib, in-memory NCA/RomFS extraction"
    - "Mbed TLS 2.28.10 CMAC build — C static lib, MBEDTLS_CMAC_C enabled"
  patterns:
    - "add_subdirectory inside if(PLATFORM_SWITCH) guard — D-08 desktop isolation"
    - "MBEDTLS_USER_CONFIG_FILE override header — CMAC delta config without touching portlib"
    - "CMake target prefix — thomaz_mbedtls_cmac distinct from portlib mbedtls"
    - "APP_PLATFORM_LIB link order — CMAC target before portlib (Pitfall 1)"
key_files:
  created:
    - lib/hactool/README.md
    - lib/hactool/CMakeLists.txt
    - lib/mbedtls/README.md
    - lib/mbedtls/CMakeLists.txt
    - lib/mbedtls/thomaz_cmac_config.h
  modified:
    - CMakeLists.txt
    - THIRD_PARTY.md
decisions:
  - "Distinct CMake target `thomaz_mbedtls_cmac` (not replacing portlib) — safe dual-mbedtls path per Pitfall 1"
  - "MBEDTLS_USER_CONFIG_FILE override header approach — minimal delta from default config.h"
  - "-Wno-error on both vendored targets per Assumption A2 (toolchain-drift guard)"
  - "SPL key sources THIRD_PARTY.md placeholder deferred to plan 01-05 (needs hardware run)"
metrics:
  duration_seconds: 248
  completed_date: "2026-06-04"
  tasks_completed: 3
  tasks_total: 3
  files_created: 5
  files_modified: 2
---

# Phase 01 Plan 01: Vendor hactool + CMAC mbedtls and Wire CMake Summary

**One-liner:** Vendored exelix hactool fork (2618b0c) and Mbed TLS 2.28.10 from source with CMAC enabled into Switch-only CMake static libs with correct dual-mbedtls link order.

## What Was Built

### Task 1: hactool fork vendored

The 49-file exelix11 hactool fork was committed under `lib/hactool/` at the pinned commit `2618b0c31e007d019757dc4095eca08b4a89e3f5`. This is the exelix fork with in-memory buffer + filter callbacks (`extraction_file_stream_cb`, `romfs_filter`) — NOT the SciresM upstream (disk-write variant). A provenance README was written recording the upstream URL, pinned commit, imported path (`SwitchThemesNX/Libs/hactool`), GPLv2 license, and Adaptations section documenting the A2 Werror relaxation. A `CMakeLists.txt` produces the `hactool` static target with `-Wno-error` guard.

### Task 2: CMAC-enabled Mbed TLS 2.28.10

The full Mbed TLS 2.28.10 source (include/, library/, 3rdparty/) was committed under `lib/mbedtls/`. A CMAC config header (`thomaz_cmac_config.h`) was written as `MBEDTLS_USER_CONFIG_FILE`, enabling `MBEDTLS_CMAC_C` and matching the devkitPro PKGBUILD flags (`MBEDTLS_ENTROPY_HARDWARE_ALT` on, `MBEDTLS_NO_PLATFORM_ENTROPY` on, `MBEDTLS_SELF_TEST` off). The `CMakeLists.txt` produces the distinct static target `thomaz_mbedtls_cmac`. No prebuilt `.a` blob was committed (D-06). The README records the upstream URL, pinned tag `mbedtls-2.28.10`, CMAC config delta, Apache-2.0/GPLv2 dual license, and the Assumption A1 single-lib consolidation follow-up note.

### Task 3: CMake wiring + THIRD_PARTY.md

Inside the existing `if (PLATFORM_SWITCH)` block in `CMakeLists.txt`:
- `add_subdirectory(lib/hactool)` and `add_subdirectory(lib/mbedtls)` added
- `lib/hactool/include` appended to `APP_PLATFORM_INCLUDE`
- `APP_PLATFORM_LIB` updated: `hactool thomaz_mbedtls_cmac` placed BEFORE `curl mbedtls mbedx509 mbedcrypto` (link order — Pitfall 1)

The `PLATFORM_DESKTOP` block is untouched (D-08). `THIRD_PARTY.md` extended with `## hactool (NCA/RomFS extraction)` (pinned commit 2618b0c, GPLv2), `## mbedtls (CMAC build)` (pinned tag mbedtls-2.28.10, Apache-2.0/GPLv2), and `## SPL key sources` placeholder pointing to plan 01-05.

## Deviations from Plan

### Auto-decisions (no structural deviations)

**1. Desktop configure verify — environment limitation**

The desktop configure (`cmake -B /tmp/thomaz_desktop_cfg -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON`) fails with a pre-existing error: `include could not find commonOption.cmake` because the borealis submodule is not checked out in this execution environment. This failure exists independently of the new code. D-08 compliance is verified by code inspection: all new targets (`add_subdirectory(lib/hactool)`, `add_subdirectory(lib/mbedtls)`, `APP_PLATFORM_INCLUDE/LIB` additions) are inside the `if (PLATFORM_SWITCH)` block on lines 55-66; the `elseif (PLATFORM_DESKTOP)` branch starting at line 69 is unchanged. The CI image (devkitpro/devkita64) builds with `-DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON` and will be the true Switch-build verification.

**2. `lib/mbedtls/CMakeLists.txt` explicit file list (not using upstream's CMakeLists)**

Rather than using `add_subdirectory(lib/mbedtls/library)` with the upstream cmake (which sets `MBEDTLS_TARGET_PREFIX` etc.), a standalone `CMakeLists.txt` at the top of `lib/mbedtls/` explicitly lists sources and produces only the `thomaz_mbedtls_cmac` static target. This avoids the upstream's `USE_SHARED_MBEDTLS_LIBRARY`, `ENABLE_TESTING`, `ENABLE_PROGRAMS` options colliding with the root project's CMake variables. This is a safe, more isolated approach that matches D-06 (no blob, build from source).

**3. SPL key sources THIRD_PARTY.md block — placeholder**

The plan says to add a `## SPL key sources` block but this requires the Atmosphère version + firmware version from the hardware run (D-07). A placeholder block was added pointing to plan 01-05, which records provenance after the hardware validation spike. This matches the plan's note: "Leave the SPL key-source + firmware-version provenance block for plan 05."

## Known Stubs

None — this plan is purely vendoring + build wiring. No data flows through any stub. The `## SPL key sources` THIRD_PARTY.md placeholder is intentional and tracked for plan 01-05.

## Threat Flags

No new threat surface beyond what the plan's threat model covers. The vendored C source at pinned origins satisfies T-01-01 (provenance integrity). The link order satisfies T-01-02 (dual-mbedtls correctness). The THIRD_PARTY.md attribution satisfies T-01-03 (GPLv2 attribution). No network endpoints, auth paths, or file access patterns were introduced.

## Self-Check: PASSED

Files created:
- lib/hactool/README.md: FOUND
- lib/hactool/CMakeLists.txt: FOUND
- lib/mbedtls/README.md: FOUND
- lib/mbedtls/CMakeLists.txt: FOUND
- lib/mbedtls/thomaz_cmac_config.h: FOUND

Commits:
- d3cb981: chore(01-01): vendor exelix hactool fork at pinned commit 2618b0c — FOUND
- 48917ab: chore(01-01): vendor Mbed TLS 2.28.10 source + CMAC build config — FOUND
- 5a9205e: chore(01-01): wire hactool + thomaz_mbedtls_cmac into CMake (Switch-only) and update THIRD_PARTY.md — FOUND
