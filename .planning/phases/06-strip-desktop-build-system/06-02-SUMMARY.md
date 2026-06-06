---
phase: 06-strip-desktop-build-system
plan: 02
subsystem: infra
tags: [devkitpro, switch, cmake, build, nxlink, deko3d]

# Dependency graph
requires:
  - phase: 06-strip-desktop-build-system (Plan 01)
    provides: PLATFORM_DESKTOP-free CMakeLists.txt + desktop helper scripts removed
provides:
  - Verified clean Switch build of the desktop-stripped tree (BUILD-03 gate green)
  - build_switch/thomaz.nro (7.7 MB) produced from the Switch-only tree
  - On-hardware confirmation: launched on a real Switch via nxlink
affects: [07-docs-cleanup-final-verification-gate, milestone-v1.1-completion]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Native devkitPro build fallback when Docker is down — run via devkitPro MSYS2 login shell so cmake inherits DEVKITPRO"
key-files:
  created:
    - .planning/phases/06-strip-desktop-build-system/06-02-SUMMARY.md
  modified: []

key-decisions:
  - "Docker daemon was down (HTTP 500); satisfied BUILD-03 via a native devkitPro build instead of deferring to a manual gate"
  - "Native build must run through devkitPro's MSYS2 login shell — the git-bash cmake does not inherit DEVKITPRO env, so the borealis toolchain check fails otherwise"
  - "Installed missing Switch portlibs (switch-curl, switch-libarchive, switch-glm, switch-mbedtls + transitive zlib/zstd/lz4/bzip2/lzma) via devkitPro pacman; deko3d already present"

patterns-established:
  - "Pattern: when Docker is unavailable, build natively via 'MSYSTEM=MSYS /c/devkitPro/msys2/usr/bin/bash.exe -lc \"cd <repo> && rm -rf build_switch && bash scripts/build-switch.sh\"'"

requirements-completed: [BUILD-03]

# Metrics
duration: ~10min
completed: 2026-06-05
---

# Phase 6 — Plan 02: Switch Build Verification Summary

**The desktop-stripped tree builds clean for Switch and runs on real hardware — `build_switch/thomaz.nro` (7.7 MB) was produced from a from-scratch build and launched on a physical Switch via nxlink.**

## Performance

- **Duration:** ~10 min (portlib install + clean from-scratch compile)
- **Completed:** 2026-06-05
- **Tasks:** 1 (verification-only)
- **Files modified:** 0 source/build files (verification plan; `build_switch/` is a build output, not committed)

## Accomplishments
- **BUILD-03 gate GREEN** — clean from-scratch Switch build (`rm -rf build_switch` then `scripts/build-switch.sh`) ran end-to-end with exit 0 against the `PLATFORM_DESKTOP`-free `CMakeLists.txt`.
- **Artifact verified** — `build_switch/thomaz.nro` exists and is non-empty (7,709,957 bytes).
- **On-hardware validation (beyond plan scope)** — the `.nro` was pushed to a real Switch over Wi-Fi via `nxlink -a 192.168.5.128` (netloader) and launched successfully.
- **Pre-gate confirmed** — `grep PLATFORM_DESKTOP CMakeLists.txt` returns nothing; `scripts/build-desktop.sh` and `scripts/run-desktop.sh` are gone; the `PLATFORM_SWITCH` path is intact (6 references). Wave 1 removal is fully landed.

## Deviation from plan: Docker → native build
The plan assumed the Docker path (`devkitpro/devkita64`). In this environment the **Docker daemon was down** (HTTP 500 from `dockerDesktopLinuxEngine`). Rather than take the plan's "deferred manual gate" fallback, BUILD-03 was satisfied with a **real native devkitPro build** on the operator's Windows machine (`C:\devkitPro`).

Two setup gaps had to be closed first:
1. **Missing Switch portlibs** — `portlibs/switch/` had only `bin/`. Installed `switch-curl`, `switch-libarchive`, `switch-glm`, `switch-mbedtls` (pulling zlib/zstd/lz4/bzip2/lzma/libexpat) via `/c/devkitPro/msys2/usr/bin/pacman.exe`. `deko3d` was already present (in `libnx/lib`).
2. **DEVKITPRO not reaching cmake** — devkitPro's `cmake.exe` (msys-2.0.dll runtime) does not inherit env vars set from the git-bash shell, so `export DEVKITPRO=...` failed and borealis `toolchain.cmake` aborted with "Please set DEVKITPRO". Fix: run the build through devkitPro's own MSYS2 **login shell**, where `/etc/profile.d/devkit-env.sh` sets `DEVKITPRO=/opt/devkitpro` and fstab mounts `/opt/devkitpro → C:\devkitPro`.

Working invocation (recorded for future builds):
```bash
MSYSTEM=MSYS /c/devkitPro/msys2/usr/bin/bash.exe -lc \
  'cd /c/www/thomaz && rm -rf build_switch && bash scripts/build-switch.sh'
```

This is a stronger outcome than the plan required: not just a deferred manual gate, but a verified-green build plus a live on-console launch.

## Task Commits

This is a verification-only plan with no source changes. Only the plan metadata / SUMMARY is committed:

1. **Task 1: Clean Switch build + thomaz.nro confirmed** — no source commit (verification only); build output is gitignored.

**Plan metadata:** _(this SUMMARY commit)_

## Files Created/Modified
- `.planning/phases/06-strip-desktop-build-system/06-02-SUMMARY.md` — this summary (only tracked change)
- `build_switch/thomaz.nro` — build output (7.7 MB, not committed; gitignored)

## Verification
- Pre-gate: no `PLATFORM_DESKTOP` in CMakeLists.txt; both desktop scripts absent. ✓
- `scripts/build-switch.sh` (native, from clean `build_switch/`) exited 0, reached `[100%] Built target thomaz.nro`. ✓
- `test -s build_switch/thomaz.nro` → 7,709,957 bytes. ✓
- nxlink transfer to Switch `192.168.5.128` returned exit 0; app launched on hardware. ✓

## Notes for Phase 7 / milestone close
- BUILD-03 is satisfied; the v1.1 post-removal compile gate is green.
- The STATE.md blocker ("if Docker unavailable, BUILD-03 becomes a manual/deferred gate") can be cleared — it was resolved via the native build path.
- Docs in Phase 7 should reflect that the Switch build can run **either** via Docker **or** natively via the devkitPro MSYS2 login shell.
