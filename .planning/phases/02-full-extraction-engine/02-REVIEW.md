---
phase: 02-full-extraction-engine
reviewed: 2026-06-05T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - source/platform/themes/cfw_paths.cpp
  - source/platform/themes/firmware_extract.hpp
  - source/platform/themes/firmware_extract_fake.cpp
  - source/platform/themes/firmware_extract_switch.cpp
  - source/platform/themes/nca_extract_switch.cpp
  - source/platform/themes/szs_validate.cpp
  - source/platform/themes/szs_validate.hpp
  - source/app/theme_detail_activity.cpp
  - tests/test_cfw_paths.cpp
  - tests/test_szs_validate.cpp
findings:
  critical: 1
  warning: 6
  info: 4
  total: 11
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-06-05
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed the Phase 2 full extraction engine diff (base `f71ae0e^`): the multi-title
firmware theme extraction driver (`extract_all_base_layouts`), the NCA RomFS filter
extension (directory-prefix matching), the host-testable SARC/Yaz0 structural
validator, and the wiring in `theme_detail_activity.cpp::doExtract`.

The new code is generally careful — applet gate is correctly the first statement,
the privileged session is closed on every exit path, buffers are validated before
write, and the structural validator never throws to the caller. The test coverage
for `cfw_paths` and `szs_validate` is solid (good/bad/empty/truncated cases).

However there is one BLOCKER: the permanent, non-restored `freopen` of `stderr` in
`nca_extract_switch.cpp` hijacks the process's entire stderr stream for the app
lifetime and silently truncates the diagnostic log on every call, so prior-run
errors are destroyed and any other component's stderr output is misdirected. Several
WARNINGs cover a divergent/now-dead validation path, a stale-buffer overwrite
hazard in the dump callback, an unverified seek assumption, and a fragile
firmware-controlled basename derivation. A consistency gap between the two output-path
computations is also flagged.

## Critical Issues

### CR-01: `freopen(stderr)` is never restored and truncates the log on every call

**File:** `source/platform/themes/nca_extract_switch.cpp:190-191` (and re-read 262-275)
**Issue:**
```cpp
std::fflush(stderr);
std::freopen(kErrLog, "w", stderr);
```
`stderr` is permanently re-pointed at `/switch/thomaz/hactool.log` and is **never
restored** to its original target before the function returns. Consequences:

1. **Process-wide side effect.** After the first `extract_szs_from_nca` call, *every*
   subsequent `fprintf(stderr, ...)` anywhere in the application (libnx, borealis,
   other modules) writes into this file, not the console/nxlink. The function leaves
   the global stream in a mutated state.
2. **Log destroyed on every run.** The `"w"` mode truncates. On the second extraction
   attempt the diagnostics from the first attempt — including the failure the log
   exists to capture — are wiped before they can be read. For `extract_all_base_layouts`
   this is one freopen per *session*, but a user retrying after a failure loses the
   prior evidence.
3. The read-back at line 262-275 reads only the last 512 bytes and never reopens the
   real stderr, so the redirection persists past the function.

This is a correctness/diagnostics-integrity defect (the log is the primary
hardware-verification artifact for this phase) and a global-state corruption.

**Fix:** Save and restore the original stderr instead of leaking the redirect. For
example, dup the fd around the call:
```cpp
std::fflush(stderr);
int saved_stderr = dup(fileno(stderr));
std::freopen(kErrLog, "a", stderr);   // append, do not truncate prior runs
// ... run hactool, fflush ...
// after reading back the log:
std::fflush(stderr);
dup2(saved_stderr, fileno(stderr));
close(saved_stderr);
clearerr(stderr);
```
At minimum switch `"w"` → `"a"` so retries do not destroy the previous failure log,
and restore stderr before returning so the redirect does not leak into the rest of
the app.

## Warnings

### WR-01: `extract_base_layout` uses the weaker magic-only validator and is now dead

**File:** `source/platform/themes/firmware_extract_switch.cpp:23-30, 148`
**Issue:** `extract_base_layout` validates with the anonymous-namespace `is_valid_szs`
(4-byte SARC/Yaz0 magic only), while the new `extract_all_base_layouts` uses the
stronger `is_structurally_valid_szs` (full Yaz0-decompress + SARC-unpack, D-04). After
this diff `doExtract` calls only `extract_all_base_layouts`, so `extract_base_layout`
(and its magic-only check) is no longer reached from the UI. This leaves two
divergent validation contracts in the tree: a 4-byte magic that a truncated/garbage
buffer passes trivially, versus the real structural check. A future caller wiring
itself to `extract_base_layout` would silently get the weaker integrity guarantee that
T-01-15 / D-04 intended to eliminate.
**Fix:** Either route `extract_base_layout` through `is_structurally_valid_szs` (delete
the local `is_valid_szs`), or, if `extract_base_layout` is obsolete, remove it and its
fake/header counterparts so only one validated path remains.

### WR-02: dump callback silently overwrites on duplicate filenames

**File:** `source/platform/themes/nca_extract_switch.cpp:75-84`
**Issue:** `nca_on_file_dumped` does `out[name] = std::vector<...>(...)`. With the new
directory-prefix filter (`"/lyt/"`) the callback now fires for *many* files per pass.
If hactool ever emits the same RomFS path twice (e.g. a streamed/chunked dump, or a
re-walk), the second invocation overwrites the first with no detection. For a chunked
stream this would mean keeping only the final chunk and discarding earlier data — the
buffer would then fail SARC validation, but the failure mode ("invalid szs") would
misattribute the cause. The single-target exact-match path was immune to this because
only one file matched.
**Fix:** Detect collisions and either append or flag:
```cpp
auto [it, inserted] = out.try_emplace(name);
if (!inserted) {
    // unexpected re-dump of the same path — append rather than discard
    it->second.insert(it->second.end(), file_data, file_data + length);
} else {
    it->second.assign(file_data, file_data + length);
}
```
If the fork is known never to chunk, document that invariant at the call site.

### WR-03: in-memory peek relies on an unverified "hactool re-seeks to 0" assumption

**File:** `source/platform/themes/nca_extract_switch.cpp:216-221`
**Issue:** The diagnostic block seeks to `SEEK_END`, reads the size, then `SEEK_SET`,
and the comment asserts "hactool re-seeks to 0 anyway." `nca_process` is then invoked
on the same `FILE*`. If that assumption is wrong for this fork, the NCA parse starts
from a non-zero offset and reads garbage — the exact "Invalid NCA header / data abort"
class of failure this code is trying to diagnose. The diagnostic instrumentation could
itself induce the failure it reports.
**Fix:** Do not rely on the assumption — restore the position explicitly after the
peek (the code already does `fseek(nca_file, 0, SEEK_SET)` at line 221, so the
remaining risk is only that hactool expects a *different* start offset). Verify against
the fork's `nca_process` entry and convert the comment into a guaranteed
`std::rewind(nca_file)` immediately before `nca_process`, or read the peek via `pread`
on a separate fd so the shared stream position is never perturbed.

### WR-04: basename derivation trusts firmware-controlled RomFS keys

**File:** `source/platform/themes/firmware_extract_switch.cpp:287-288`
**Issue:**
```cpp
const std::string base = romfs_key.substr(romfs_key.rfind('/') + 1);
const std::string out  = base_layout_dir() + "/" + base;
```
The comment (line 285-287) argues `rfind('/')+1` neutralizes any `..`. That holds only
for keys of the exact shape `/lyt/NAME.szs`. The `romfs_key` originates from the NCA
RomFS (decrypted firmware data) via hactool's filename callback, and the prefix filter
`"/lyt/"` admits anything under that directory, including subpaths such as
`/lyt/sub/dir/x.szs` (basename `x.szs`) or, if a key ever lacks a `/`, `rfind` returns
`npos` and `npos + 1 == 0`, so `substr(0)` yields the whole key. While `..` cannot
survive `rfind('/')+1`, an empty or dotted basename (e.g. key ending in `/` →
empty `base` → write target `base_layout_dir() + "/"`, a directory) is not guarded.
The write then targets a directory path and fails, but the failure is misreported and
the assumption is undocumented for the multi-file case the new filter enables.
**Fix:** Validate the derived basename before use: reject empty, reject names
containing no `.szs` suffix, and reject `.`/`..`:
```cpp
auto slash = romfs_key.rfind('/');
std::string base = (slash == std::string::npos) ? romfs_key
                                                : romfs_key.substr(slash + 1);
if (base.empty() || base == "." || base == ".." ||
    base.size() < 4 || base.compare(base.size()-4, 4, ".szs") != 0) {
    failed_parts.push_back(romfs_key + ": rejected output name");
    continue;
}
```

### WR-05: output-path computation diverges between the two extractors

**File:** `source/platform/themes/firmware_extract_switch.cpp:80, 288`
**Issue:** `extract_base_layout` writes to `base_szs_path(target)` (which routes through
`target_map` and emits a *known* canonical filename), whereas `extract_all_base_layouts`
writes to `base_layout_dir() + "/" + basename(romfs_key)` (the raw firmware filename).
These two can disagree: `target_map` curates exactly the eight known szs names, but the
prefix filter will write *every* `/lyt/*.szs` the firmware exposes — including layout
files that `base_present_for`/`target_map` do not recognize. The result is files in
`systemData/` that the apply path never consumes, and a contract split where "what was
extracted" ≠ "what target_map knows about." This is a maintainability/consistency
defect that will surface when a future firmware adds or renames a `/lyt/` entry.
**Fix:** Decide a single source of truth. Either filter written files against the set
of `target_map` szs names, or document explicitly that `extract_all` is intentionally
a superset and that unrecognized szs are harmless. Prefer routing both extractors
through one helper that maps a RomFS key → canonical output path.

### WR-06: hactool abort leaks NCA section contexts (state left partially built)

**File:** `source/platform/themes/nca_extract_switch.cpp:247-252`
**Issue:** On the `setjmp` recovery branch, `nca_free_section_contexts(&nca_ctx)` is
deliberately skipped (comment: "sections leaked"). While pure memory leakage is out of
scope, the concern here is that `nca_ctx` (and the global hactool keyset still holding
the header key until line 256) is left in a partially-initialized state after a
`longjmp` that bypassed normal cleanup. Because `g_hactool_recover_active` is reset and
the function returns an error, the immediate call is safe, but any future reuse of
hactool global state in the same process (the keyset, static buffers in the fork) after
an abort is unaudited. The header key wipe at line 256 still runs, which is good.
**Fix:** If `nca_free_section_contexts` is safe to call on a partially-built context,
call it on the abort path too; if it is not, document precisely which global hactool
state survives an abort and confirm a subsequent extraction re-initializes all of it.

## Info

### IN-01: triplicated helper bodies across translation units

**File:** `source/platform/themes/firmware_extract_switch.cpp:34-49`
**Issue:** `ensure_parent_dirs` and `write_file` are hand-copied from
`theme_install.cpp` (the comments even cite the source line numbers), and
`is_valid_szs` duplicates the magic check in `szs_validate`/`nca_extract`. Three copies
of the same logic drift independently over time.
**Fix:** Extract `ensure_parent_dirs`/`write_file` into a shared neutral utility header
(e.g. `platform/fs_util.hpp`) and have both call sites include it.

### IN-02: debug printf instrumentation shipped in the extraction path

**File:** `source/platform/themes/firmware_extract_switch.cpp:94, 123, 162, 216, 262-300` and `source/app/theme_detail_activity.cpp:432-441`
**Issue:** Numerous `std::printf` diagnostics and the entire stderr-peek block are
production code paths, not gated behind a debug flag. The header comment in
`theme_detail_activity.cpp:414` acknowledges the `doExtract` logging is a "temporary
Phase 2 verification trigger." These should be removed or compiled out before this code
ships beyond hardware verification.
**Fix:** Wrap diagnostics in a `#ifdef THOMAZ_EXTRACT_DEBUG` (or equivalent) and track
the temporary trigger removal in the Phase 3 backlog as the comment promises.

### IN-03: magic byte sequences and `0x20`/`0x10` key sizes are unnamed constants

**File:** `source/platform/themes/firmware_extract_switch.cpp:26-28`, `source/platform/themes/nca_extract_switch.cpp:117, 154-172, 256`
**Issue:** The SARC/Yaz0 magic bytes, the `0x20` header-key length, and the `0x10`
kaek source length appear as inline literals in multiple places. `0x20` in particular
is checked in three files. Magic numbers obscure intent and risk an inconsistent update.
**Fix:** Introduce named constants (`constexpr std::size_t kHeaderKeyLen = 0x20;`,
`kKaekSourceLen = 0x10;`) and reuse them at every check/`memcpy`.

### IN-04: `make_good_sarc` returns `packed.data` but ignores `PackedSarc` alignment hint

**File:** `tests/test_szs_validate.cpp:12-19`
**Issue:** The helper discards everything in `SARC::PackedSarc` except `.data`. This is
fine for the validator test, but if `PackedSarc` also reports an alignment/offset the
validator should honor, the test would not catch a regression there. Purely a coverage
note — the existing good/bad/empty/truncated cases are otherwise comprehensive.
**Fix:** None required; optionally add a case asserting a SARC with multiple files and
non-default alignment still validates.

---

_Reviewed: 2026-06-05_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
