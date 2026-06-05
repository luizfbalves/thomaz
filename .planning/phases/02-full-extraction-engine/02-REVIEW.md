---
phase: 02-full-extraction-engine
reviewed: 2026-06-05T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - source/app/theme_detail_activity.cpp
  - source/platform/themes/cfw_paths.cpp
  - source/platform/themes/firmware_extract.hpp
  - source/platform/themes/firmware_extract_fake.cpp
  - source/platform/themes/firmware_extract_switch.cpp
  - source/platform/themes/nca_extract_switch.cpp
  - source/platform/themes/szs_validate.cpp
  - source/platform/themes/szs_validate.hpp
  - tests/test_cfw_paths.cpp
  - tests/test_szs_validate.cpp
findings:
  critical: 1
  warning: 6
  info: 4
  total: 11
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-06-05
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed the Phase 2 full firmware-extraction engine: the multi-title driver
(`extract_all_base_layouts`), the NCA RomFS extraction facade, the structural
SZS validator, the CFW path resolver, and the theme detail UI that triggers
extraction. The core control flow is generally sound and the systemic-vs-per-part
contract is implemented as documented, but there are several real defects:

- A **`/lyt/` directory-prefix filter that captures non-`.szs` files** and writes
  any that happen to unpack as SARC to disk under firmware-controlled filenames —
  the only Critical finding here, driven by the gap between the filter (every
  file under `/lyt/`) and the write logic (basename, no extension or
  path-segment check).
- A clutch of robustness/quality issues: `freopen(stderr)` is never restored,
  a fixed-size `peek[0x210]` diagnostic read with duplicated magic offsets,
  an unused dead validator (`is_valid_szs`) alongside the real one, duplicated
  `ensure_parent_dirs`/`write_file` helpers, and `ExtractResult`/
  `extract_base_layout` now being dead code superseded by the multi-title driver.

## Critical Issues

### CR-01: `/lyt/` prefix filter writes non-`.szs` files using firmware-controlled basenames

**File:** `source/platform/themes/firmware_extract_switch.cpp:244-300`,
`source/platform/themes/nca_extract_switch.cpp:54-69`

**Issue:** The multi-title driver passes `lyt_filter = {"/lyt/"}` (a directory
prefix), so `nca_romfs_filter` accepts **every** file under `/lyt/`, not just
`*.szs` layouts. The per-file loop then derives the output filename purely from
the RomFS key basename:

```cpp
const std::string base = romfs_key.substr(romfs_key.rfind('/') + 1);
const std::string out  = base_layout_dir() + "/" + base;
```

Two concrete problems:

1. **Non-szs files reach the write path.** Anything under `/lyt/` that is *not*
   a `.szs` (a stray asset, a future firmware addition, etc.) is fed to
   `is_structurally_valid_szs`. Most fail validation and pollute `failed_parts`
   with spurious "invalid szs" entries, but any file that *does* Yaz0/SARC-unpack —
   regardless of extension — is written into `/themes/systemData/` under its own
   basename and is later treated as a base layout by `base_present_for()` / the
   apply path. There is no `.szs` extension or expected-target whitelist check
   before writing.

2. **The basename is firmware-controlled and unvalidated.** The comment at
   lines 286-287 asserts "no `..` can survive rfind('/')+1", which holds for the
   *last* segment, but the code never confirms the captured key actually lives in
   `/lyt/` with a single trailing `.szs` segment, nor that `base` is one of the
   expected layout names. A directory entry `/lyt/` yields `out ==
   base_layout_dir() + "/"` (write to a directory path). The safety argument
   relies entirely on the firmware image being well-formed; this engine is
   explicitly the boundary decrypting third-party NCA content, so "trust the
   firmware image" is not an acceptable invariant.

**Fix:** Constrain both the filter and the write to the known target set rather
than a blind directory prefix. Either keep the per-target exact-name filter
(iterate the `target_map` szs names) or, if the broad capture is intentional,
gate the write on extension + a non-empty basename:

```cpp
const std::string base = romfs_key.substr(romfs_key.rfind('/') + 1);
if (base.empty() ||
    base.size() < 4 ||
    base.compare(base.size() - 4, 4, ".szs") != 0) {
    failed_parts.push_back(romfs_key + ": not a .szs layout, skipped");
    continue;
}
const std::string out = base_layout_dir() + "/" + base;
```

The `is_structurally_valid_szs` SARC check is integrity, not authorization — it
does not prevent a non-layout SARC file from being written under an unexpected
name.

## Warnings

### WR-01: `freopen(stderr, ...)` permanently redirects process stderr to an SD log

**File:** `source/platform/themes/nca_extract_switch.cpp:190-191, 258-275`

**Issue:** `std::freopen(kErrLog, "w", stderr)` rebinds the process-wide `stderr`
to `/switch/thomaz/hactool.log` and it is **never restored**. After the first
extraction, every subsequent `fprintf(stderr)`/assert/library diagnostic in the
entire app goes to that file (and each `extract_szs_from_nca` call reopens it in
`"w"` mode, truncating prior content). This is a global side effect of a
supposedly self-contained facade and will silently swallow unrelated diagnostics
for the rest of the session.

**Fix:** Save and restore the original stream, e.g. capture via `dup`/`dup2` on
the underlying fd, or `freopen` back to the console device after reading the log:

```cpp
fflush(stderr);
int saved = dup(fileno(stderr));
freopen(kErrLog, "w", stderr);
// ... run + read back ...
fflush(stderr);
dup2(saved, fileno(stderr));
close(saved);
```

### WR-02: Fixed `peek[0x210]` diagnostic read with duplicated magic offsets

**File:** `source/platform/themes/nca_extract_switch.cpp:216-229`

**Issue:** `peek` is a 0x210-byte stack buffer; the loop at line 227 reads
`peek[0x200 + i]` for `i < 16`. It is in-bounds today only because `sizeof(peek)`
happens to be exactly `0x200 + 0x10`, but the `0x200` offset and the `0x210` size
are unrelated literals with no named constant linking them, so any future shrink
of `peek` becomes a stack overread. This is debug instrumentation added to chase
the Phase 1 key bugs (commits `068499f`, `c749d46`, `301c2e5`) that are now
fixed; it should not remain on the shipping extraction path.

**Fix:** Gate the entire diagnostic block behind a debug flag and derive both the
buffer size and the peek offset from one named constant, or remove it.

### WR-03: Empty/garbage-name RomFS keys produce a write to a directory path

**File:** `source/platform/themes/firmware_extract_switch.cpp:287-298`

**Issue:** If `romfs_key` ends in `/` (a directory entry) or `rfind('/')` returns
`npos`, `base` is empty or the whole key, yielding `out == base_layout_dir() +
"/"`. `write_file` then attempts to open a directory path for writing, which
fails into `failed_parts` with a confusing `"/themes/systemData/: write failed"`
message. `nca_on_file_dumped` guards `length == 0` but not directory-style names.
This overlaps CR-01 but is separate because even with a `.szs` extension guard,
an empty/odd basename should be rejected with a clear message before the write.

**Fix:** Reject `romfs_key.rfind('/') == std::string::npos` and empty `base`
explicitly (covered by the CR-01 fix snippet).

### WR-04: Dead/duplicate validator — `is_valid_szs` (magic-only) shadows the real structural check

**File:** `source/platform/themes/firmware_extract_switch.cpp:23-30`

**Issue:** `is_valid_szs` (magic-byte-only) is defined and used by the legacy
single-target `extract_base_layout` (line 148), while the Phase 2 driver uses the
stronger `is_structurally_valid_szs`. Two validators of different strength on the
same family of code paths invite a future caller picking the weaker one by
mistake. The magic-only check accepts any 4-byte `SARC`/`Yaz0` prefix followed by
garbage — exactly the case `is_structurally_valid_szs` was added to reject (see
`test_szs_validate.cpp:47-52`).

**Fix:** If `extract_base_layout` is retained (see WR-05), route it through
`is_structurally_valid_szs` and delete `is_valid_szs`. If it is removed, this
helper goes with it.

### WR-05: `extract_base_layout` is dead code superseded by the multi-title driver

**File:** `source/platform/themes/firmware_extract_switch.cpp:59-166`,
`source/platform/themes/firmware_extract.hpp:53`

**Issue:** The only UI trigger (`doExtract` in `theme_detail_activity.cpp:429`)
now calls `extract_all_base_layouts()`. There is no remaining production caller of
`extract_base_layout`. It is ~100 lines of privileged, key-handling code that is
no longer exercised, uses the weaker validator (WR-04), and duplicates the
session/derive/resolve/write chain — and it has already drifted (its single-target
filter `{"/lyt/" + tm->szs}` uses the exact-name path while the driver uses the
prefix path). Dead privileged code is a security maintenance hazard, not merely a
style issue.

**Fix:** Remove `extract_base_layout` and its declaration, or document a concrete
retained caller. No test currently exercises it.

### WR-06: `freopen` failure is unchecked — extraction may write to a closed stream

**File:** `source/platform/themes/nca_extract_switch.cpp:191`

**Issue:** The `std::freopen` return value is ignored. Per the C standard, on
failure `freopen` closes the original stream and returns NULL. If
`/switch/thomaz` cannot be created or the open fails, every subsequent
`fprintf(stderr, ...)` in this function (and beyond — see WR-01) writes to a
closed stream.

**Fix:** Check the return value; on NULL, skip the stderr-capture diagnostics and
proceed without redirection rather than writing to a closed stream.

## Info

### IN-01: Duplicated `ensure_parent_dirs` / `write_file` helpers across modules

**File:** `source/platform/themes/firmware_extract_switch.cpp:34-49`

**Issue:** Both helpers are acknowledged copies of `theme_install.cpp` (comments
at lines 33, 42). Three copies of the same `mkdir -p` + binary-trunc write logic
will drift. `ensure_parent_dirs` also ignores all `mkdir` errors, not just the
benign `EEXIST`.

**Fix:** Hoist into a shared `fs_util` TU reused from both `theme_install` and
`firmware_extract_switch`.

### IN-02: `printf` diagnostics on the production extraction path

**File:** `source/platform/themes/firmware_extract_switch.cpp:94-98, 123,
162-163, 216-219, 262-263, 294-295`; `source/app/theme_detail_activity.cpp:432-441`

**Issue:** Numerous `std::printf` calls (firmware version, NCA paths, byte counts,
per-part results) on the extraction path. `theme_detail_activity.cpp` documents
these as a "temporary Phase 2 hardware gate" (lines 414-419). Tracked so they are
not forgotten when Phase 3 lands.

**Fix:** Gate behind a debug build flag or remove when the Phase 3 UI replaces the
verification trigger.

### IN-03: `base_present_for` conflates unknown-target with missing-file

**File:** `source/platform/themes/cfw_paths.cpp:47-56`

**Issue:** `base_present_for` returns `false` for both "unknown target" and "file
missing" with no way to distinguish a typo'd target from a genuinely absent base
file. If a target name ever drifts from the `target_map` keys, the user silently
sees "base missing" forever with no diagnostic.

**Fix:** Optionally log when `base_szs_path(t).empty()` (line 51) to surface
target-name drift during development.

### IN-04: Header documents a desktop fallback for `extract_szs_from_nca` that does not exist

**File:** `source/platform/themes/nca_extract_switch.hpp` (doc),
`source/platform/themes/nca_extract_switch.cpp:7,299`

**Issue:** The header comment states the desktop build "returns an empty map +
'NCA extraction is only available on Nintendo Switch.'" but the `.cpp` is wrapped
entirely in `#ifdef __SWITCH__` with **no** `#else` branch and there is no
`nca_extract_fake.cpp`. On a non-Switch build the symbol is simply undefined
(link error), not the documented stub. This is benign only because the sole
caller is itself `__SWITCH__`-guarded, but the header's contract is false and
would break any future host-side caller or unit test of this facade.

**Fix:** Either add the documented `#else` stub / `_fake.cpp`, or correct the
header comment to state the symbol is Switch-only and not linkable on desktop.

---

_Reviewed: 2026-06-05_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
