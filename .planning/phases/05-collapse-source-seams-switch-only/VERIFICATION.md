---
phase: 05
name: collapse-source-seams-switch-only
status: passed
verified: 2026-06-05
method: direct command evidence (orchestrator-verified; gsd-verifier skipped due to an active concurrent session on the shared working tree)
requirements: [SIMPL-01, SIMPL-02, SIMPL-03]
scope_decision: Option D (platform-portability seams retained)
---

# Phase 5 Verification — Collapse Source Seams to Switch-Only

**Goal:** the `source/` tree reasons as a single Switch target for implementation selection —
every desktop *stub* is gone and every `*_fake`-vs-`*_switch` selection seam uses the Switch
path, with the host doctest suite still green.

## Success criteria (all met)

| # | Criterion | Evidence | Verdict |
|---|-----------|----------|---------|
| 1 | 5 desktop stub pairs deleted | `find source/platform -name '*fake*'` → only `saves/fake_cloud_save_client.{cpp,hpp}` (glob corrected from `*_fake*`) | ✅ PASS |
| 2 | No `*_fake`-vs-`*_switch` implementation-selection seam | Selection seams collapsed in 05-01 (`main.cpp`, `home_activity.cpp`); 9 stubs deleted in 05-02. Residual `#else` are platform-portability seams, **retained per Option D** | ✅ PASS |
| 3 | No desktop-stub / desktop-only references | `grep -rnE 'PLATFORM_DESKTOP\|SDL2\|GLFW\|_fake' source/` → only `fake_cloud_save_client` | ✅ PASS |
| 4 | Host doctest suite passes unchanged | `make -C tests test` → **208 cases / 618 assertions / 0 failed** | ✅ PASS |

## Scope decision (Option D)

Research (and the initial plan) treated ~12 path-helper `#ifdef __SWITCH__ … #else … #endif`
blocks as removable "desktop branches." They are in fact platform-**portability** seams
(Switch absolute path vs host-writable relative path, or a host-compilable fallback impl) —
the same category as the retained `_WIN32`/`localtime_r` seams. The host doctest suite compiles
several of those files and writes to their outputs; removing the host branch broke 4 tests
(writes to absolute `/themes` etc.). The user chose **Option D: retain platform-portability
seams.** ROADMAP criterion #2 and REQUIREMENTS SIMPL-02 were reworded to record this.

## Notes / follow-ups

- Verified manually because a concurrent session (`260605-s2h`, navbar system-status widget)
  was actively committing to `main` and editing the shared working tree. No concurrent work
  was touched; all phase-5 commits stage explicit paths only.
- The first 05-03 executor attempt over-scoped (removed portability seams) and was reverted
  non-destructively. Its edits remain recoverable in `git stash@{0}` and
  `.05-03-executor-edits.patch` (both safe to drop now that 03 is complete correctly).

**Phase 5: PASSED.**
