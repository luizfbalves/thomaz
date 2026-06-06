---
type: quick
slug: fix-save-backup
status: complete
created: 2026-06-05
completed: 2026-06-05
requirements: []
---

# Quick Task Summary: Fix save backup failing on Switch

**`copy_tree()` now creates its full destination directory chain, so
`NsSaveService::backup()` can write saves to `/switch/thomaz/saves/...` — backup
confirmed working on real hardware.**

## Root cause

`NsSaveService::backup()` copies a mounted save into
`/switch/thomaz/saves/<title_id>/<timestamp>/<uid>` via `copy_tree()`.
`copy_tree()` created only the **leaf** dir with one **non-recursive** `::mkdir()`
and ignored the return. The parent chain never existed (backup had never
succeeded), so the leaf `mkdir` failed with `ENOENT`, `copy_file()` then could
not open the destination, and `copy_tree` returned `cannot copy <src>` →
on-screen `Falha no backup: copy failed`.

The import/restore-to-disk path was unaffected because it writes via
`write_text_file() → ensure_parent_dirs()` (recursive). The NS-service backup
path relied on `copy_tree`, which wasn't recursive — that asymmetry was the bug.

Not a Phase 05/06 regression: `save_service_switch.cpp` was untouched since
Phase 03/04. Latent never-worked path (development happened against desktop
test doubles, which use a different write path).

## Fix

`source/platform/fs_util.cpp` — in `copy_tree()`, replaced the single
non-recursive `::mkdir(dst_dir)` with `ensure_parent_dirs(dst_dir + "/")`, which
creates `dst_dir` and every missing ancestor. Fixes all `copy_tree` callers
(save backup + mod copy).

## Diagnosis method

1. Read backup() → identified 3 failure branches ("no save data" / "copy failed"
   / "could not write manifest"), each surfaced on screen.
2. Built a `-DDEBUG` instrumented `.nro` (activates borealis `nxlinkStdio`) and
   surfaced `copy_tree`'s detailed error on-screen; nxlink stdout was blocked
   (firewall), but the on-screen detail showed `cannot copy ...` → the
   `copy_file` dst-open branch → missing destination directory.
3. Reproduced as a host doctest (RED) — same `cannot copy ...` error class.
4. Applied fix → host doctest GREEN (209/209) → rebuilt Release `.nro` → deployed
   via nxlink → user confirmed Backup works on hardware.

## Verification

- Host doctest `tests/test_fs_util.cpp`: new case "copy_tree creates destination
  chain when parent dirs are missing" — RED before, GREEN after. Suite 209/209.
- On-hardware: Backup on a real game succeeded (was failing before).

## Files
- `source/platform/fs_util.cpp` — recursive destination creation in `copy_tree`
- `tests/test_fs_util.cpp` — regression test

## Notes
- Diagnostic instrumentation in `save_service_switch.cpp` was reverted — the
  committed change set is the fix + test only.
- Host test build on this machine needs msys2 `gcc` and
  `-DDOCTEST_CONFIG_NO_POSIX_SIGNALS` (doctest's POSIX signal handler doesn't
  compile under the msys2 runtime). See project memory.
