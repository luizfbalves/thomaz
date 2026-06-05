---
phase: 04-c-activity-hardening
fixed_at: 2026-06-05T00:00:00Z
review_path: .planning/phases/04-c-activity-hardening/04-REVIEW.md
iteration: 1
findings_in_scope: 8
fixed: 8
skipped: 0
status: all_fixed
---

# Phase 04: Code Review Fix Report

**Fixed at:** 2026-06-05
**Source review:** .planning/phases/04-c-activity-hardening/04-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 8 (2 Critical + 6 Warning; Info findings IN-01..IN-05 out of scope)
- Fixed: 8
- Skipped: 0

**Build/verification note:** The host test suite stays green at **185/185** after
all fixes. The three platform translation units I touched
(`mod_download.cpp`, `self_update.cpp`, `http_client_curl.cpp`) additionally pass a
real compiler syntax check (`g++ -std=c++17 -Isource -fsyntax-only`). The Borealis
UI activity `*.cpp` files cannot be compiled host-side (they need borealis/libcurl
headers absent from this environment), so those were verified by re-read against the
established alive-guard / dynamic_cast idioms already used elsewhere in the phase. A
full desktop (`-DUSE_SDL2=ON`) / Switch build was not run.

## Fixed Issues

### CR-01: Dialog / `brls::sync` deferred callbacks capture raw `this` with no `alive` guard

**Files modified:** `source/app/settings_activity.cpp`, `source/app/clear_cheats_activity.cpp`, `source/app/mod_manager_activity.cpp`, `source/app/theme_detail_activity.cpp`
**Commits:** `9c037e8` (settings update-confirm dialog button), `c44d341` (clear-cheats confirm button, mod-manager uninstall-confirm button, theme-detail download `brls::sync`)
**Applied fix:** Added `[alive = this->alive]` capture + `if (!alive->load()) return;`
early-return to each deferred dialog button / `brls::sync` closure, matching the
existing `save_detail_activity.cpp` / `mod_detail_activity.cpp` pattern. Settings was
committed separately from the other three sites because settings also carries WR-01
changes and per-finding atomicity required isolating the CR-01 hunk.

### CR-02: `auth_activity.cpp` dereferences C-style view casts with no null check

**Files modified:** `source/app/auth_activity.cpp`
**Commit:** `16fad71`
**Applied fix:** Replaced every `(brls::T*)this->getView(id)` with
`dynamic_cast<brls::T*>(this->getView(id))` followed by a `brls::Logger::error` + early
return null guard, for `usernameCell`, `passwordCell`, `tabsRow`, `submitBtn`,
`submitLabel`, and `authStatus` — matching the DEBT-03 hardening used in the other four
activities.

### WR-01: CONC-03 cancellation not wired into settings transfers / mod-detail resolve

**Files modified:** `source/app/settings_activity.cpp`, `source/app/mod_detail_activity.cpp`, `source/platform/self_update.hpp`, `source/platform/self_update.cpp`
**Commit:** `97860ef`
**Applied fix:** Captured `this->cancelledFlag()` before each affected `runAsync` and
built `HttpRequest{ .url=..., .cancelled=cancelled }` (via `request()`) in place of the
convenience `get()` for the settings update-check, the db-refresh, and the mod-detail
resolve fetcher. Threaded a new optional `cancelled` parameter through
`apply_downloaded_update` → `download_file` so the self-update download aborts on
teardown.

### WR-02: xferInfo hook installed on every request app-wide

**Files modified:** `source/platform/http_client_curl.cpp`
**Commit:** `fd5ce78`
**Applied fix:** Gated `CURLOPT_NOPROGRESS`/`CURLOPT_XFERINFOFUNCTION`/`XFERINFODATA`
behind `if (req.cancelled)` so only cancel-capable callers install the hook; the
no-cancel hot path is untouched. The `request()` abort path already distinguishes a
cooperative `CURLE_ABORTED_BY_CALLBACK` (body cleared, status 0) from a transport
failure, satisfying part (b).

### WR-03: `import_archive` runs after a download that completed just before teardown

**Files modified:** `source/app/mod_detail_activity.cpp`
**Commit:** `84006b2`
**Applied fix:** Re-check `cancelled->load()` between `download_file` returning `true`
and `import_archive`; when cancelled, skip the filesystem import (the onSync is already
dropped by `run_if_alive`).

### WR-04: Click-handler `brls::sync([this]...)` deferrals in browsers are unguarded

**Files modified:** `source/app/theme_browser_activity.cpp`, `source/app/mod_browser_activity.cpp`, `source/app/mod_manager_activity.cpp`
**Commit:** `78db230`
**Applied fix:** Added `[alive]` capture + `if (!alive->load()) return;` to every
click-deferred `brls::sync` closure (reload / runQuery / runGameSearch /
runGlobalSearch / importFlow / refreshList), including the inner syncs nested inside the
already-guarded IME callbacks in both browsers.

### WR-05: `save_detail` conflict-retry can loop unbounded

**Files modified:** `source/app/save_detail_activity.cpp`, `source/app/save_detail_activity.hpp`
**Commit:** `7db13ff`
**Status:** fixed — **requires human verification** (logic/control-flow change)
**Applied fix:** Added a `cloudConflictRetries` counter (cap `kMaxConflictRetries = 2`).
`doUpload(bool autoRetry=false)` resets the budget on a user-initiated upload; the
conflict path now passes `autoRetry=true` and increments the counter; a clean push
resets it; exceeding the cap surfaces `cloud_err_generic` instead of re-uploading. The
retry-budget semantics should be confirmed by a human before release.

### WR-06: `apply_downloaded_update` can apply a truncated `.nro`

**Files modified:** `source/platform/mods/mod_download.cpp`
**Commit:** `9d2a13e`
**Status:** fixed — **requires human verification** (transfer-integrity logic)
**Applied fix:** `download_file` now tallies bytes written via a `FileSink` and compares
the total to the server's advertised `CURLINFO_CONTENT_LENGTH_DOWNLOAD_T`; a short
transfer fails with a `truncated download` error instead of succeeding. Length-less
(chunked) responses still fall back to the downstream archive-EOF check, as documented
in the updated comment. The byte-count comparison and the `advertisedLen < 0`
fallback should be human-verified against real GameBanana/GitHub responses.

---

_Fixed: 2026-06-05_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
