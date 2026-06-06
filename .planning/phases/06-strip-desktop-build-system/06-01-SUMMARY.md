---
phase: 06-strip-desktop-build-system
plan: 01
subsystem: infra
tags: [cmake, build-system, switch, devkitpro, desktop-removal]

# Dependency graph
requires:
  - phase: 05-collapse-source-seams-switch-only
    provides: Switch-only source tree (desktop stub files deleted, service/store seams collapsed) so removing the build's desktop path leaves a still-buildable tree
provides:
  - Switch-only CMakeLists.txt (no PLATFORM_DESKTOP branch or comment)
  - Removed desktop helper scripts (build-desktop.sh, run-desktop.sh)
affects: [06-02-build-verification, 07-docs-cleanup-final-gate]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single-platform CMake: PLATFORM_SWITCH is the sole platform guard; link + packaging are unconditional Switch paths"

key-files:
  created: []
  modified:
    - CMakeLists.txt
    - scripts/build-desktop.sh (deleted)
    - scripts/run-desktop.sh (deleted)

key-decisions:
  - "PLATFORM_SWITCH link block and .nro packaging target preserved byte-for-byte; only the desktop arms were excised — zero change to the shipped .nro"

patterns-established:
  - "Switch-only build: no dual-target branching; desktop find_package(CURL/LibArchive) and .data resource-copy paths fully removed"

requirements-completed: [BUILD-01, BUILD-02]

# Metrics
duration: 4min
completed: 2026-06-06
---

# Phase 6 Plan 1: Strip Desktop Build System Summary

**CMakeLists.txt is now Switch-only — both PLATFORM_DESKTOP branches (link + packaging) and the dual-target comments are gone, and the two desktop helper scripts are deleted, with the Switch link/.nro/hactool paths preserved exactly.**

## Performance

- **Duration:** 4 min
- **Started:** 2026-06-06T00:37:00Z
- **Completed:** 2026-06-06T00:41:45Z
- **Tasks:** 2
- **Files modified:** 3 (1 edited, 2 deleted)

## Accomplishments
- Removed the `elseif (PLATFORM_DESKTOP)` link branch (find_package CURL/LibArchive) — Switch link block (curl/mbedtls/archive + HACTOOL_ISOLATED) is now the sole link path
- Removed the `if (PLATFORM_DESKTOP)` packaging branch (`${PROJECT_NAME}.data` resource copy) and converted the trailing `elseif (PLATFORM_SWITCH)` to a standalone `if (PLATFORM_SWITCH)` — the `.nro` packaging target is the only packaging path
- Reworded the dual-target file-top comment ("...and desktop PC (GLFW) from one tree" → "Nintendo Switch (devkitPro) only") and the inline D-08 comment (dropped the "desktop must not see these targets" framing)
- Deleted `scripts/build-desktop.sh` and `scripts/run-desktop.sh` via `git rm`; `scripts/build-switch.sh` retained as the sole build helper

## Task Commits

Each task was committed atomically (staging only this plan's explicit files):

1. **Task 1: Remove PLATFORM_DESKTOP branches + dual-target comments from CMakeLists.txt** - `9aee066` (refactor)
2. **Task 2: Delete the desktop helper scripts** - `a028afe` (chore)

**Plan metadata:** see final docs commit

## Files Created/Modified
- `CMakeLists.txt` - Removed both PLATFORM_DESKTOP arms (link + packaging) and reworded the two dual-target comments; PLATFORM_SWITCH is the sole platform guard
- `scripts/build-desktop.sh` - Deleted (was the `-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` host build helper)
- `scripts/run-desktop.sh` - Deleted (was the host build+run helper)

## Decisions Made
- None - followed plan as specified. The PLATFORM_SWITCH link block, hactool/mbedtls isolation custom command, and `.nro` packaging command were preserved byte-for-byte.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
- Plan's verify snippet `grep -q 'thomaz.nro' CMakeLists.txt` does not match because the file uses `${PROJECT_NAME}.nro`, never the literal "thomaz.nro". The plan's OR-branch acceptance criterion (`grep -q 'add_custom_target(${PROJECT_NAME}.nro' ...`) passes (with `grep -F` to avoid shell `${}` expansion), confirming the `.nro` packaging target is intact at line 130. Not a defect in the edit — only the verify literal differs from the file's variable form.

## Working-Tree Note (concurrent session)
Per the dirty-tree directive, only this plan's explicit files were staged per-commit (always `git add <path>`, never `git add -A`). The two concurrent-session WIP files (`source/app/save_manager_activity.cpp`, `source/platform/self_update.cpp`) remain modified-but-uncommitted in the working tree and were never touched or staged. The accepted unrelated `VERSION_ALTER` hunk in CMakeLists.txt rode along with the Task 1 commit as authorized.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- BUILD-01 and BUILD-02 satisfied: CMakeLists.txt has no PLATFORM_DESKTOP token; both desktop scripts removed.
- Plan 06-02 (Wave 2) is the compile gate (BUILD-03): a clean Switch build via `scripts/build-switch.sh` (devkitPro Docker) must produce `build_switch/thomaz.nro`. Docker/devkitPro availability is the only gating dependency.

## Self-Check: PASSED

- FOUND: 06-01-SUMMARY.md
- FOUND: CMakeLists.txt (no PLATFORM_DESKTOP token)
- CONFIRMED-DELETED: scripts/build-desktop.sh
- CONFIRMED-DELETED: scripts/run-desktop.sh
- FOUND commit: 9aee066 (Task 1)
- FOUND commit: a028afe (Task 2)

---
*Phase: 06-strip-desktop-build-system*
*Completed: 2026-06-06*
