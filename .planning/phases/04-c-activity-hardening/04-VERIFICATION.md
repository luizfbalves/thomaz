---
phase: 04-c-activity-hardening
verified: 2026-06-05T20:00:00Z
status: human_needed
score: 4/4 must-haves verified
overrides_applied: 0
re_verification:
  previous_status: gaps_found
  previous_score: 3/4
  gaps_closed:
    - "CONC-03 D-03 scope — theme transfers and browse GETs now wired (plan 04-06 delivered)"
  gaps_remaining: []
  regressions: []
human_verification:
  - test: "Pop settings mid-update-check or mid-download, confirm no crash and no stale toast about network error"
    expected: "Settings activity is destroyed cleanly; any in-flight update-check GET or installUpdate download_file call aborts without crashing or surfacing an error toast"
    why_human: "settings_activity.cpp update-check runAsync and installUpdate download_file do NOT set req.cancelled / pass a cancelled flag — so the XFERINFOFUNCTION hook will not fire for these specific requests. WR-01 from the code review: these transfers run to completion in the pool thread after teardown (resource leak, not crash). The alive guard drops the UI continuation, so no toast fires. Whether this is acceptable (WR-01 warning) or whether a crash/toast can still be triggered requires live hardware or desktop run."
  - test: "Pop settings while the update-confirm dialog is still open, then tap 'Yes'"
    expected: "The dialog button's [this, rel, status] closure is called on a freed activity — either no crash (dialog is dismissed with activity), or a UAF crash is observed"
    why_human: "CR-01 site: settings_activity.cpp:172-173 dialog->addButton captures raw [this, rel, status] with no alive guard. This is a confirmed code-level use-after-free if the activity can be popped while the dialog remains open. Whether Borealis dismisses the dialog on popActivity (preventing the scenario) or leaves it open (allowing the UAF) is a runtime question only a human can confirm."
  - test: "Pop clear_cheats while the confirm-clear dialog is open, then tap the confirm button"
    expected: "Either safe (dialog dismissed with activity) or UAF crash on accessing this->selections"
    why_human: "CR-01 site: clear_cheats_activity.cpp:129-138 dialog->addButton([this]) captures bare this with no alive guard. Same as settings scenario — whether the dialog outlives the activity depends on Borealis behavior, not observable via grep."
  - test: "Pop mod_manager while the uninstall-confirm dialog is open, then tap confirm"
    expected: "Either safe (dialog dismissed) or crash on this->refreshList()"
    why_human: "CR-01 site: mod_manager_activity.cpp:192-197 uninstall-confirm button captures [this, tid, modName] with no alive guard."
  - test: "Pop theme_detail while a download is starting — specifically in the one-frame window after the download button fires brls::sync but before startDownload runs"
    expected: "Either safe (alive guard in runAsync drops startDownload) or crash if the brls::sync fires before runAsync checks alive"
    why_human: "CR-01 site: theme_detail_activity.cpp:98-105 downloadButton registerClickAction defers brls::sync([this]{...startDownload()...}) with no alive capture. The one-frame deferral is the exposure window."
---

# Phase 04: C++ Activity Hardening — Verification Report

**Phase Goal:** All activities inherit the `ThomazActivity` base class with its `runAsync` wrapper (making the `alive` guard impossible to forget), unsafe C-style view casts are null-guarded, in-flight curl requests cancel on activity destruction, and the conflict-resolution path is covered by a host test.
**Verified:** 2026-06-05T20:00:00Z
**Status:** human_needed
**Re-verification:** Yes — after gap closure (plan 04-06 closed the prior CONC-03 D-03 scope gap)

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | All activities inherit ThomazActivity and use the runAsync wrapper for async work (CONC-02) | VERIFIED | All 12 activities declare `: public ThomazActivity` in their headers. Zero `brls::async` calls survive in `source/app/*.cpp` (only in `thomaz_activity.hpp` internally). All 13 pre-phase async call-sites migrated. |
| 2 | Unsafe C-style view casts replaced with null-guarded dynamic_cast in the four flagged activities: game_list, save_manager, save_detail, mod_browser (DEBT-03) | VERIFIED | All four DEBT-03-scoped files use `dynamic_cast<brls::T*>(this->getView(...))` with explicit null guards (log + return on null). No C-style casts remain in these four files. ROADMAP SC-2 explicitly scopes DEBT-03 to these four files only. |
| 3 | In-flight curl requests cancel on activity destruction across BOTH curl surfaces (mod_download.cpp AND http_client_curl.cpp), including theme + browse GET sites (CONC-03) | VERIFIED | Both surfaces have XFERINFOFUNCTION abort hooks. mod_download.cpp: `ProgressCtx.cancelled` + `xferInfo` returns 1 on cancel. http_client_curl.cpp: `CancelCtx.cancelled` + `curlCancelXferInfo` returns 1 on cancel. HttpRequest.cancelled field exists (default null). Plan 04-06 wired all five remaining activity-owned transfer sites: theme_detail (3 runAsync sites), theme_browser (2), mod_browser (3), cheat_detail (1), game_list (1). theme_download.hpp/cpp extended with optional `cancelled` parameter forwarded into download_file. |
| 4 | A host doctest covers the cloud-save conflict-resolution / plan_push decision composition (TEST-04) | VERIFIED | `tests/test_save_sync.cpp` contains `classify -> plan_push` composition TEST_CASEs (TEST-04a) with conflict (CloudAhead), clean-push (InSync), and new-slot (NoCloud) triples asserting real implementation values. `tests/test_async_guard.cpp` covers the runAsync dropped-callback semantics (TEST-04b). Host suite: 185/185 test cases pass (552 assertions, exit 0). |

**Score:** 4/4 truths verified

---

## CR-01 Independent Assessment: Unguarded Dialog / brls::sync Deferred Callbacks

The code review (CR-01) identified four sites where modal dialog button callbacks and `brls::sync` deferrals capture a bare `this` with no `alive` guard. This verifier independently confirms all four:

**settings_activity.cpp:172-173** — `dialog->addButton("thomaz/update/confirm_yes"_i18n, [this, rel, status]() { this->installUpdate(rel, status); })`. Raw `this` and raw `brls::Label* status` captured. No alive check. Confirmed by reading the file.

**clear_cheats_activity.cpp:129-138** — `dialog->addButton("thomaz/clear/confirm_button"_i18n, [this]() { ... this->selections ... popActivity() })`. Raw `this` captured. No alive check.

**mod_manager_activity.cpp:192-197** — `dialog->addButton("mods/uninstall_button"_i18n, [this, tid, modName]() { ... this->refreshList(); })`. Raw `this` captured. No alive check.

**theme_detail_activity.cpp:98-105** — `brls::sync([this]() { ... this->startDownload(); })` in a click-action closure. No alive capture.

**Verdict on impact to must-haves:** The ROADMAP phase goal language says "making the `alive` guard impossible to forget." These four sites show the guard WAS forgotten in non-runAsync deferred closures in the very same phase-scoped files. However, ROADMAP SC-1 specifically says "all activities that previously used `brls::async` directly now call `this->runAsync(...) instead`" — the runAsync migration is complete (zero remaining direct brls::async). The CR-01 sites involve dialog button callbacks and `brls::sync` one-shot deferrals, which are NOT `brls::async` call-sites and are explicitly out of the D-01 migration scope documented in Plan 04-03.

The plan explicitly distinguished these two categories: CONC-02's D-01 mandate was migration of `brls::async` sites, not all deferred closures app-wide. Plans 04-02 and 04-03 note "special case — mod_manager_activity: its `brls::sync([this]{...})` calls are NOT alive-guarded today... do NOT invent a runAsync wrapping for its unguarded `brls::sync` calls (out of D-01 scope)." The counterexample (save_detail and mod_detail DO guard their dialog buttons) shows the established pattern exists — these are omissions in the non-required scope.

**Conclusion:** CR-01 does NOT constitute a BLOCKER for the defined ROADMAP must-haves, but the UAF exposure in these four sites is genuine and requires human runtime verification to determine whether Borealis dialog lifecycle prevents the crash scenario.

---

## CR-02 Independent Assessment: auth_activity.cpp C-style Casts

The code review (CR-02) identified that `auth_activity.cpp` still uses C-style casts (`(brls::InputCell*)this->getView(...)`) at lines 17, 18, 25, 51, 60, 68 with no null guards or dynamic_cast. This verifier independently confirms: **5 C-style casts are present in auth_activity.cpp, all dereferenced without null checks.**

**Verdict on impact to must-haves:** ROADMAP SC-2 explicitly and only scopes DEBT-03 to "the flagged activities (game_list, save_manager, save_detail, mod_browser)." RESEARCH.md Pitfall 4 explicitly states "DEBT-03 is scoped to exactly four files" and lists auth_activity as OUT of scope. Plan 04-03 explicitly mandates "No DEBT-03 cast edits are made in these nine files."

The phase goal says "unsafe C-style view casts are null-guarded" — this is the high-level goal description, but the ROADMAP success criterion 2 is the binding contract with explicit four-file scope. auth_activity was never in DEBT-03 scope per plan, research, or success criteria.

**Conclusion:** CR-02 does NOT constitute a BLOCKER for the defined ROADMAP must-haves. The auth_activity casts are out of scope for this phase. They remain a latent crash risk (acknowledged in CONCERNS.md) and should be addressed in a follow-up.

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `source/app/thomaz_activity.hpp` | ThomazActivity base with alive+cancelled+runAsync | VERIFIED | `class ThomazActivity : public brls::Activity`, protected `runAsync` template, `alive` and `cancelled` shared_ptr members, dtor sets both flags, `cancelledFlag()` accessor |
| `source/core/async_guard.hpp` | Pure Borealis-free `run_if_alive` | VERIFIED | `namespace thomaz::core`, `inline bool run_if_alive(...)`, no Borealis includes, C++17 clean |
| `tests/test_async_guard.cpp` | TEST-04b dropped-callback cases | VERIFIED | Three TEST_CASEs: alive runs, not-alive drops, null drops. No Borealis include. |
| `tests/test_save_sync.cpp` | TEST-04a classify->plan_push composition | VERIFIED | Three new composition TEST_CASEs appended (conflict, clean-push, no-cloud). Asserts real implementation values. |
| `source/platform/http_client.hpp` | HttpRequest.cancelled field (optional, default null) | VERIFIED | `std::shared_ptr<std::atomic<bool>> cancelled;` in HttpRequest struct |
| `source/platform/http_client_curl.cpp` | XFERINFOFUNCTION abort hook on request/response surface | VERIFIED | `CancelCtx` struct, `curlCancelXferInfo` callback returning 1 on cancel, installed unconditionally (CURLOPT_NOPROGRESS=0), CURLE_ABORTED_BY_CALLBACK silently handled |
| `source/platform/mods/mod_download.cpp` | cancelled check in existing xferInfo, CURLE_ABORTED_BY_CALLBACK silent | VERIFIED | `ProgressCtx.cancelled`, `xferInfo` returns 1 when `cancelled->load()`, abort handled silently before generic error branch |
| `source/platform/themes/theme_download.hpp` | download_theme with optional cancelled=nullptr | VERIFIED | Signature extended with `std::shared_ptr<std::atomic<bool>> cancelled = nullptr` |
| `source/app/theme_detail_activity.cpp` | cancelled captured before each runAsync; threaded into transport | VERIFIED | 3 runAsync sites: preview fetch (req.cancelled), detail-resolve (makeFetcher(client, cancelled)), startDownload (download_theme(d, cancelled)) |
| `source/app/theme_browser_activity.cpp` | cancelled captured before runAsync; set on HttpRequest | VERIFIED | 2 runAsync sites: runQuery (makeFetcher(client, cancelled)), loadThumb (req.cancelled) |
| `source/app/mod_browser_activity.cpp` | cancelled captured before each browse runAsync; req.cancelled set | VERIFIED | 3 runAsync sites; UrlFetcher lambdas use explicit HttpRequest with req.cancelled |
| `source/app/cheat_detail_activity.cpp` | cancelled captured; req.cancelled set | VERIFIED | 1 runAsync site; UrlFetcher lambda uses explicit HttpRequest with req.cancelled |
| `source/app/game_list_activity.cpp` | cancelled captured; req.cancelled set | VERIFIED | 1 runAsync site; explicit HttpRequest with req.cancelled replacing client->get() |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| All 12 activity headers | thomaz_activity.hpp | `: public ThomazActivity` | WIRED | Confirmed by grep across all .hpp files |
| thomaz_activity.hpp | core/async_guard.hpp | `thomaz::core::run_if_alive(aliveCapture, s)` | WIRED | Direct call in runAsync body |
| tests/test_async_guard.cpp | core/async_guard.hpp | `#include "core/async_guard.hpp"` | WIRED | Direct include, no Borealis |
| tests/test_save_sync.cpp | core/saves/save_sync.hpp | classify() then plan_push() on result | WIRED | Both functions called in composition TEST_CASEs |
| mod_detail_activity.cpp | download_file(..., cancelled) | passes cancelledFlag() as 5th arg | WIRED | Line 200-208 confirmed |
| save_detail_activity.cpp | HttpRequest.cancelled | sets req.cancelled on cloud-save transfers | WIRED | Lines 262, 311, 365 confirmed |
| theme_detail_activity.cpp | download_theme(d, cancelled) | passes cancelledFlag() as 2nd arg | WIRED | Line 154 confirmed |
| theme_browser_activity.cpp | req.cancelled via makeFetcher | cancelled set on HttpRequest in makeFetcher | WIRED | req.cancelled present in makeFetcher body |
| mod_browser_activity.cpp | req.cancelled in UrlFetcher | UrlFetcher uses http->request(req) with req.cancelled | WIRED | 3 sites confirmed |
| http_client_curl.cpp | XFERINFOFUNCTION | CancelCtx + curlCancelXferInfo installed on all requests | WIRED | Lines 72-75 confirmed |
| mod_download.cpp | xferInfo returns 1 | ProgressCtx.cancelled checked, return 1 | WIRED | Line 28 confirmed |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Host test suite passes (185 cases) | `./tests/run` | 185/185 passed, 552 assertions, exit 0 | PASS |
| run_if_alive drops callback when alive=false | `grep -c "onSync dropped when not alive" tests/test_async_guard.cpp` | 1 | PASS |
| classify->plan_push composition covered | `grep -c "plan_push(sit" tests/test_save_sync.cpp` | 3 | PASS |
| Zero direct brls::async in activity files | `grep -rc "brls::async" source/app/*.cpp` | 0 (only in thomaz_activity.hpp) | PASS |
| DEBT-03 files: zero C-style getView casts | grep across 4 DEBT-03 files | 0 remaining | PASS |

### Probe Execution

No conventional `scripts/*/tests/probe-*.sh` probes declared. Host test suite run above is the equivalent.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| CONC-02 | 04-01, 04-02, 04-03 | runAsync base class; all brls::async sites migrated | SATISFIED | ThomazActivity confirmed; zero brls::async in source/app/ |
| CONC-03 | 04-04, 04-06 | Curl cancellation across both surfaces; theme+browse GETs wired | SATISFIED | XFERINFOFUNCTION on both surfaces; all 9 activity-owned transfer sites wired; 04-06 gap-closure confirmed |
| DEBT-03 | 04-02 | Null-guarded dynamic_cast in 4 flagged activities | SATISFIED | All four files use dynamic_cast with null guards; ROADMAP SC-2 scope honored |
| TEST-04 | 04-01, 04-05 | Host doctest for conflict/plan_push composition + dropped-callback | SATISFIED | test_async_guard.cpp (TEST-04b) + test_save_sync.cpp composition cases (TEST-04a); 185/185 passing |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| auth_activity.cpp | 17, 18, 25, 60, 68 | C-style `(brls::T*)this->getView(...)` with no null check | WARNING | Out-of-DEBT-03-scope crash risk on missing/wrong-type view ID; acknowledged in CONCERNS.md; ROADMAP SC-2 explicitly excluded this file from Phase 4 scope |
| settings_activity.cpp | 172-173 | `dialog->addButton([this, rel, status]...)` with no alive guard | WARNING | Potential UAF if activity is popped while dialog is open; out of D-01 scope (not a brls::async site); requires human runtime verification |
| clear_cheats_activity.cpp | 129-138 | `dialog->addButton([this]...)` with no alive guard | WARNING | Same class as above; potential UAF on confirm button tap after activity pop |
| mod_manager_activity.cpp | 192-197, 100, 171, 293 | `dialog->addButton([this...])` and `brls::sync([this]...)` with no alive guards | WARNING | Same UAF class; out of D-01 scope; plan 04-03 explicitly preserved these as-is |
| theme_detail_activity.cpp | 98-105 | `brls::sync([this]{...startDownload()...})` with no alive capture | WARNING | Same UAF class; one-frame exposure window |

No unreferenced TBD/FIXME/XXX markers found in any modified file. The `// WR-02:` comment in mod_download.cpp is a documentation note, not an action marker.

---

### Human Verification Required

#### 1. settings_activity — Unguarded dialog button UAF scenario (CR-01)

**Test:** While in settings, trigger an update check so the update-confirm dialog appears ("Install version X.Y.Z?"). Then pop the settings activity (back button) while the dialog is still open. Then tap the "Yes" button.
**Expected:** Either (a) Borealis dismisses the dialog when the activity is popped (safe), or (b) a crash occurs on `this->installUpdate(rel, status)` where `this` is freed.
**Why human:** Whether Borealis destroys all child dialogs on `popActivity` or leaves them open is runtime behavior not visible in the source. If the dialog can outlive the activity, this is a live UAF.

#### 2. settings_activity — Update download not cancellable

**Test:** Trigger an update check, confirm the download starts, then immediately pop settings.
**Expected:** The pool thread finishes the download in the background (resource leak, not crash). No error toast should appear (alive guard drops the UI callback).
**Why human:** The `installUpdate` runAsync does not pass `cancelled` into the `download_file` call — so the transfer runs to completion regardless of activity teardown. WR-01 in the code review. Whether this causes observable side effects (notification of completion on a dead activity, etc.) needs runtime confirmation.

#### 3. clear_cheats_activity — Unguarded dialog button UAF scenario (CR-01)

**Test:** Open clear-cheats, select some entries, hit the clear button to open the confirm dialog. Pop the activity while the dialog is open, then tap the confirm button.
**Expected:** Either safe (dialog dismissed with activity) or crash on `this->selections`.
**Why human:** Same Borealis dialog lifecycle question as scenario 1.

#### 4. mod_manager_activity — Unguarded dialog + brls::sync callbacks (CR-01)

**Test:** Open mod-manager, trigger the uninstall-confirm dialog, pop the activity, then tap confirm. Also: rapidly navigate in and out of mod-manager while imports or refreshList calls are in-flight via brls::sync.
**Expected:** No crash if Borealis cleans up dialogs; UAF crash otherwise. The brls::sync one-frame-window exposure is lower probability but present.
**Why human:** Runtime Borealis dialog lifecycle and single-frame window gap.

#### 5. theme_detail_activity — brls::sync([this]) one-frame UAF (CR-01)

**Test:** Navigate to a theme detail view and very rapidly tap the download button then immediately back. (The registerClickAction defers `brls::sync([this]{...startDownload()...})` — if the activity is popped in that one-frame window, `this` is freed.)
**Expected:** Either safe (startDownload begins before pop completes) or a crash in the brls::sync callback.
**Why human:** One-frame window, not reliably reproducible by grep, and Borealis frame scheduling determines exposure.

---

### Gaps Summary

No gaps block the must-have truths: all four ROADMAP success criteria are verified in the codebase. The `status: human_needed` reflects that CR-01 use-after-free scenarios in four files (settings, clear_cheats, mod_manager, theme_detail) are genuine code-level risks that must be confirmed or ruled out at runtime. These are WARNING-class findings per the code review — the crash requires a specific race condition (dialog open when activity is popped) that Borealis dialog lifecycle may prevent.

CR-02 (auth_activity C-style casts) is confirmed but is explicitly out of DEBT-03 scope per ROADMAP SC-2, RESEARCH Pitfall 4, and Plan 04-03. It is a known gap for a follow-up, not a Phase 4 blocker.

The previous VERIFICATION.md `gaps_found` status (CONC-03 incomplete) is **closed**: plan 04-06 wired the cancelled flag into all five remaining activity-owned transfer sites (themes, browse GETs). Codebase evidence confirms the wiring.

---

_Verified: 2026-06-05T20:00:00Z_
_Verifier: Claude (gsd-verifier)_
