---
phase: 03-c-platform-hardening
verified: 2026-06-05T17:00:00Z
status: human_needed
score: 4/5
overrides_applied: 0
human_verification:
  - test: "Force thomaz::tls_insecure_flag()=true at startup (e.g. add the line in main.cpp or tls_banner.cpp before the Application loop), build with cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_desktop && cmake --build build_desktop, run the desktop binary, and navigate to all 13 screens (Home, Game List, Cheats, Mods, Settings, Save Manager, System, Themes, Mod Detail, Cheat Detail, Theme Detail, Clear Cheats, Mod Manager)."
    expected: "A high-contrast red warning Label appears in the AppletFrame header on every screen while the flag is true. No banner appears on any screen when the flag is false (normal desktop run)."
    why_human: "The tls_insecure latch is never set on desktop (always verifies CA bundle) so the banner cannot trigger naturally on the host. Visual rendering of a live brls::Label requires running the app. This was deferred by explicit user decision in the 03-03 checkpoint."
---

# Phase 03: C++ Platform Hardening — Verification Report

**Phase Goal:** Duplicated filesystem helpers are consolidated into a shared `fs_util` platform utility, the TLS fail-safe shows a visible on-screen warning, `cloudBusy` is `std::atomic<bool>`, and all three are verified by a clean desktop build and host tests.
**Verified:** 2026-06-05T17:00:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `ensure_parent_dirs` and `copy_tree` defined in exactly one place (`fs_util.cpp`); all duplicated copies removed | VERIFIED | `grep -rn 'void ensure_parent_dirs\|bool copy_tree' source/ \| grep -v fs_util` returns empty. All 7 call-sites include `platform/fs_util.hpp`. `mod_store.hpp` no longer declares `copy_tree` (grep returns 0). |
| 2 | `cloudBusy` is `std::atomic<bool>{false}` in header; all read/write sites use `.load()`/`.store()` | VERIFIED | `save_detail_activity.hpp:54` declares `std::atomic<bool> cloudBusy{false}` with CONC-01 threading contract comment. 10 access sites in `.cpp`: 2 `.load()` guards + 8 `.store()` calls. No bare assignment. No `compare_exchange`. `alive` member untouched (S2 boundary intact). |
| 3 (code wiring) | When `ca_ok == false`, `tls_insecure_flag()` is set to true; `install_tls_warning_banner` is called in all 13 activities and injects a red Label gated on `tls_is_insecure()`; no `CURLOPT_SSL_VERIFYPEER` outside the `#ifdef __SWITCH__` structural block | VERIFIED | `curl_tls.hpp` sets `tls_insecure_flag()=true` only in the `ca_ok==false` branch inside `#ifdef __SWITCH__`. All 13 activity `.cpp` files call `install_tls_warning_banner(this)` (banner count == username count == 13). `tls_banner.cpp` gates on `tls_is_insecure()`, uses `_i18n` key, injects red Label at `hint_box[0]`. All 3 `CURLOPT_SSL_VERIFYPEER` uses are inside the `#ifdef __SWITCH__`/`#else`/`#endif` region (lines 32–60 of `curl_tls.hpp`). |
| 3 (visual rendering) | Red warning Label actually renders on every screen when the flag is forced; no banner when flag is false | UNCERTAIN — needs human | Cannot verify visual rendering without running the app with a forced flag. Deferred by explicit user decision. |
| 4 | Host doctest covering `ca_ok == false` / `tls_policy(false)` passes in the test suite (TEST-03) | VERIFIED | `tests/test_tls_policy.cpp` has 2 TEST_CASEs: `tls_policy(false)` == `{0,0}` and `tls_policy(true)` == `{1,2}`. File includes only the curl-free `platform/tls_policy.hpp` — no curl dependency. Orchestrator confirms 177 tests / 533 assertions, 0 failures, exit 0. |
| 5 | Desktop build with `-DUSE_SDL2=ON` compiles clean — zero errors, zero new warnings | VERIFIED | Orchestrator confirms cmake + build exit 0, zero new warnings, `thomaz` executable linked. Commit record shows checkpoint Task 3 APPROVED in both 03-01 and 03-03 plans. |

**Score:** 4/5 truths fully verified (Truth 3 split: code-wiring VERIFIED, visual-rendering UNCERTAIN)

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `source/platform/fs_util.hpp` | `thomaz::ensure_parent_dirs` and `thomaz::copy_tree` declarations | VERIFIED | Exists; contains both declarations under `namespace thomaz`; 14 lines |
| `source/platform/fs_util.cpp` | Single definition of both helpers | VERIFIED | 92 lines; defines `thomaz::ensure_parent_dirs` (substring-at-slash canonical) and `thomaz::copy_tree` (3-arg with `is_dir`/`copy_file` in anonymous namespace); no C++20-only constructs |
| `tests/test_fs_util.cpp` | D-05 equivalence gate doctest | VERIFIED | 102 lines; 3 TEST_CASEs: `ensure_parent_dirs` basic, trailing-slash, and D-05 equivalence vs char-by-char oracle |
| `source/platform/tls_policy.hpp` | Pure curl-free `tls_policy(bool)` seam | VERIFIED | 25 lines; `TlsPolicy` struct + `inline tls_policy(bool)` in `namespace thomaz`; no curl includes, no `__SWITCH__` guards (grep count: 0) |
| `tests/test_tls_policy.cpp` | TEST-03 fail-safe branch doctest | VERIFIED | 28 lines; 2 TEST_CASEs; includes only `platform/tls_policy.hpp`; no curl headers in test TU |
| `source/app/tls_banner.hpp` | `install_tls_warning_banner(brls::Activity*)` declaration | VERIFIED | Declares `void thomaz::install_tls_warning_banner(brls::Activity* activity)` |
| `source/app/tls_banner.cpp` | Banner injection gated on `tls_is_insecure()` | VERIFIED | 32 lines; includes `platform/curl_tls.hpp`; gates on `tls_is_insecure()`; uses `_i18n` key `thomaz/tls/insecure_warning`; injects red `brls::Label` at `hint_box` index 0 |
| `source/app/save_detail_activity.hpp` | `std::atomic<bool> cloudBusy` with threading contract | VERIFIED | `cloudBusy` declared as `std::atomic<bool> cloudBusy{false}` at line 54; `<atomic>` included; CONC-01 contract comment present; `alive` member untouched |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `source/platform/cheat_store.cpp` | `thomaz::ensure_parent_dirs` | `#include "platform/fs_util.hpp"` | WIRED | File present in `grep -rl fs_util.hpp` output |
| `tests/Makefile` | `source/platform/fs_util.cpp` | Explicit `SRCS` entry | WIRED | `../source/platform/fs_util.cpp` appears in SRCS on line 3 |
| `source/platform/curl_tls.hpp` | `thomaz::tls_policy` | `#include "platform/tls_policy.hpp"` | WIRED | `curl_tls.hpp:4` includes `tls_policy.hpp`; both Switch and desktop branches call `tls_policy()` |
| `tests/test_tls_policy.cpp` | `thomaz::tls_policy` | `#include "platform/tls_policy.hpp"` (curl-free) | WIRED | `test_tls_policy.cpp:2` includes only `tls_policy.hpp`; calls `tls_policy(false)` and `tls_policy(true)` |
| `source/app/tls_banner.cpp` | `thomaz::tls_is_insecure` | `#include "platform/curl_tls.hpp"` | WIRED | `tls_banner.cpp:4` includes `curl_tls.hpp`; calls `thomaz::tls_is_insecure()` at line 15 |
| `source/app/home_activity.cpp` | `install_tls_warning_banner` | call in `onContentAvailable` | WIRED | Confirmed in banner-count check; all 13 activity files include the banner call |
| `source/app/save_detail_activity.cpp` | `cloudBusy.load / cloudBusy.store` | atomic accessors at all guard/set sites | WIRED | 10 sites verified: 2 `.load()` + 8 `.store()`; no bare assignment |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|--------------|--------|-------------------|--------|
| `tls_banner.cpp` | `tls_is_insecure()` latch | `curl_tls.hpp` static bool set in `ca_ok==false` branch | Yes — process-global one-way latch set from real CA probe | FLOWING |
| `save_detail_activity.cpp` | `cloudBusy` | `std::atomic<bool>` member; set by `store(true)` before async ops, `store(false)` in callbacks | Yes — state flows through real guard logic | FLOWING |
| `tls_banner.cpp` | `_i18n` label text | `resources/i18n/en-US/thomaz.json` `tls.insecure_warning` key | Yes — key present in all 5 locales (verified) | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| `tls_policy(false)` returns `{0,0}` | Orchestrator: `make -C tests test` — 177 tests / 533 assertions / 0 failures | exit 0 | PASS |
| `ensure_parent_dirs` D-05 equivalence | Orchestrator: `make -C tests test` — includes `test_fs_util.cpp` 3 TEST_CASEs | exit 0 | PASS |
| Desktop build clean | Orchestrator: cmake `-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` + full build | exit 0, zero new warnings | PASS |
| Red banner renders on all screens when flag forced | Requires running app with patched startup | Cannot verify without running binary | SKIP — see human verification |

### Probe Execution

No conventional `scripts/*/tests/probe-*.sh` probes exist for this phase. Build and test verification was performed by the orchestrator (desktop build exit 0; host test suite 177/0). No probe scripts to run.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| DEBT-01 | 03-01 | `ensure_parent_dirs` in exactly one shared helper; duplicates removed | SATISFIED | `fs_util.hpp/cpp` define the single canonical form; `grep -rn 'void ensure_parent_dirs' source/ \| grep -v fs_util` returns empty |
| DEBT-02 | 03-01 | `copy_tree` in exactly one shared platform utility; duplicate removed | SATISFIED | Same `fs_util.hpp/cpp`; `grep -rn 'bool copy_tree' source/ \| grep -v fs_util` returns empty; `mod_store.hpp` copy_tree count: 0 |
| TEST-03 | 03-02 | Host test covers TLS fail-safe branch (`ca_ok == false`) | SATISFIED | `test_tls_policy.cpp` has 2 TEST_CASEs; both pass in the orchestrator-confirmed 177-test suite |
| SEC-03 | 03-03 | Persistent on-screen warning when CA bundle probe fails; fail-safe networking unchanged | SATISFIED (code) / UNCERTAIN (visual) | Banner helper created and wired into all 13 activities; code path to inject Label is correct; visual rendering of the Label requires human verification (forced-flag smoke test deferred) |
| CONC-01 | 03-04 | `cloudBusy` is `std::atomic<bool>` with documented threading contract; guard behavior unchanged | SATISFIED | Header changed to `std::atomic<bool> cloudBusy{false}`; 10 `.load()`/`.store()` sites; no bare access; no `compare_exchange`; `alive` untouched |

**Orphaned requirements check:** REQUIREMENTS.md maps SEC-03, CONC-01, DEBT-01, DEBT-02, TEST-03 to Phase 3. All 5 are claimed by the 4 plans. No orphaned requirements.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | — | — | No TBD/FIXME/XXX/HACK/PLACEHOLDER found in any phase-modified file |

No debt markers. No stub returns. No hardcoded-empty data flowing to render paths. No C++20-only constructs in `fs_util.cpp/.hpp`.

### Human Verification Required

#### 1. TLS Warning Banner Visual Rendering (Forced-Flag Smoke Test)

**Test:** Force `thomaz::tls_insecure_flag()=true` at application startup (e.g. add `thomaz::tls_insecure_flag() = true;` as the first line after `Application::init()` in `main.cpp`, or temporarily edit `tls_banner.cpp` to remove the `tls_is_insecure()` guard). Then:
1. Build: `cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_smoke && cmake --build build_smoke`
2. Run `./build_smoke/thomaz` and navigate to all 13 screens that call `install_tls_warning_banner(this)`: Home, Game List, Cheats, Mods, Settings, Save Manager, System, Themes, Mod Detail, Cheat Detail, Theme Detail, Clear Cheats, Mod Manager.
3. Revert the forced flag and rebuild; run again to confirm no banner appears.

**Expected:** A high-contrast red (`nvgRGB(0xFF,0x55,0x55)`) warning Label from the `thomaz/tls/insecure_warning` i18n key renders in the AppletFrame header on every screen when the flag is forced. No banner appears on any screen when the flag is false.

**Why human:** The `tls_insecure` latch is never set on the desktop build (the CA probe only runs on Switch, and the desktop `#else` branch calls `tls_policy(true)` without setting the latch). Visual label rendering inside the borealis UI framework requires running the binary. Forced-flag visual confirmation was explicitly deferred from the 03-03 checkpoint to phase UAT by user decision.

### Gaps Summary

No blocking gaps found. All code deliverables for DEBT-01, DEBT-02, TEST-03, CONC-01 and the code-wiring portion of SEC-03 are fully verified in the codebase. The single outstanding item is the visual forced-flag smoke test for the TLS banner (SEC-03), which is an intentionally deferred human-only verification step.

The `status: human_needed` reflects that one human verification item (banner visual rendering) remains, per the Step 9 decision tree: human items prevent `passed` even when all automated checks pass.

---

_Verified: 2026-06-05T17:00:00Z_
_Verifier: Claude (gsd-verifier)_
