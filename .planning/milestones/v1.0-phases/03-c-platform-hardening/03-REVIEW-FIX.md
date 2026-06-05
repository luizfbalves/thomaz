---
phase: 03-c-platform-hardening
fixed_at: 2026-06-05T00:00:00Z
review_path: .planning/phases/03-c-platform-hardening/03-REVIEW.md
iteration: 1
findings_in_scope: 12
fixed: 12
skipped: 0
status: all_fixed
---

# Phase 3: Code Review Fix Report

**Fixed at:** 2026-06-05
**Source review:** .planning/phases/03-c-platform-hardening/03-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 12 (1 Critical, 6 Warning, 5 Info — fix_scope: all)
- Fixed: 12
- Skipped: 0
- Host doctest suite: 179 cases / 537 assertions, all passing (was 177/533 at baseline; +2 cases from the TEST-03 rewrite).

## Deliberate, user-authorized design change (D-06 reversal)

CR-01 reverses the phase's locked **D-06 fail-open** decision to **fail-closed**, on explicit
user authorization. This is intentional, not an accidental behavior change:

- `tls_policy(false)` now defaults to `{1,2}` (verification ON) instead of `{0,0}`.
- A new `TlsMode` enum gates the insecure `{0,0}` policy behind an explicit, opt-in
  `TlsMode::InsecureAllowed` argument. No content-bearing download requests it.
- **TEST-03 (`tests/test_tls_policy.cpp`) was updated** to match the new contract. The fail-safe
  regression intent is *preserved*: the test now asserts that the DEFAULT for a missing CA bundle
  is verification-ON (`tls_policy(false) == {1,2}`), so a future change that silently disables
  verification on the default path still fails CI. Added cases assert the explicit opt-in
  (`tls_policy(false, TlsMode::InsecureAllowed) == {0,0}`) and the explicit Verify mode.
- **SEC-03 banner coherence:** `source/app/tls_banner.cpp` and its `tls_is_insecure()` helper are
  left intact and wired. Under fail-closed, `apply_curl_tls` no longer sets the insecure latch on a
  missing CA (the insecure path is opt-in), so the banner only renders if a future narrowly-scoped
  channel deliberately opts in and sets the latch. The banner compiles and is logically consistent
  with the new default.

## Fixed Issues

### CR-01: TLS verification silently disabled (fail-open) when CA bundle is missing

**Files modified:** `source/platform/tls_policy.hpp`, `source/platform/curl_tls.hpp`, `tests/test_tls_policy.cpp`
**Commit:** 3829744
**Applied fix:** Added `enum class TlsMode { Verify, InsecureAllowed }`; `tls_policy(bool, TlsMode=Verify)`
now returns `{1,2}` for a missing CA unless `InsecureAllowed` is passed. `apply_curl_tls`'s CA-absent
branch keeps verification ON and no longer flips the insecure latch. TEST-03 rewritten to assert the
fail-closed default while preserving the silent-downgrade regression guard.
**Note:** This reverses locked decision D-06, user-authorized.

### WR-01: TLS warning banner can silently fail to attach

**Files modified:** `source/app/tls_banner.cpp`
**Commit:** d96ec66
**Applied fix:** Fall back from `brls/applet_frame/hint_box` to `brls/applet_frame/header`; if neither
slot exists, emit `brls::Logger::warning(...)` instead of dropping the warning silently.

### WR-02: download_file writes can silently truncate on disk-full / early close

**Files modified:** `source/platform/mods/mod_download.cpp`
**Commit:** 9b93aaa
**Applied fix:** Documented (per the review's "at minimum" recommendation) that `download_file` does
not by itself guarantee a byte-complete file and that archive integrity depends on the downstream
libarchive EOF check. No behavioral change — the review confirmed the extractor already rejects
truncated archives.

### WR-03: copy_tree reports success on partial copies in nullptr-err callers (save restore)

**Files modified:** `source/platform/save_service_switch.cpp`
**Commit:** 579c71e
**Applied fix:** Added an explicit INVARIANT comment at the `restore()` wipe-then-copy site documenting
that wiping the live save before copying the backup is only safe because `fsdevCommitDevice` is gated
on `copy_tree` succeeding (uncommitted mounts are discarded). Guards a future edit from committing
unconditionally.

### WR-04: read_marker / read_text_file perform unbounded reads

**Files modified:** `source/platform/mods/mod_store.cpp`, `source/platform/cheat_store.cpp`
**Commit:** b8d95dd (combined with WR-05 — same functions, inseparable edits)
**Applied fix:** Capped `read_marker` at 4096 bytes and `read_text_file` at 1 MiB to bound allocation
from an SD-card file a malformed download could enlarge.

### WR-05: read_marker / read_text_file ignore ferror

**Files modified:** `source/platform/mods/mod_store.cpp`, `source/platform/cheat_store.cpp`
**Commit:** b8d95dd (combined with WR-04)
**Applied fix:** After the read loop, check `std::ferror(f)` and return `std::nullopt` on a mid-read
I/O error, mirroring the `save_service_switch.cpp` `read_tree` precedent. Prevents a flaky-SD
truncation from looking like valid (truncated) content.

### WR-06: copy_tree follows symlinks via stat

**Files modified:** `source/platform/fs_util.cpp`, `source/platform/fs_util.hpp`
**Commit:** 5a1d6a5
**Applied fix:** `is_dir` now uses `::lstat` (no symlink follow); added `is_symlink`; `copy_tree` skips
any symlink entry (escape/loop guard). Documented the symlink-skip contract in `fs_util.hpp`.

### IN-01: xferInfo casts signed curl_off_t to uint64_t

**Files modified:** `source/platform/mods/mod_download.cpp`
**Commit:** 0c6d0df
**Applied fix:** Clamp `dltotal`/`dlnow` to 0 when not positive before the cast, so an unknown size
(`-1`) does not become `0xFFFF...`. UI treats 0 as indeterminate.

### IN-02: extract_archive lists the archive twice

**Files modified:** `source/platform/mods/libarchive_extractor.cpp`
**Commit:** 0de9e25
**Applied fix:** Added a maintainability comment explaining the double-open is an accepted v1 cost and
noting the optimization options (indeterminate total / cached entries).

### IN-03: uid_hex / sscanf pointer-cast aliasing

**Files modified:** `source/platform/save_service_switch.cpp`
**Commit:** 957d4d4
**Applied fix:** Switched formatting to `%016" PRIx64` with `std::uint64_t` and added a `uid_from_hex`
helper that parses into local `std::uint64_t` values via `SCNx64` (no `(unsigned long*)` cast).
Replaced all 4 parse sites and the format site.
**Status: requires human verification** — this file is inside `#ifdef __SWITCH__` and is NOT compiled
by the host doctest suite (`g++ -std=c++17` skips the Switch branch). The change is a
behavior-preserving refactor, but it must be confirmed against a real devkitPro/Switch build before
relying on it. The host suite remains green (no shared-header regression).

### IN-04: tls_insecure_flag non-atomic cross-thread

**Files modified:** `source/platform/curl_tls.hpp`
**Commit:** fbc38b7
**Applied fix:** Changed the latch to `std::atomic<bool>` (`#include <atomic>`); `tls_is_insecure()`
uses `.load()`. Consistent with the phase's cloudBusy atomic migration. Header is not host-compiled
(pulls curl.h); verified by re-read.

### IN-05: D-05 oracle vs canonical diverge on leading-slash edge

**Files modified:** `tests/test_fs_util.cpp`
**Commit:** 3fa978f
**Applied fix:** Annotated the equivalence TEST_CASE to make clear it proves
equivalence-over-tested-inputs (repo-shaped absolute paths), not universal equivalence, and described
the `"//x"` divergence so a future reader does not over-trust the claim.

## Skipped Issues

None — all 12 in-scope findings were fixed.

## Verification notes

- Host doctest suite (`make -C tests clean && make -C tests test`) passes: 179 cases, 537 assertions.
- CR-01 increased the suite from 177 to 179 cases (TEST-03 rewrite adds the explicit-mode and
  opt-in cases).
- IN-03 is flagged "requires human verification" because the modified code lives in a Switch-only
  block not exercised by the host build.

---

_Fixed: 2026-06-05_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
