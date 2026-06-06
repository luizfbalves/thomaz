---
plan: 05-03
phase: 05
status: complete
requirements: [SIMPL-02, SIMPL-03]
completed: 2026-06-05
---

# 05-03 Summary — Collapse remaining seams (Option D rescope)

## Outcome

**Rescoped mid-execution** after a checkpoint, per user decision **Option D** (keep
platform-portability seams). The plan's original premise — "collapse 22 `#else` desktop
branches across the path-helper files" — was **wrong**: those ~12 `#else` branches are
platform-portability seams (Switch absolute path vs host-relative writable path, or a
host-compilable fallback impl), not `*_fake`-vs-`*_switch` stub-selection seams. The host
doctest suite **compiles** several of those files (`app_settings`, `cheat_store`,
`theme_paths`, `cfw_paths`, `title_visibility_store`, all of `core/**`), and 4 tests
**write** to the helper outputs via `fs::create_directories()`. Removing the host path
branches makes them write to absolute Switch paths (`/themes`, `/switch/thomaz/...`) and
fail with permission errors on the Linux host — directly violating success criterion #4
("host suite passes unchanged"). RESEARCH.md §4–§5 asserted the suite would pass unchanged;
that premise was falsified.

These portability seams are the same category as the `_WIN32`/`localtime_r` seams the phase
already agreed to keep (Category C). Under Option D they are **retained**.

## What was actually done

- **Kept** all ~12 path-string portability seams and the 3 host-fallback `#else` impls
  (`game_stats.cpp` fake-data fallback, `system_reboot.cpp` no-op, `theme_install.cpp`
  `(void)patches`) — they support the retained host build.
- **SIMPL-03 comment cleanup (the only required source change):** removed 3 stale comments
  that named deleted fakes, so the criterion-#3 grep is clean:
  - `source/platform/themes/firmware_extract.hpp` — dropped the `_switch.cpp / _fake.cpp pair`
    reference and the whole "Desktop behaviour (firmware_extract_fake.cpp)" comment block.
  - `source/platform/sysmod/sysmod_store.hpp` — dropped the "desktop fake (sysmod_store_fake)"
    comment.
- The genuine `*_fake`-vs-`*_switch` implementation-selection seams were already collapsed in
  Plan 05-01 (`main.cpp`, `home_activity.cpp`); the 9 stub files were deleted in 05-02.

## Deviations

- **Major rescope (Option D).** Original plan would have removed portability seams; that broke
  4 host tests. Reverted the executor's path-helper edits (preserved in
  `.05-03-executor-edits.patch` and `git stash@{0}`) and kept only the comment cleanups.
- The first executor attempt also over-reached (created unrelated files) and collided with a
  **concurrent session** (`260605-s2h`) editing the shared working tree. Its wrong-scope edits
  were set aside non-destructively; no concurrent work was touched.
- ROADMAP criterion #2 and REQUIREMENTS SIMPL-02 were reworded to record the Option-D scope.

## Verification

- `find source/platform -name '*fake*'` → only `saves/fake_cloud_save_client.{cpp,hpp}` ✓
- `grep -rnE 'PLATFORM_DESKTOP|SDL2|GLFW|_fake' source/` → only `fake_cloud_save_client` ✓
- `make -C tests test` → **208 cases / 618 assertions, 0 failed** ✓

## Artifacts this phase produces

No new symbols/files. Removals only: 3 stale desktop-fake comment references.
