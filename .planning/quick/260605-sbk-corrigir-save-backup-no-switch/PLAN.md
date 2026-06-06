---
type: quick
slug: fix-save-backup
created: 2026-06-05
requirements: []
---

# Quick Task: Fix save backup failing on Switch

## Problem

User reported save **backup** always fails on the Switch with an on-screen error.
Investigation (systematic-debugging) confirmed the on-screen message is
`Falha no backup: ... cannot copy ...` — i.e. `NsSaveService::backup()` returns
`outError = "copy failed"`.

## Root cause (confirmed)

`NsSaveService::backup()` (`source/platform/save_service_switch.cpp`) copies a
mounted save into `/switch/thomaz/saves/<title_id>/<timestamp>/<uid>` via
`copy_tree()`. But `copy_tree()` (`source/platform/fs_util.cpp`) created only the
**leaf** directory with a single **non-recursive** `::mkdir()` and ignored its
return value. The parent chain never existed (backup had never succeeded), so the
leaf `mkdir` failed with `ENOENT`, then `copy_file()` could not open the
destination and `copy_tree` returned `cannot copy <src>` → `"copy failed"`.

Asymmetry that hid it: the import/restore-to-disk path uses
`write_text_file() → ensure_parent_dirs()` (recursive), so it created its dirs;
the NS-service backup path relied on `copy_tree`, which did not.

Not a Phase 05/06 regression — `save_service_switch.cpp` was untouched since
Phase 03/04; this was a latent never-worked path (dev happened on desktop doubles).

## Fix

In `copy_tree()`, replace the single non-recursive `::mkdir(dst_dir)` with
`ensure_parent_dirs(dst_dir + "/")` so the full destination chain (and `dst_dir`
itself) is created. Benefits every `copy_tree` caller (save backup + mod copy).

## Verification

- RED→GREEN host doctest: new `test_fs_util.cpp` case "copy_tree creates
  destination chain when parent dirs are missing" fails before the fix
  (`err=cannot copy .../game.sav`, same class as the device error) and passes
  after. Full suite: 209/209.
- On-hardware: rebuild `.nro`, deploy via nxlink, user runs Backup → succeeds.

## Files
- `source/platform/fs_util.cpp` — the fix (recursive dst creation)
- `tests/test_fs_util.cpp` — regression test
