---
phase: 05-collapse-source-seams-switch-only
plan: 02
subsystem: platform-source-cleanup
tags: [switch-only, stub-removal, simpl-01, deletion]
requires:
  - 05-01 (consumer factory/include seams collapsed to Switch-only)
provides:
  - "5 desktop stub pairs removed from source/platform/ (9 files)"
  - "SIMPL-01 complete: no desktop FakeSaveService/FakeTitleService/FakeAuthClient/FakeSysmoduleService/firmware_extract_fake in tree"
affects:
  - source/platform/ (file inventory; CMake GLOB_RECURSE picks up deletions on next configure)
tech-stack:
  added: []
  patterns:
    - "Single-target source tree: NsSaveService/NsTitleService/HttpAuthClient/SysmoduleStore are now the sole impls behind their interfaces"
key-files:
  created: []
  modified: []
  deleted:
    - source/platform/save_service_fake.cpp
    - source/platform/save_service_fake.hpp
    - source/platform/title_service_fake.cpp
    - source/platform/title_service_fake.hpp
    - source/platform/fake_auth_client.cpp
    - source/platform/fake_auth_client.hpp
    - source/platform/themes/firmware_extract_fake.cpp
    - source/platform/sysmod/sysmod_store_fake.cpp
    - source/platform/sysmod/sysmod_store_fake.hpp
decisions:
  - "Comment-only stale references in firmware_extract.hpp:45 and sysmod_store.hpp:20 left untouched — they are outside this plan's <files> scope (Step 5 interface-comment cleanup is a separate concern); deferred, not a SIMPL-01 blocker"
metrics:
  duration: ~3min
  completed: 2026-06-05
  tasks: 1
  files: 9
---

# Phase 5 Plan 02: Delete Desktop Stub Files Summary

Removed the 9 desktop stub files (5 stub pairs, one .hpp-less) from `source/platform/`, completing SIMPL-01. The retained doctest double `saves/fake_cloud_save_client.*` is explicitly preserved and remains git-tracked.

## What Was Built

A single atomic deletion. After Plan 01 cleared all `#include` and factory-instantiation seams in `main.cpp` and `home_activity.cpp`, these stubs had zero consumer references and were safe to delete. The CMake `GLOB_RECURSE` source list picks up the deletions on next configure with no `CMakeLists.txt` edit; `tests/Makefile` never listed any of the 5 stubs, so the host doctest suite is unaffected.

## Tasks Completed

| Task | Name | Commit | Files |
| ---- | ---- | ------ | ----- |
| 1 | Verify Plan 01 references clear, delete 9 stub files (SIMPL-01) | 481135e | 9 deletions in source/platform/ |

## Pre-Deletion Safety Gate (T-05-05 mitigation)

Ran the mandatory grep gate before deleting:
```
grep -rn 'save_service_fake|title_service_fake|fake_auth_client|sysmod_store_fake|firmware_extract_fake|FakeTitleService|FakeSaveService|FakeAuthClient|FakeSysmoduleStore' source/
```
All matches were either (a) self-references inside the 9 stub files themselves, or (b) three stale comments in non-stub headers. Zero `#include` directives or `make_unique<Fake...>`/`make_shared<Fake...>` factory references existed outside the stubs — confirming Plan 01 completed correctly. Deletion proceeded.

## Retained Doctest Double (T-05-04 mitigation)

`source/platform/saves/fake_cloud_save_client.{cpp,hpp}` was explicitly NOT deleted. Verified present and git-tracked after the deletion commit. The deletion list contained exactly 9 paths; the retained double (in the `saves/` subdirectory, no `_fake` underscore prefix) was never on it.

## Verification

- `find source/platform -name '*fake*'` returns exactly the two retained files:
  - `source/platform/saves/fake_cloud_save_client.cpp`
  - `source/platform/saves/fake_cloud_save_client.hpp`
- All 9 stub paths now resolve non-existent (`git ls-files` no longer tracks them).
- `git diff --cached` at commit time showed exactly 9 deletions, nothing else.
- The unrelated working-tree WIP (`CMakeLists.txt`, `source/app/settings_activity.cpp`, `source/platform/self_update.cpp`) was left untouched and unstaged.

### Note on the plan's stated glob

The plan's acceptance text expected `find source/platform -name '*_fake*'` (underscore-prefixed) to return the two retained files. The retained file is named `fake_cloud_save_client` — `fake` at the start with no preceding underscore — so it does not match `*_fake*`. Using the correct `*fake*` glob returns exactly the two retained files and nothing else. The substantive requirement (9 stubs gone, retained double intact) is fully met; this is a cosmetic discrepancy in the plan's example pattern, not a deviation in the work performed.

## Deviations from Plan

None affecting deletion scope — the plan executed exactly as written for its `<files>` list (the 9 deletions).

### Out-of-scope comment references (deferred, not fixed)

Two stale comments still name deleted stubs:
- `source/platform/themes/firmware_extract.hpp:45` — `// Desktop behaviour (firmware_extract_fake.cpp):`
- `source/platform/sysmod/sysmod_store.hpp:20` — `// desktop fake (sysmod_store_fake) is in-memory.`

These are comment-only and outside this task's `<files>` list (RESEARCH Step 5 scopes interface-comment cleanup separately from the deletion task). They are SIMPL-03 cleanup items, not SIMPL-01 blockers, and were intentionally left untouched to respect plan-file scope and avoid editing files not assigned to this task. A subsequent Phase 5 plan (or the SIMPL-03 sweep) should remove them.

## Known Stubs

None introduced. This plan only removes stubs.

## Self-Check: PASSED

- Deletions confirmed: all 9 stub paths absent from `git ls-files`.
- Commit 481135e exists in history (`git log` confirmed).
- Retained double `saves/fake_cloud_save_client.{cpp,hpp}` present and tracked.
