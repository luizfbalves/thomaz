---
phase: 03-c-platform-hardening
plan: "01"
subsystem: platform/fs_util
tags: [C++, refactor, debt, DEBT-01, DEBT-02, D-05, fs_util]
dependency_graph:
  requires: []
  provides:
    - thomaz::ensure_parent_dirs (source/platform/fs_util.hpp)
    - thomaz::copy_tree (source/platform/fs_util.hpp)
  affects:
    - source/platform/cheat_store.cpp
    - source/platform/themes/theme_install.cpp
    - source/platform/mods/libarchive_extractor.cpp
    - source/platform/mods/mod_download.cpp
    - source/platform/mods/mod_store.cpp
    - source/platform/mods/mod_store.hpp
    - source/platform/mods/mod_actions.cpp
    - source/platform/save_service_switch.cpp
tech_stack:
  added: []
  patterns:
    - shared platform utility in thomaz:: namespace (POSIX + std::string, C++17-compatible)
    - D-05 equivalence gate doctest with char-by-char oracle
key_files:
  created:
    - source/platform/fs_util.hpp
    - source/platform/fs_util.cpp
    - tests/test_fs_util.cpp
  modified:
    - tests/Makefile
    - source/platform/cheat_store.cpp
    - source/platform/themes/theme_install.cpp
    - source/platform/mods/libarchive_extractor.cpp
    - source/platform/mods/mod_download.cpp
    - source/platform/mods/mod_store.cpp
    - source/platform/mods/mod_store.hpp
    - source/platform/mods/mod_actions.cpp
    - source/platform/save_service_switch.cpp
    - tests/test_mod_store.cpp
decisions:
  - Canonical ensure_parent_dirs is the substring-at-slash form from cheat_store.cpp (not the char-by-char variant from theme_install.cpp)
  - Ghost-file-removal-on-open-fail from save_service_switch.cpp folded into copy_file in fs_util.cpp (no behavior lost)
  - save_service_switch 2-arg copy_tree callers migrated to 3-arg with nullptr for err (callers ignore error detail; copy_tree's top-level mkdir handles EEXIST)
  - test_http_feed_client.cpp removed (orphan from Phase 1 community removal, blocking make)
  - test_mod_store.cpp updated to include fs_util.hpp (copy_tree moved out of mod_store.hpp)
metrics:
  duration: "~18 minutes"
  completed: "2026-06-05T15:20:59Z"
  tasks_completed: 2
  tasks_total: 3
  files_created: 3
  files_modified: 10
---

# Phase 3 Plan 01: fs_util Consolidation Summary

**One-liner:** Extracted ensure_parent_dirs and copy_tree from 7 duplicated call-sites into a single thomaz::fs_util POSIX utility with D-05 equivalence gate doctest; 175 tests pass green.

## What Was Built

Created `source/platform/fs_util.{hpp,cpp}` — a shared platform utility under the `thomaz::` namespace that consolidates:

- `thomaz::ensure_parent_dirs(const std::string& path)` — canonical substring-at-slash form from cheat_store.cpp; handles trailing slashes correctly for libarchive_extractor's usage pattern
- `thomaz::copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err)` — 3-arg recursive copy from mod_store.cpp; ghost-file-removal-on-open-fail folded in from save_service_switch.cpp

All 7 previously-duplicated call-sites migrated:
1. `cheat_store.cpp` — ensure_parent_dirs (anon-namespace deleted)
2. `theme_install.cpp` — ensure_parent_dirs char-by-char variant (deleted after D-05 equivalence proved)
3. `libarchive_extractor.cpp` — ensure_parent_dirs (anon-namespace deleted)
4. `mod_download.cpp` — ensure_parent_dirs (anon-namespace deleted)
5. `mod_store.cpp` — is_dir/copy_file/copy_tree all moved to fs_util.cpp
6. `mod_store.hpp` — copy_tree declaration removed (now in fs_util.hpp)
7. `mod_actions.cpp` — include added (call already 3-arg)
8. `save_service_switch.cpp` — make_dirs + 2-arg copy_tree removed; callers use 3-arg with nullptr

D-05 equivalence gate: `tests/test_fs_util.cpp` proves via inline char-by-char oracle that the canonical (substring-at-slash) form produces identical directory creation results for interior paths and trailing-slash paths BEFORE the char-by-char copy was removed.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| TDD RED | D-05 test + Makefile fix | 03000fe | tests/test_fs_util.cpp, tests/Makefile, tests/test_http_feed_client.cpp (deleted) |
| Task 1 GREEN | fs_util.{hpp,cpp} implementation | 1e8179e | source/platform/fs_util.hpp, source/platform/fs_util.cpp, tests/Makefile |
| Task 2 | Migrate 7 call-sites | 33ed988 | 9 source files modified |

## Pending

**Task 3 (checkpoint:human-verify):** Desktop CMake build verification — human must run fresh CMake configure with `-DUSE_SDL2=ON` to confirm fs_util.cpp is picked up by GLOB_RECURSE and compiled with zero new warnings.

## Verification Results

- `make test`: **175 passed, 0 failed** (includes 3 new test_fs_util.cpp tests)
- `grep -rn 'void ensure_parent_dirs|bool copy_tree' source/ | grep -v fs_util`: **empty** (exactly one definition each)
- `grep -l fs_util.hpp` across all 7 migrated files: **all 7 found**
- `grep -c copy_tree source/platform/mods/mod_store.hpp`: **0**
- No C++20-only constructs (std::format, std::span) in fs_util.cpp/.hpp

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed orphaned test_http_feed_client.cpp blocking make**
- **Found during:** Task 1 TDD RED phase
- **Issue:** `tests/test_http_feed_client.cpp` included `platform/feed/http_feed_client.hpp` which was deleted in Phase 1 (commit c0354b0). The Makefile also had an explicit SRCS entry for `../source/platform/feed/http_feed_client.cpp`. Both blocked `make` entirely, preventing the test build.
- **Fix:** Deleted `tests/test_http_feed_client.cpp` and removed the stale SRCS entry from `tests/Makefile`. The underlying source was legitimately removed as part of community-feature removal (Phase 1, D-08/D-09).
- **Files modified:** tests/test_http_feed_client.cpp (deleted), tests/Makefile
- **Commit:** 03000fe

**2. [Rule 1 - Bug] Added fs_util.hpp include to tests/test_mod_store.cpp**
- **Found during:** Task 2 migration (linker error)
- **Issue:** `test_mod_store.cpp` used `copy_tree` (under `using namespace thomaz`) which it previously got transitively from `mod_store.hpp`. After removing `copy_tree` from `mod_store.hpp` per Task 2, the test lost its declaration.
- **Fix:** Added `#include "platform/fs_util.hpp"` to `tests/test_mod_store.cpp`.
- **Files modified:** tests/test_mod_store.cpp
- **Commit:** 33ed988

## Known Stubs

None. All helpers are fully wired; tests exercise real filesystem operations with temp dirs.

## Threat Flags

No new network endpoints, auth paths, or file access patterns introduced beyond what the plan's threat model covers. The threat register disposition for T-03-01 (path traversal) remains `accept` — paths arrive pre-validated by `core::is_safe_archive_path`. T-03-02 (behavioral drift) mitigated via D-05 equivalence gate doctest (test_fs_util.cpp). No new threat surface.

## Self-Check: PASSED

- source/platform/fs_util.hpp: FOUND
- source/platform/fs_util.cpp: FOUND
- tests/test_fs_util.cpp: FOUND
- .planning/phases/03-c-platform-hardening/03-01-SUMMARY.md: FOUND
- commit 03000fe (TDD RED): FOUND
- commit 1e8179e (feat GREEN): FOUND
- commit 33ed988 (migrate): FOUND
