---
phase: 03-c-platform-hardening
reviewed: 2026-06-05T00:00:00Z
depth: standard
files_reviewed: 33
files_reviewed_list:
  - resources/i18n/en-US/thomaz.json
  - resources/i18n/fr/thomaz.json
  - resources/i18n/pt-BR/thomaz.json
  - resources/i18n/ru/thomaz.json
  - resources/i18n/zh-Hans/thomaz.json
  - source/app/cheat_detail_activity.cpp
  - source/app/clear_cheats_activity.cpp
  - source/app/game_list_activity.cpp
  - source/app/home_activity.cpp
  - source/app/mod_browser_activity.cpp
  - source/app/mod_detail_activity.cpp
  - source/app/mod_manager_activity.cpp
  - source/app/save_detail_activity.cpp
  - source/app/save_detail_activity.hpp
  - source/app/save_manager_activity.cpp
  - source/app/settings_activity.cpp
  - source/app/system_activity.cpp
  - source/app/theme_browser_activity.cpp
  - source/app/theme_detail_activity.cpp
  - source/app/tls_banner.cpp
  - source/app/tls_banner.hpp
  - source/platform/cheat_store.cpp
  - source/platform/curl_tls.hpp
  - source/platform/fs_util.cpp
  - source/platform/fs_util.hpp
  - source/platform/mods/libarchive_extractor.cpp
  - source/platform/mods/mod_actions.cpp
  - source/platform/mods/mod_download.cpp
  - source/platform/mods/mod_store.cpp
  - source/platform/mods/mod_store.hpp
  - source/platform/save_service_switch.cpp
  - source/platform/themes/theme_install.cpp
  - source/platform/tls_policy.hpp
  - tests/test_fs_util.cpp
  - tests/test_mod_store.cpp
  - tests/test_tls_policy.cpp
findings:
  critical: 1
  warning: 6
  info: 5
  total: 12
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-06-05
**Depth:** standard
**Files Reviewed:** 33
**Status:** issues_found

## Summary

This phase consolidates duplicated file-I/O helpers into `fs_util.{hpp,cpp}`, extracts a pure
host-testable TLS verification policy (`tls_policy.hpp`), adds a process-wide insecure-TLS latch
plus a per-activity warning banner (`tls_banner`), and migrates `cloudBusy` to `std::atomic`.
The refactor itself is clean: `grep` confirms exactly one definition each of `ensure_parent_dirs`
and `copy_tree` survives (no redefinition/ODR hazard), the i18n key `thomaz/tls/insecure_warning`
is present and valid JSON in all 5 locales, and the banner is wired into all 14 activities next to
`install_header_username`. The doctest oracle in `test_fs_util.cpp` and the canonical
`ensure_parent_dirs` are behaviourally equivalent for the absolute temp paths the tests exercise.

The dominant concern is the **deliberate TLS fail-open** (`tls_policy(false) -> {0,0}`). The code
documents this as intentional, but the threat model behind it is not airtight on the Switch and the
mitigation (a UI banner) has a real failure mode where it never renders. There are also several
robustness gaps in the new/migrated file-I/O helpers (silent partial copies on disk-full, no
symlink guard, unbounded marker reads) and a banner that can leak/never-attach depending on view
availability.

## Critical Issues

### CR-01: TLS verification silently disabled (fail-open) when CA bundle is missing

**File:** `source/platform/tls_policy.hpp:21-23`, `source/platform/curl_tls.hpp:42-54`
**Issue:** When the romfs CA bundle cannot be opened, `apply_curl_tls` sets
`CURLOPT_SSL_VERIFYPEER=0` and `CURLOPT_SSL_VERIFYHOST=0`, then performs HTTPS downloads (mods,
themes, cloud saves) with **no certificate validation at all**. This is a fail-open security
posture: a network attacker (rogue AP, captive portal, on-path MITM) can serve arbitrary content
over a forged "HTTPS" connection, and the app will download and then *extract archives / install
mods / restore save packages* from it. The documented justification — "the bundle is read-only in
romfs, so this only triggers on our own build error, not an attacker-removable file" — only covers
*why* the bundle goes missing, not the *consequence*: once the latch flips, every subsequent
transfer in the process is unauthenticated, including code/content that gets written to the SD card
and applied to games. The only compensating control is a UI banner that the user may not see or
understand (and that itself can fail to attach — see WR-01). For a homebrew that fetches and applies
third-party content, downgrading silently to plaintext-equivalent trust is a data-integrity /
remote-content-tampering risk, not merely a "keep the updater alive" convenience.
**Fix:** Do not fail open globally. At minimum, gate the insecure path so it is opt-in and
per-transfer, and never auto-applies to content that gets written/executed:
```cpp
// tls_policy.hpp — keep the pure seam, but make "insecure" an explicit caller decision,
// not an automatic fallback that the whole process inherits.
enum class TlsMode { Verify, InsecureAllowed };
inline TlsPolicy tls_policy(bool ca_present, TlsMode mode = TlsMode::Verify) {
    if (ca_present) return TlsPolicy{1L, 2L};
    return (mode == TlsMode::InsecureAllowed) ? TlsPolicy{0L, 0L} : TlsPolicy{1L, 2L};
}
```
Then in `apply_curl_tls`, when `ca_ok == false`, **leave verification ON** (the transfer will fail
loudly) for content-bearing downloads, and only allow the insecure mode for a narrowly-scoped
self-update channel if one exists. If product truly requires fail-open, require an explicit user
confirmation before the *first* insecure transfer rather than a passive banner, and document the
residual MITM risk in the security model. At a minimum, raise this to an explicit accepted-risk
sign-off rather than an inline code comment.

## Warnings

### WR-01: TLS warning banner can silently fail to attach (wrong view id, no fallback)

**File:** `source/app/tls_banner.cpp:20-29`
**Issue:** The banner is the sole user-facing mitigation for CR-01, yet it is best-effort and silent:
`getView("brls/applet_frame/hint_box")` followed by `if (!hintBox) return;`. The phase pattern map
(`03-PATTERNS.md:138-144`) specified `brls/applet_frame/header`; the implementation switched to
`hint_box`. If any activity's XML lacks that exact slot, or the `dynamic_cast<brls::Box*>` fails,
the warning is dropped with no log and no alternative surface — the user runs fully insecure with no
indication. Pairing a fail-open security decision with a fail-silent warning compounds CR-01.
**Fix:** Fall back to a guaranteed-visible surface and log when the slot is missing:
```cpp
auto* slot = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/hint_box"));
if (!slot) slot = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/header"));
if (!slot) {
    brls::Logger::warning("tls banner: no header slot on this activity; insecure mode unwarned");
    return;
}
```
Consider also a one-time `brls::Application::notify()` toast so the warning is not purely passive.

### WR-02: `download_file` writes can silently truncate on disk-full (write callback ignores fwrite short count semantics)

**File:** `source/platform/mods/mod_download.cpp:13-16, 70-71`
**Issue:** `writeToFile` returns `std::fwrite(...)`. On a short write (SD card full), `fwrite`
returns fewer bytes than requested; curl detects the mismatch and aborts, so `rc != CURLE_OK` and
the file is removed — that part is fine. However, the *success* path only checks
`std::fclose(out) == 0`. `fclose` flushing a buffered short-write may not reliably report ENOSPC on
all newlib/Switch configurations, and a 2xx HTTP status with an aborted body is already handled by
curl. The real gap: there is no verification that the number of bytes written equals
`Content-Length`. A server that closes a connection early after a 200 can yield a truncated archive
that still passes `(rc==CURLE_OK && 2xx && closeOk)` if curl treats the early close as success under
chunked/no-length responses.
**Fix:** Track written bytes in the callback and compare against `CURLINFO_SIZE_DOWNLOAD_T` /
expected length, or rely on libarchive to reject the truncated archive (it does — verify
`extract_archive` surfaces `r != ARCHIVE_EOF`, which it does at `libarchive_extractor.cpp:124`). At
minimum, document that download integrity depends on the downstream extractor's EOF check, since
`download_file` alone does not guarantee a complete file.

### WR-03: `copy_tree` / `copy_file` report success on partial copies in the `nullptr` err callers

**File:** `source/platform/fs_util.cpp:30-40`, `source/platform/save_service_switch.cpp:156, 206`
**Issue:** `copy_file` returns `false` on a short `fwrite` (good), and `copy_tree` aborts on the
first failure (good). But `save_service_switch` calls `copy_tree(..., nullptr)` during **save
restore** (`restore()` at line 206: `clear_tree(mountRoot)` then `copy_tree(src, mountRoot, ...)`).
The current save is wiped *first*, then the backup is copied back. If `copy_tree` fails midway
(disk/IO error on the save partition), the original save is already destroyed and only a partial
backup is restored. The code does check the `copy_tree` return before `fsdevCommitDevice`, so an
*uncommitted* mount is discarded — but this relies entirely on the commit-gating being correct and
on `fsdevMountSaveData` semantics discarding uncommitted writes. There is no second-level integrity
check (e.g. file count) that the restored tree matches the backup.
**Fix:** This is mostly mitigated by the commit gate, but add a defensive assertion: after
`copy_tree` succeeds, verify the restored file count equals the backup file count before committing,
and on any `copy_tree` failure ensure `fsdevUnmountDevice` happens *without* commit (it does). Add a
comment at `save_service_switch.cpp:205-207` making the "wipe-then-copy is safe only because commit
is gated" invariant explicit so a future edit cannot break it.

### WR-04: `read_marker` / `read_text_file` perform unbounded reads of attacker-or-corruption-influenced files

**File:** `source/platform/mods/mod_store.cpp:66-80`, `source/platform/cheat_store.cpp:10-23`
**Issue:** `read_marker` and `read_text_file` loop `fread` into an unbounded `std::string` with no
size cap. A marker file is supposed to be one short line, and cheat files are small, but these read
from SD-card paths that a user (or a malformed download) can replace with an arbitrarily large file.
`read_marker`'s result is later used as a directory name (`active_mod` -> `mod_staging_dir(title_id,
mod_name)`), so a multi-megabyte marker becomes a multi-megabyte path string. Not a memory-safety
bug, but an unbounded-allocation / robustness gap on a memory-constrained console.
**Fix:** Cap the read:
```cpp
std::optional<std::string> read_marker(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::nullopt;
    std::string out;
    char buf[256];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < 4096)
        out.append(buf, n);
    std::fclose(f);
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}
```

### WR-05: `read_text_file` and `read_marker` ignore `ferror` — corrupted reads look like valid empty/partial content

**File:** `source/platform/cheat_store.cpp:18-22`, `source/platform/mods/mod_store.cpp:73-79`
**Issue:** Both functions break the read loop on `fread() == 0`, which is true for both EOF and a
read error. Neither checks `std::ferror(f)`. A mid-read I/O failure (flaky SD card) returns a
*truncated but apparently successful* string. Compare with `save_service_switch.cpp:69-71`, which
correctly checks `std::ferror(in)` and fails the read — the new/sibling helpers should match that
standard. For `read_marker` this can silently select the wrong active mod; for `read_text_file`
(version cache JSON) it yields a half-parsed cache.
**Fix:** Mirror the `read_tree` precedent:
```cpp
while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
bool readErr = std::ferror(f) != 0;
std::fclose(f);
if (readErr) return std::nullopt;
```

### WR-06: `copy_tree` follows symlinks via `stat` (no loop / escape guard) for the host-test path

**File:** `source/platform/fs_util.cpp:12-15, 58-90`
**Issue:** `is_dir` uses `::stat` (follows symlinks), not `::lstat`. On the Switch FAT filesystem
symlinks do not exist, so this is harmless there. But `copy_tree` is also compiled and run on the
host (`test_mod_store.cpp`) and is a general-purpose `thomaz::` utility now. A symlink inside a
source tree would be followed: a directory symlink causes recursion outside the intended tree (and
potentially an infinite loop / unbounded copy if it points to an ancestor). Since this is now a
reusable utility rather than a Switch-only helper, the unguarded behaviour is a latent defect for
any future host or desktop caller.
**Fix:** Use `::lstat` in `is_dir` (or a dedicated `is_symlink` check) and skip symlinks in
`copy_tree`, or document loudly in `fs_util.hpp` that `copy_tree` assumes a symlink-free tree.

## Info

### IN-01: `xferInfo` casts `curl_off_t` (signed) to `std::uint64_t` — negative "unknown size" becomes huge

**File:** `source/platform/mods/mod_download.cpp:22-27`
**Issue:** `dltotal` is `-1` (or `0`) while the total size is unknown. The cast
`(std::uint64_t)dltotal` turns `-1` into `0xFFFFFFFFFFFFFFFF`, which a progress-bar consumer may
render as a nonsensical denominator.
**Fix:** Clamp: `std::uint64_t total = dltotal > 0 ? (std::uint64_t)dltotal : 0;` and treat 0 as
"indeterminate" in the UI.

### IN-02: `extract_archive` lists the archive twice (full pass just to compute `total`)

**File:** `source/platform/mods/libarchive_extractor.cpp:62`
**Issue:** `list_archive_entries(archive_path)` re-opens and fully walks the archive solely to get a
count for the progress callback, then opens it again to extract. Correctness is fine; it is wasteful
on a console. (Flagged as Info only — performance is out of v1 scope, but the double-open is a
maintainability smell worth a comment.)
**Fix:** Pass `total = -1` (indeterminate) to `progress` and skip the pre-count, or cache entries
from the first pass.

### IN-03: `uid_hex` / `sscanf` round-trip uses `%016lx` with `unsigned long` cast — fragile on LP64 assumptions

**File:** `source/platform/save_service_switch.cpp:39-42, 127-128, 149-150, 192-193, 234-235`
**Issue:** `uid.uid[]` is `u64`; formatting/parsing via `(unsigned long*)` and `%016lx` assumes
`unsigned long == 64-bit`. True on aarch64/Switch, but the cast through a differently-typed pointer
in `sscanf` is technically aliasing-fragile. Pre-existing pattern (not introduced this phase) but
adjacent to the touched `copy_tree` migration.
**Fix:** Use `%016" PRIx64` with `std::uint64_t` and parse into a local `u64` then assign, avoiding
the pointer cast. Low priority — Switch-only, works today.

### IN-04: `tls_insecure_flag()` is a non-atomic process-global read/written across threads

**File:** `source/platform/curl_tls.hpp:13-16, 53`
**Issue:** `apply_curl_tls` runs on download worker threads (`brls::async`) and sets
`tls_insecure_flag() = true`, while `tls_is_insecure()` is read on the main UI thread in
`install_tls_warning_banner`. The flag is a plain `bool`, not `std::atomic<bool>`. It is a one-way
latch so the race is benign in practice (worst case: banner appears one screen later), but it is a
data race per the C++ memory model — ironic given the same phase migrated `cloudBusy` to
`std::atomic` specifically to avoid exactly this.
**Fix:** Make it `std::atomic<bool>`:
```cpp
inline std::atomic<bool>& tls_insecure_flag() { static std::atomic<bool> f{false}; return f; }
inline bool tls_is_insecure() { return tls_insecure_flag().load(); }
```

### IN-05: D-05 oracle and canonical `ensure_parent_dirs` diverge on leading-slash root segment (untested edge)

**File:** `tests/test_fs_util.cpp:15-21` vs `source/platform/fs_util.cpp:48-56`
**Issue:** The oracle guards with `acc.size() > 1`; the canonical guards with `!dir.empty()`. They
agree for every path the tests actually exercise (absolute temp paths whose first real segment is
non-empty), so the "equivalence" test passes. But the two would diverge on a path like `"//x"`
(canonical: `dir = "/"` is non-empty -> `mkdir("/")`; oracle: `acc = "/"` size 1 -> skipped).
Neither matters on Switch paths, but the test asserts an equivalence that is not universal — it is
equivalence-over-the-tested-inputs, not equivalence-in-general. Worth a comment so a future reader
does not over-trust the "canonical == oracle" claim.
**Fix:** Either add a leading/double-slash case to demonstrate the bound, or annotate the test that
equivalence holds only for repo-shaped paths (no leading-`/`-only or double-slash segments).

---

_Reviewed: 2026-06-05_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
