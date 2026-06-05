---
phase: 04-c-activity-hardening
reviewed: 2026-06-05T00:00:00Z
depth: standard
files_reviewed: 33
files_reviewed_list:
  - source/app/auth_activity.cpp
  - source/app/auth_activity.hpp
  - source/app/cheat_detail_activity.cpp
  - source/app/cheat_detail_activity.hpp
  - source/app/clear_cheats_activity.cpp
  - source/app/clear_cheats_activity.hpp
  - source/app/game_list_activity.cpp
  - source/app/game_list_activity.hpp
  - source/app/mod_browser_activity.cpp
  - source/app/mod_browser_activity.hpp
  - source/app/mod_detail_activity.cpp
  - source/app/mod_detail_activity.hpp
  - source/app/mod_manager_activity.cpp
  - source/app/mod_manager_activity.hpp
  - source/app/save_detail_activity.cpp
  - source/app/save_detail_activity.hpp
  - source/app/save_manager_activity.cpp
  - source/app/save_manager_activity.hpp
  - source/app/settings_activity.cpp
  - source/app/settings_activity.hpp
  - source/app/theme_browser_activity.cpp
  - source/app/theme_browser_activity.hpp
  - source/app/theme_detail_activity.cpp
  - source/app/theme_detail_activity.hpp
  - source/app/thomaz_activity.hpp
  - source/core/async_guard.hpp
  - source/platform/http_client.hpp
  - source/platform/http_client_curl.cpp
  - source/platform/mods/mod_download.cpp
  - source/platform/mods/mod_download.hpp
  - source/platform/themes/theme_download.cpp
  - source/platform/themes/theme_download.hpp
  - tests/test_async_guard.cpp
  - tests/test_save_sync.cpp
findings:
  critical: 2
  warning: 6
  info: 5
  total: 13
status: issues_found
---

# Phase 04: Code Review Report

**Reviewed:** 2026-06-05
**Depth:** standard
**Files Reviewed:** 33
**Status:** issues_found

## Summary

Phase 04 introduced a `ThomazActivity` base owning `alive`/`cancelled` guards plus a
`runAsync` wrapper (CONC-02), threaded a `cancelled` flag through the HTTP client and
download helpers (CONC-03), replaced four C-style view casts with null-guarded
`dynamic_cast` (DEBT-03), and added host tests (TEST-04).

The **core lifetime-guard machinery is correct**: `runAsync` captures the `alive`
shared_ptr by value, `run_if_alive` drops the continuation when the guard is cleared
or null, and the curl `CancelCtx`/`ProgressCtx` hold a shared_ptr copy of the flag so
it outlives the activity. The XFERINFO return conventions (return 1 only when
`cancelled` is set) are correct on both curl surfaces. The four DEBT-03
`dynamic_cast` conversions are sound where applied.

The defect is that **the hardening was applied selectively and left the same bug
classes alive in sibling code touched by this phase**. Adversarially, the headline
claim — "use-after-free in async callbacks is fixed" — does not hold: `runAsync`
continuations are guarded, but **Dialog button callbacks and `brls::sync([this]...)`
deferrals in the very same activities capture a raw `this` and run later with no
`alive` check.** And DEBT-03's cast hardening skipped `auth_activity.cpp`, which still
dereferences C-style casts unguarded. These are BLOCKER-class because they are
exactly the crash/UAF modes the phase set out to eliminate.

Findings below are ordered Critical → Warning → Info.

## Critical Issues

### CR-01: Dialog / `brls::sync` deferred callbacks capture raw `this` with no `alive` guard (use-after-free)

**File:** `source/app/settings_activity.cpp:172-175`, `source/app/clear_cheats_activity.cpp:129-138`, `source/app/mod_manager_activity.cpp:192-197`, `source/app/theme_detail_activity.cpp:98-107`
**Issue:**
CONC-02 guards the `runAsync` continuation, but the same activities create modal
Dialog buttons and click-deferred closures that capture a bare `this` and execute
**later**, after the activity can be popped — with no `alive->load()` check. This is
the precise use-after-free CONC-02 claims to close, surviving in phase-scoped files.

The intended pattern already exists elsewhere in this phase:
`save_detail_activity.cpp:284-291,425-428` and `mod_detail_activity.cpp:179-183` use
`[this, alive = this->alive]{ if (!alive->load()) return; ... }` on dialog buttons.
The following sites were missed:

- `settings_activity.cpp:172-175` — update-confirm dialog button:
  ```cpp
  dialog->addButton("thomaz/update/confirm_yes"_i18n,
                    [this, rel, status]() { this->installUpdate(rel, status); });
  ```
  Captures `this` and the raw `brls::Label* status` with no guard. If settings is
  popped while the dialog is open, the button calls `installUpdate` on a freed `this`
  and writes to a freed label.
- `clear_cheats_activity.cpp:129-138` — confirm-clear button captures `[this]`, then
  reads `this->selections`, calls `clear_cheat_files`, and `popActivity()` unguarded.
- `mod_manager_activity.cpp:192-197` — uninstall-confirm button captures
  `[this, tid, modName]` and calls `this->refreshList()` unguarded.
- `theme_detail_activity.cpp:98-105` — download button defers
  `brls::sync([this]{ ...this->startDownload(); })` with no `alive` capture.

**Fix:** Apply the established guard uniformly:
```cpp
dialog->addButton("thomaz/update/confirm_yes"_i18n,
                  [this, alive = this->alive, rel, status]() {
    if (!alive->load()) return;
    this->installUpdate(rel, status);
});
```
Stronger: add a `ThomazActivity::runOnUi(fn)` helper that wraps `brls::sync` +
`run_if_alive`, and route all deferred UI closures through it so unguarded `this`
capture becomes impossible.

### CR-02: `auth_activity.cpp` dereferences C-style view casts with no null check (DEBT-03 gap → crash)

**File:** `source/app/auth_activity.cpp:17-23, 60-62, 68`
**Issue:**
DEBT-03 replaced unchecked casts with null-guarded `dynamic_cast` in four activities
but left `AuthActivity` — touched in the same phase scope and inheriting
`ThomazActivity` — using unchecked C-style casts that are immediately dereferenced:
```cpp
auto* userCell = (brls::InputCell*)this->getView("usernameCell"); // 17
auto* passCell = (brls::InputCell*)this->getView("passwordCell"); // 18
userCell->init(...);   // 20 — null deref if id missing / wrong type
passCell->init(...);   // 22
...
auto* submitLabel = (brls::Label*)this->getView("submitLabel");   // 60
submitLabel->setText(...);                                        // 61
...
auto* status = (brls::Label*)this->getView("authStatus");         // 68
status->setText(...);                                             // 72
```
`getView` returns `nullptr` on a missing/renamed id, and a C-style cast does not
type-check, so a wrong-typed node casts to the wrong vtable. Either path is an
immediate crash — the exact fail-safe failure DEBT-03 was meant to prevent. The
remediation was applied selectively and skipped this activity. `submit` (line 51) is
also dereferenced unchecked.

**Fix:** Use the hardened pattern:
```cpp
auto* userCell = dynamic_cast<brls::InputCell*>(this->getView("usernameCell"));
auto* passCell = dynamic_cast<brls::InputCell*>(this->getView("passwordCell"));
if (!userCell || !passCell) {
    brls::Logger::error("auth.xml: username/password cell missing or wrong type");
    return;
}
```
Apply to `submitLabel`, `status`, `tabsRow`, and `submit` in the same file.

## Warnings

### WR-01: CONC-03 cancellation not wired into theme transfers or any browse GET

**File:** `source/app/theme_detail_activity.cpp:145-166`, `source/platform/themes/theme_download.cpp:32`, `source/app/settings_activity.cpp:142-146,191-193,212-214`, `source/app/mod_detail_activity.cpp:71-76`
**Issue:** The `cancelled` plumbing exists end-to-end (`IHttpClient::request` honors
`req.cancelled`; `download_file` honors its `cancelled` param), and theme/browse
*browse* calls do pass it. But the heaviest transfers do **not**:
- `theme_detail_activity.cpp:154` calls `download_theme(d, cancelled)` — good — and
  `theme_download.cpp:32` correctly forwards it. (Verified wired.) However
  `settings_activity.cpp` issues `client->get(...)` (null `cancelled`) for update
  check (`:143`), the multi-MB update **download** via `apply_downloaded_update`
  (`:193`, no cancel param at all), and the db refresh (`:213`). Popping settings
  mid-update-download leaves a large transfer running uncancellable.
- `mod_detail_activity.cpp:72` resolve uses `client->get(url)` (null `cancelled`),
  inconsistent with its own `startDownload` which threads the flag (`:200,208`).

Because the `alive` guard still drops the UI continuation there is no crash, but the
pool thread + in-flight transfer run to completion after teardown — the resource leak
CONC-03 exists to fix.
**Fix:** Capture `auto cancelled = this->cancelledFlag();` before each affected
`runAsync` and build `HttpRequest{ .url = url, .cancelled = cancelled }` instead of
the convenience `get()`. Thread a `cancelled` argument into `apply_downloaded_update`
→ `download_file` (the helper already accepts one).

### WR-02: xferInfo hook installed on *every* request app-wide; abort cannot be distinguished from real failure on the download path

**File:** `source/platform/http_client_curl.cpp:72-75,118-124`, `source/platform/mods/mod_download.cpp:92-99`
**Issue:** `http_client_curl.cpp` now sets `CURLOPT_NOPROGRESS 0` and installs the
xferInfo hook unconditionally, even for the majority of callers that pass no
`cancelled` flag — adding a per-progress-tick indirect call with a `shared_ptr` deref
to all HTTP traffic. More importantly, in `download_file` a cooperative abort returns
`CURLE_ABORTED_BY_CALLBACK`, and the caller cannot distinguish "teardown cancelled"
from "real failure": both yield `ok == false`. The silent-drop on teardown currently
relies entirely on the `alive` guard suppressing the toast — correct only while abort
is exclusively destructor-driven. That is true today but fragile.
**Fix:** (a) Only install the hook when `req.cancelled` is non-null, leaving the
no-cancel hot path untouched; (b) when `rc == CURLE_ABORTED_BY_CALLBACK`, set a
distinct sentinel (or leave `*err` empty, as `download_file` already does) so callers
can recognize a cooperative abort and never toast it.

### WR-03: `import_archive` runs after a download that completed just before teardown

**File:** `source/app/mod_detail_activity.cpp:208-215`
**Issue:** If `cancelled` flips true *after* `download_file` returns `true` (download
finished, then the dtor ran), the worker still proceeds into `import_archive(...)`,
doing filesystem work for a torn-down activity. The `popActivity()` onSync is
correctly dropped by `run_if_alive`, but the import side effect already executed.
**Fix:** Re-check between download and import:
```cpp
if (ok && !cancelled->load()) { ir = import_archive(...); }
else { results->first = false; }
```

### WR-04: Click-handler `brls::sync([this]...)` deferrals in browsers are unguarded (latent UAF)

**File:** `source/app/theme_browser_activity.cpp:56,64,232`, `source/app/mod_browser_activity.cpp:264,347-352`, `source/app/mod_manager_activity.cpp:100,270,293`
**Issue:** Several view-click handlers defer work with `brls::sync([this]{
this->reload()/runQuery()/refreshList()/runGameSearch()... })`, capturing a bare
`this` with no `alive` guard. `brls::sync` runs on the next UI frame; if the activity
is popped in that one-frame window, the closure dereferences a freed `this`. The phase
added `alive` guards to IME callbacks (`theme_browser_activity.cpp:244`,
`mod_browser_activity.cpp:103,258`) but left these sibling deferrals unguarded — same
root cause as CR-01, lower probability (single-frame window), hence WARNING.
**Fix:** Capture and check `alive`, or route through the `runOnUi` helper proposed in
CR-01.

### WR-05: `save_detail` conflict-retry can loop `doUpload`→`pushAtRevision`→`doUpload` unbounded

**File:** `source/app/save_detail_activity.cpp:342-344`
**Issue:** On a push returning `r.conflict`, the onSync calls `this->doUpload()`
again, which re-fetches status and may re-`pushAtRevision`. A backend that
persistently reports `conflict` (or two revisions that never reconcile) yields an
unbounded, user-invisible request loop with no attempt cap or backoff, hammering the
API.
**Fix:** Thread an attempt counter through `pushAtRevision`, cap automatic retries
(1–2), and surface an error toast after the cap.

### WR-06: `apply_downloaded_update` can apply a truncated `.nro` (no archive EOF guard)

**File:** `source/platform/mods/mod_download.cpp:82-101`, `source/app/settings_activity.cpp:191-193`
**Issue:** As the in-file comment notes, `download_file` does not guarantee a
byte-complete file; a server early-close after a 200 can produce a truncated file that
still passes `rc==CURLE_OK && 2xx && closeOk`. For mod archives the downstream
extractor's `ARCHIVE_EOF` check rejects truncation — but the self-update path
(`installUpdate` → `apply_downloaded_update`) downloads a raw `.nro` with **no**
archive EOF guard, so a truncated update binary could be written and applied.
**Fix:** For non-archive downloads, verify received bytes against `Content-Length`
(when present) before applying, or reject in `apply_downloaded_update` when the
transfer is short.

## Info

### IN-01: `run_if_alive` forces `onSync` through `std::function` erasure on every completion

**File:** `source/core/async_guard.hpp:15-22`, `source/app/thomaz_activity.hpp:71-73`
**Issue:** `runAsync` is a template, but the inner `brls::sync` lambda passes `s` to
`run_if_alive(const std::function<void()>&)`, type-erasing the callable into a
heap-allocating `std::function` on every async completion (all 14+ migrated sites),
on the UI thread. Correctness-neutral but avoidable.
**Fix:** `template <class F> bool run_if_alive(const std::shared_ptr<std::atomic<bool>>&, F&& onSync)` — stays header-only and host-testable.

### IN-02: Inner `brls::sync` lambda is `mutable` but only reads

**File:** `source/app/thomaz_activity.hpp:71`
**Issue:** The inner `brls::sync([aliveCapture, s]() mutable { ... })` is declared
`mutable` while only reading `aliveCapture` and copying `s` — misleading (implies
mutation). The outer lambda needs `mutable` to move-invoke `w`; the inner does not.
**Fix:** Drop `mutable` from the inner lambda.

### IN-03: Duplicated `makeFetcher` / divergent `strip_extension` helpers

**File:** `source/app/theme_browser_activity.cpp:26-38` vs `source/app/theme_detail_activity.cpp:28-40`; `source/app/mod_detail_activity.cpp:41-47` vs `source/app/mod_manager_activity.cpp:44-50`
**Issue:** `makeFetcher` is copy-pasted verbatim between the two theme activities;
`strip_extension` exists in two mod files with **different** leading-dot behavior
(`mod_detail` returns `.zip` unchanged; `mod_manager` strips it to ``). The divergence
is a latent inconsistency bug.
**Fix:** Hoist `makeFetcher` into a shared themezer helper and unify `strip_extension`
into one util with a single defined behavior.

### IN-04: No host test exercises the cancellation propagation

**File:** `tests/` (absence); `source/app/thomaz_activity.hpp:52-55`
**Issue:** TEST-04 covers `run_if_alive` and `classify→plan_push`, but nothing
asserts that the `cancelled` flag is propagated into `HttpRequest.cancelled` by any
client, nor that the composed retry loop (WR-05) terminates. The curl surfaces are
host-untestable, but the *propagation* is pure and coverable with a recording fake
`IHttpClient`.
**Fix:** Add a stub `IHttpClient` that records the `HttpRequest` it receives and
assert the activity's `cancelledFlag()` reaches `recorded.cancelled`.

### IN-05: Cancellation is cooperative only; dtor cannot stop a worker mid-flight — document it

**File:** `source/app/thomaz_activity.hpp:34-38`
**Issue:** `~ThomazActivity` sets `*alive=false; *cancelled=true;`, but a worker
already past its last guard check (inside `import_archive`, `install_theme`, or any
non-curl blocking call) keeps running on a detached pool thread. Correct and
intended, but the contract ("workers must reach a guard check promptly; long pure-CPU
/ FS work is not preemptible") deserves an explicit comment so future workers don't
assume the dtor force-stops them.
**Fix:** Expand the header comment to state cancellation is cooperative and observed
only at curl XFERINFO ticks and explicit `cancelled->load()` checks.

---

_Reviewed: 2026-06-05_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
