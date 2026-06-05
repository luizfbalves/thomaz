---
phase: 02-full-extraction-engine
fixed_at: 2026-06-05T00:00:00Z
review_path: .planning/phases/02-full-extraction-engine/02-REVIEW.md
iteration: 1
findings_in_scope: 7
fixed: 7
skipped: 0
status: all_fixed
---

# Phase 02: Code Review Fix Report

**Fixed at:** 2026-06-05
**Source review:** .planning/phases/02-full-extraction-engine/02-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 7 (1 Critical + 6 Warning; 4 Info findings out of scope under `critical_warning`)
- Fixed: 7
- Skipped: 0

> Build note: this repo's extraction code is Switch-only (`#ifdef __SWITCH__`,
> `#include <switch.h>`/`<hactool.h>`), so no host compiler/`tsc`-equivalent is
> available. Verification used Tier 1 (re-read) plus a brace/preprocessor
> balance check. Findings whose fix changes control flow are flagged
> "requires human verification" — confirm on a devkitPro/hardware build before
> the phase proceeds to the verifier gate.

## Fixed Issues

### CR-01: `/lyt/` prefix filter writes non-`.szs` files using firmware-controlled basenames

**Files modified:** `source/platform/themes/firmware_extract_switch.cpp`
**Commit:** 1449288
**Status:** fixed: requires human verification (adds a control-flow guard)
**Applied fix:** Before deriving the output path in the per-file write loop of
`extract_all_base_layouts`, the basename is now extracted defensively
(`rfind('/') == npos` yields an empty base) and the write is gated on a
non-empty basename ending in `.szs`. Non-layout files under `/lyt/` (including
directory-style keys) are pushed to `failed_parts` with a clear
"not a .szs layout, skipped" message instead of being written to
`base_layout_dir()` under a firmware-controlled name. This closes the gap
between the broad directory-prefix filter and the basename-only write path.

### WR-01: `freopen(stderr, ...)` permanently redirects process stderr to an SD log

**Files modified:** `source/platform/themes/nca_extract_switch.cpp`
**Commit:** 6ab47d2
**Status:** fixed: requires human verification (fd save/restore behaviour)
**Applied fix:** Added `<unistd.h>`; `dup(fileno(stderr))` captures the original
stderr fd before `freopen`, and after the hactool log read-back the fd is
restored via `dup2(saved, fileno(stderr))` + `close(saved)`. The redirect is
now local to `extract_szs_from_nca`, so later diagnostics in the rest of the
session are no longer swallowed by `hactool.log`.

### WR-02: Fixed `peek[0x210]` diagnostic read with duplicated magic offsets

**Files modified:** `source/platform/themes/nca_extract_switch.cpp`
**Commit:** 0b46781
**Applied fix:** The raw-byte peek dump is now wrapped in
`#if THOMAZ_NCA_EXTRACT_DEBUG` (default `0`, i.e. off on the shipping path).
The buffer size derives from named constants `kPeekSize = kPeekOffset (0x200)
+ kPeekWindow (0x10)`, and both peek loops bound on `kPeekWindow`/`kPeekOffset`,
so the loop can no longer overrun the buffer if either constant changes.

### WR-03: Empty/garbage-name RomFS keys produce a write to a directory path

**Files modified:** `source/platform/themes/firmware_extract_switch.cpp`
**Commit:** 1449288
**Status:** fixed: requires human verification (shares the CR-01 control-flow guard)
**Applied fix:** Resolved by the same guard as CR-01. `rfind('/') == npos` now
produces an empty base, and the `base.empty()` branch rejects directory-style
or garbage keys with the "not a .szs layout, skipped" message before any
`write_file` is attempted — eliminating the confusing
`/themes/systemData/: write failed` outcome. (No separate diff; the REVIEW
explicitly noted WR-03 is covered by the CR-01 fix snippet.)

### WR-04: Dead/duplicate validator — `is_valid_szs` (magic-only) shadows the real structural check

**Files modified:** `source/platform/themes/firmware_extract_switch.cpp`
**Commit:** 7c50ff2
**Applied fix:** Deleted the magic-only `is_valid_szs` helper (its only caller,
`extract_base_layout`, was removed in WR-05) and corrected the stale comment in
the driver that referenced it. The driver retains the stronger
`is_structurally_valid_szs`, so no caller can pick the weaker validator.

### WR-05: `extract_base_layout` is dead code superseded by the multi-title driver

**Files modified:** `source/platform/themes/firmware_extract_switch.cpp`,
`source/platform/themes/firmware_extract.hpp`,
`source/platform/themes/firmware_extract_fake.cpp`
**Commit:** 0544b1c
**Applied fix:** Removed the ~100-line single-target `extract_base_layout`
implementation (Switch), its declaration and doc block, the now-unused
`ExtractResult` struct in the header, and the desktop fake definition. The only
UI trigger (`doExtract`) already calls `extract_all_base_layouts()`; grep
confirmed no remaining production caller. (`ExtractResult` in
`mods/archive_extractor.hpp` is a different, unrelated struct and was left
untouched.)

### WR-06: `freopen` failure is unchecked — extraction may write to a closed stream

**Files modified:** `source/platform/themes/nca_extract_switch.cpp`
**Commit:** 8d31e35
**Status:** fixed: requires human verification (control-flow gating)
**Applied fix:** The `freopen` return value is now captured as
`stderr_redirected`. The build-stamp line, the byte/key diagnostics, and the
log read-back are all gated on `stderr_redirected`, so on a failed redirect
(NULL return, original stream closed) the code proceeds without redirection
instead of writing to a closed stream.

## Skipped Issues

None — all in-scope findings were fixed.

> Out-of-scope (Info tier, not attempted under `fix_scope: critical_warning`):
> IN-01 (duplicated `ensure_parent_dirs`/`write_file` helpers),
> IN-02 (`printf` diagnostics on the production path),
> IN-03 (`base_present_for` conflates unknown-target with missing-file),
> IN-04 (header documents a non-existent desktop fallback for
> `extract_szs_from_nca`). Re-run with `fix_scope: all` to address these.

---

_Fixed: 2026-06-05_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
