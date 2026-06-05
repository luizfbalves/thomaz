---
phase: 03-c-platform-hardening
verified: 2026-06-05T18:00:00Z
status: human_needed
score: 5/5
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 4/5
  gaps_closed:
    - "SC-3 (fail-closed): tls_policy(false) now returns {1,2}; test_tls_policy.cpp asserts fail-closed default and the InsecureAllowed opt-in; no caller uses InsecureAllowed — confirmed VERIFIED"
    - "SC-4 (TEST-03): test rewritten post CR-01 to assert tls_policy(false)=={1,2} (fail-closed) AND tls_policy(false,InsecureAllowed)=={0,0} (opt-in); 4 TEST_CASEs, 179 total tests pass"
  gaps_remaining: []
  regressions: []
human_verification:
  - test: "Force the TLS-insecure latch at application startup (e.g. add `thomaz::tls_insecure_flag().store(true);` immediately after Application::init() in main.cpp, or remove the `if (!thomaz::tls_is_insecure()) return;` guard in tls_banner.cpp temporarily). Build: `cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_smoke && cmake --build build_smoke`. Run `./build_smoke/thomaz` and navigate to all 13 activity screens: Home, Game List, Cheats, Mods, Settings, Save Manager, System, Themes, Mod Detail, Cheat Detail, Theme Detail, Clear Cheats, Mod Manager."
    expected: "A high-contrast red (nvgRGB 0xFF,0x55,0x55) warning Label from the `thomaz/tls/insecure_warning` i18n key renders in the AppletFrame header on every screen while the latch is forced. No banner appears on any screen when the latch is false (normal run)."
    why_human: "Under the revised fail-closed design, the tls_insecure latch is NEVER set automatically — not on desktop (CA probe does not run), and not on Switch when the CA bundle is absent (verification stays ON). The banner is now latent: reachable only if a future caller passes TlsMode::InsecureAllowed and sets the latch manually. Visual rendering of a brls::Label requires running the binary with a forced latch. This item is non-gating for the phase goal (see assessment below) but retained as an optional Switch-hardware integration check."
    gating: false
  - test: "Build the Switch NRO target with devkitPro aarch64 toolchain and confirm save_service_switch.cpp compiles cleanly. The uid_from_hex refactor (IN-03) uses SCNx64/PRIx64 inside #ifdef __SWITCH__ — this code is excluded from the host build."
    expected: "Zero compiler errors or warnings from save_service_switch.cpp in the Switch build. uid_from_hex parses AccountUid correctly (hex round-trip: format then parse returns the original uid)."
    why_human: "save_service_switch.cpp is gated by #ifdef __SWITCH__ and never compiled by the host g++ build. The IN-03 refactor is behavior-preserving but cannot be exercised without a devkitPro toolchain."
    gating: false
---

# Phase 03: C++ Platform Hardening — Re-Verification Report (Post-CR-01 Reversal)

**Phase Goal:** Duplicated filesystem helpers consolidated into a shared `fs_util` platform utility; TLS CA-missing handling is fail-closed (verification stays ON); `cloudBusy` is `std::atomic<bool>`; verified by a clean desktop build and host tests.
**Verified:** 2026-06-05T18:00:00Z
**Status:** human_needed
**Re-verification:** Yes — after CR-01 fail-open → fail-closed reversal (commits 3829744..3fa978f, user-authorized)

## Design Change Summary (D-06a)

After initial verification, a `/gsd-code-review 3 --fix --all` pass applied **CR-01** (user-authorized): `tls_policy(false)` now returns `{verifypeer:1, verifyhost:2}` (fail-closed) instead of `{0:0}` (fail-open). The `TlsMode::InsecureAllowed` enum gates the legacy insecure path behind an explicit per-caller opt-in that no caller currently uses. `apply_curl_tls` no longer sets `tls_insecure_flag()` on the automatic CA-missing path. `test_tls_policy.cpp` was rewritten to assert the new contract (4 TEST_CASEs, 179 total host tests pass). The banner (`tls_banner.cpp`) and its 13-activity wiring remain in place and compile, but are now latent — they only activate if a future `InsecureAllowed` caller sets the latch. The design record is updated in `03-CONTEXT.md D-06a`.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `fs_util.hpp`/`fs_util.cpp` exist; `ensure_parent_dirs` and `copy_tree` defined there only; all 7 duplicate definitions removed; `is_dir` uses `lstat` + symlinks skipped (WR-06) | VERIFIED | `grep -rn 'void ensure_parent_dirs\|bool copy_tree' source/ \| grep -v fs_util` returns empty. All 7 call-sites include `platform/fs_util.hpp`. `mod_store.hpp` copy_tree count: 0. `fs_util.cpp` uses `::lstat` (not `::stat`) in `is_dir`; `is_symlink` helper added; `copy_tree` skips any symlink entry. |
| 2 | `save_detail_activity.hpp` declares `cloudBusy` as `std::atomic<bool>{false}`; all read/write sites use `.load()`/`.store()`; no CAS | VERIFIED | Line 54 declares `std::atomic<bool> cloudBusy{false}` with CONC-01 threading contract comment. 10 access sites in `.cpp`: 2 `.load()` guards + 8 `.store()` calls. `grep -nE 'cloudBusy *= *(true\|false)'` returns empty (no bare assignment). `compare_exchange` count: 0. `alive` member untouched (S2 boundary intact). |
| 3 | [REVISED — fail-closed] `tls_policy(false)` returns `{verifypeer:1, verifyhost:2}` by default; `TlsMode::InsecureAllowed` opt-in returns `{0,0}`; no caller uses `InsecureAllowed`; `apply_curl_tls` keeps verification ON when CA is absent; `tls_insecure_flag()` not set on the automatic path; `install_tls_warning_banner` + 14-activity wiring compile and are present (latent) | VERIFIED | `tls_policy.hpp:31-34` — `tls_policy(bool, TlsMode=Verify)` returns `{1L,2L}` unless `InsecureAllowed` passed. `curl_tls.hpp` else-branch (ca_ok==false): calls `tls_policy(false)` (default Verify) → `{1,2}`; does NOT call `tls_insecure_flag().store(true)`. `grep -rn 'InsecureAllowed' source/` shows only declaration sites (tls_policy.hpp + curl_tls.hpp comment) — zero call-site uses. Banner + 13-activity wiring exist and compile (desktop build exit 0). All 3 `CURLOPT_SSL_VERIFYPEER` uses in `curl_tls.hpp` are inside the `#ifdef __SWITCH__` / `#else` / `#endif` structural block. |
| 4 | TEST-03 host doctest asserts BOTH `tls_policy(false) == {1,2}` (fail-closed default) AND `tls_policy(false, InsecureAllowed) == {0,0}` (opt-in); passes in host suite | VERIFIED | `tests/test_tls_policy.cpp` has 4 TEST_CASEs: (a) `tls_policy(false)` → `{1,2}`, (b) `tls_policy(false, TlsMode::Verify)` → `{1,2}`, (c) `tls_policy(false, TlsMode::InsecureAllowed)` → `{0,0}`, (d) `tls_policy(true)` → `{1,2}`. File includes only curl-free `platform/tls_policy.hpp`. Orchestrator confirms 179 tests / 537 assertions, 0 failures, exit 0. |
| 5 | Desktop build with `-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` compiles clean — zero errors, zero new warnings | VERIFIED | Orchestrator confirms cmake + full build exit 0, zero new warnings, `thomaz` executable linked (run after all 11 fix commits 3829744..3fa978f). |

**Score:** 5/5 truths verified

### Banner Latency Assessment (SC-3 Human Verification Decision)

Under the fail-closed design, `SEC-03`'s intent — "never silently operate insecure" — is now met by **refusing the insecure transfer** rather than by warning-and-continuing. When the CA bundle is absent, curl receives `verifypeer=1`/`verifyhost=2` and the HTTPS connection fails with a certificate error. No silent downgrade occurs. The banner's original purpose (warn the user that insecure transfers are happening) no longer applies to the automatic path.

**The forced-flag visual smoke test is therefore NON-GATING for this phase.** The phase goal is achieved without it: the code compiles, the latch infrastructure exists, and the banner renders correctly if the latch is ever set by a future `InsecureAllowed` caller. The visual smoke test is retained in the human-verification list as an optional integration check for completeness, but it does not block the phase from proceeding.

**The Switch-build IN-03 item** (`uid_from_hex` / PRIx64/SCNx64 refactor in `save_service_switch.cpp`) is also NON-GATING: the change is behavior-preserving, lives entirely inside `#ifdef __SWITCH__`, and the host suite remains green. It is flagged for Switch-hardware verification when a devkitPro build is next available.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `source/platform/fs_util.hpp` | `thomaz::ensure_parent_dirs` and `thomaz::copy_tree` declarations; symlink-skip contract documented | VERIFIED | 18 lines; both declarations under `namespace thomaz`; doc comment on `copy_tree` notes symlink-skip and lstat behavior (WR-06) |
| `source/platform/fs_util.cpp` | Single definition of both helpers; `is_dir` uses `lstat`; `is_symlink` added; symlinks skipped in `copy_tree` | VERIFIED | 107 lines; `::lstat` in `is_dir` (line 19); `is_symlink` helper (lines 22-25); `copy_tree` skips symlinks (line 88-89); no C++20-only constructs |
| `tests/test_fs_util.cpp` | D-05 equivalence gate doctest | VERIFIED | 3 TEST_CASEs: basic, trailing-slash, D-05 oracle equivalence |
| `source/platform/tls_policy.hpp` | Pure curl-free seam; `TlsMode` enum; `tls_policy(bool, TlsMode=Verify)` fail-closed default | VERIFIED | 37 lines; `TlsMode { Verify, InsecureAllowed }` enum; `tls_policy` returns `{1,2}` for missing CA unless `InsecureAllowed`; no `#ifdef __SWITCH__` guards (count: 0); "curl" appears only in prose comments (not as an include) |
| `tests/test_tls_policy.cpp` | TEST-03 doctest — 4 TEST_CASEs asserting fail-closed default and opt-in | VERIFIED | 51 lines; 4 TEST_CASEs; includes only `platform/tls_policy.hpp`; no curl headers; `using thomaz::TlsMode` |
| `source/app/tls_banner.hpp` | `install_tls_warning_banner(brls::Activity*)` declaration | VERIFIED | 12 lines; declares `void thomaz::install_tls_warning_banner(brls::Activity* activity)` |
| `source/app/tls_banner.cpp` | Banner injection gated on `tls_is_insecure()`; hint_box fallback to header; Logger::warning if neither slot found | VERIFIED | 37 lines; gates on `tls_is_insecure()`; falls back from `hint_box` to `header`; `Logger::warning` if no slot (WR-01 fix applied); uses `_i18n` key; red Label at index 0 |
| `source/app/save_detail_activity.hpp` | `std::atomic<bool> cloudBusy{false}` with CONC-01 threading contract | VERIFIED | Line 54: `std::atomic<bool> cloudBusy{false}`; `<atomic>` included at line 5; 7-line threading contract comment present; `alive` member at line 60 unchanged |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `source/platform/cheat_store.cpp` | `thomaz::ensure_parent_dirs` | `#include "platform/fs_util.hpp"` | WIRED | Confirmed in 7-file grep scan |
| `source/platform/save_service_switch.cpp` | `thomaz::copy_tree` | `#include "platform/fs_util.hpp"` | WIRED | Confirmed in 7-file grep scan |
| `tests/Makefile` | `source/platform/fs_util.cpp` | Explicit `SRCS` entry | WIRED | `../source/platform/fs_util.cpp` appears at end of SRCS line |
| `source/platform/curl_tls.hpp` | `thomaz::tls_policy` (fail-closed) | `#include "platform/tls_policy.hpp"`; both branches call `tls_policy()` | WIRED | Line 5 includes `tls_policy.hpp`; ca_ok==true path: `tls_policy(true)`; ca_ok==false path: `tls_policy(false)` (default Verify → `{1,2}`) |
| `source/platform/curl_tls.hpp` | `tls_insecure_flag` NOT set on automatic path | absence of assignment in ca_ok==false branch | VERIFIED (absence) | `grep -n 'tls_insecure_flag.*=\|= true' curl_tls.hpp` returns empty — latch is defined but never set automatically |
| `tests/test_tls_policy.cpp` | `thomaz::tls_policy` + `TlsMode` | `#include "platform/tls_policy.hpp"` (curl-free) | WIRED | Line 2 includes only `tls_policy.hpp`; 4 TEST_CASEs exercise both default and InsecureAllowed paths |
| `source/app/tls_banner.cpp` | `thomaz::tls_is_insecure` | `#include "platform/curl_tls.hpp"` | WIRED | Line 4 includes `curl_tls.hpp`; line 15 calls `thomaz::tls_is_insecure()` |
| `source/app/home_activity.cpp` (representative) | `install_tls_warning_banner` | call in `onContentAvailable` | WIRED | 13 activity files confirmed; banner-call count == username-call count == 13 |
| `source/app/save_detail_activity.cpp` | `cloudBusy.load()` / `cloudBusy.store()` | atomic accessors at all 10 guard/set sites | WIRED | 2 `.load()` + 8 `.store()` confirmed; no bare `cloudBusy = true/false` assignment |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|--------------|--------|-------------------|--------|
| `tls_banner.cpp` | `tls_is_insecure()` latch | `curl_tls.hpp` `tls_insecure_flag()` — latent under fail-closed; only set if `InsecureAllowed` caller exists | Latent — latch is always false on automatic path; banner is non-rendering in normal operation | LATENT (intentional by design — fail-closed makes the banner dormant on the normal path; banner still compiles and renders if latch is forced) |
| `save_detail_activity.cpp` | `cloudBusy` | `std::atomic<bool>` member; `store(true)` before async ops, `store(false)` in callbacks | Yes — state flows through real guard logic | FLOWING |
| `tls_banner.cpp` | i18n label text | `resources/i18n/*/thomaz.json` `tls.insecure_warning` key | Yes — key present in all 5 locales | FLOWING (when latch is set) |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| `tls_policy(false)` returns `{1,2}` (fail-closed) | Orchestrator: `make -C tests clean && make -C tests test` — 179 tests / 537 assertions / 0 failures | exit 0 | PASS |
| `tls_policy(false, InsecureAllowed)` returns `{0,0}` (opt-in gate) | Same test run — TEST_CASE "opt-in insecure" CHECKs `verifypeer==0L` and `verifyhost==0L` | exit 0 | PASS |
| `ensure_parent_dirs` D-05 equivalence | Same test run — includes `test_fs_util.cpp` 3 TEST_CASEs | exit 0 | PASS |
| Desktop build clean post CR-01 commits | Orchestrator: cmake `-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` + full build after commits 3829744..3fa978f | exit 0, zero new warnings | PASS |
| Banner renders when latch forced | Requires running app with patched startup | Cannot verify without running binary | SKIP — non-gating; see human verification |
| IN-03 uid_from_hex Switch-build behavior | Requires devkitPro aarch64 toolchain | Cannot build without Switch toolchain | SKIP — non-gating; see human verification |

### Probe Execution

No conventional `scripts/*/tests/probe-*.sh` probes exist for this phase. Build and test verification was performed by the orchestrator (desktop build exit 0 after all 11 fix commits; host test suite 179/0).

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| DEBT-01 | 03-01 | `ensure_parent_dirs` in exactly one shared helper; all duplicates removed | SATISFIED | `grep -rn 'void ensure_parent_dirs' source/ \| grep -v fs_util` returns empty; 4 former sites include `platform/fs_util.hpp` |
| DEBT-02 | 03-01 | `copy_tree` in exactly one shared utility; all duplicates removed | SATISFIED | `grep -rn 'bool copy_tree' source/ \| grep -v fs_util` returns empty; `mod_store.hpp` copy_tree count: 0; 3 former sites use the shared helper |
| TEST-03 | 03-02 + CR-01 | Host test covers TLS fail-safe branch and the new fail-closed contract | SATISFIED | 4 TEST_CASEs in `test_tls_policy.cpp`; asserts `tls_policy(false)=={1,2}` (fail-closed default) and `tls_policy(false,InsecureAllowed)=={0,0}` (opt-in); 179 tests pass |
| SEC-03 | 03-03 + D-06a | CA-missing handling is SAFE — no silent insecure transfer; persistent warning infrastructure present (latent) | SATISFIED | Under fail-closed: `apply_curl_tls` keeps `verifypeer=1`/`verifyhost=2` on CA-absent path → transfer fails loudly rather than downgrading silently. SEC-03's intent ("don't silently operate insecure") is met by refusing the transfer. Banner + 13-activity wiring compile and are latent-correct. Visual smoke test is non-gating (see assessment above). |
| CONC-01 | 03-04 | `cloudBusy` is `std::atomic<bool>` with documented threading contract; guard behavior unchanged | SATISFIED | `save_detail_activity.hpp:54` declares `std::atomic<bool> cloudBusy{false}`; 10 `.load()`/`.store()` sites; no bare access; no `compare_exchange`; `alive` untouched (S2 boundary) |

**Orphaned requirements check:** REQUIREMENTS.md maps SEC-03, CONC-01, DEBT-01, DEBT-02, TEST-03 to Phase 3. All 5 are claimed by the 4 plans and verified above. No orphaned requirements.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | — | — | No TBD/FIXME/XXX/HACK/PLACEHOLDER in any phase-modified file |

No debt markers found. No stub returns flowing to render paths. No hardcoded-empty data. No C++20-only constructs in `fs_util.cpp/.hpp`. `tls_policy.hpp` mentions "curl" only in prose comments — no `#include <curl/curl.h>` (grep for `#include.*curl` returns 0).

### Human Verification Required

#### 1. TLS Warning Banner Visual Rendering (Forced-Flag Smoke Test) — NON-GATING

**Test:** Force `thomaz::tls_insecure_flag().store(true)` at application startup (add immediately after `Application::init()` in `main.cpp`, or temporarily remove the `if (!thomaz::tls_is_insecure()) return;` guard in `tls_banner.cpp`). Build: `cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_smoke && cmake --build build_smoke`. Run `./build_smoke/thomaz` and navigate to all 13 screens: Home, Game List, Cheats, Mods, Settings, Save Manager, System, Themes, Mod Detail, Cheat Detail, Theme Detail, Clear Cheats, Mod Manager.

**Expected:** A high-contrast red (`nvgRGB(0xFF,0x55,0x55)`) warning Label from the `thomaz/tls/insecure_warning` i18n key renders in the AppletFrame header (via `hint_box` or `header` fallback slot) on every screen when the flag is forced. No banner appears on any screen when the flag is false.

**Why human:** Under fail-closed, the `tls_insecure` latch is never set automatically on any path. The banner is latent. Visual rendering of a `brls::Label` requires running the binary with a forced latch.

**Why non-gating:** SEC-03's intent ("never silently operate insecure") is now met by the fail-closed design — the transfer is refused, not downgraded. The banner exists as a latent, ready-if-needed mechanism for any future `InsecureAllowed` caller. Its correct rendering under a forced flag is an integration smoke test, not a phase-goal gate.

#### 2. Switch-Build Verification for IN-03 (uid_from_hex Refactor) — NON-GATING

**Test:** Build the Switch NRO target with a devkitPro aarch64 toolchain (standard CI path). Confirm `save_service_switch.cpp` compiles without warnings or errors. Optionally run the save-slot flow on hardware and confirm uid round-trip (format then parse) is correct.

**Expected:** Zero warnings from `save_service_switch.cpp`. No behavioral regression in save-slot list, backup, or restore flows.

**Why human:** `save_service_switch.cpp` is entirely inside `#ifdef __SWITCH__` and not compiled by the host `g++` build. The IN-03 refactor (`PRIx64`/`SCNx64` + `uid_from_hex` helper) is behavior-preserving but cannot be exercised without the devkitPro toolchain.

**Why non-gating:** The change is a strict type-safety improvement (from `(unsigned long*)` aliasing-cast to proper `std::uint64_t` with `SCNx64`). The pre-existing behavior worked on the same aarch64 LP64 target. The host suite has no regression. Flagged for the next hardware build cycle.

### Gaps Summary

No blocking gaps. All 5 success criteria (revised) are fully satisfied in the codebase:

1. `fs_util.hpp/cpp` — single canonical definitions, lstat + symlink-skip — VERIFIED
2. `cloudBusy` atomic with load/store discipline — VERIFIED
3. Fail-closed TLS: `tls_policy(false)` returns `{1,2}`; `InsecureAllowed` opt-in gates `{0,0}`; no caller uses it; banner + 14-activity wiring compile and are latent-correct — VERIFIED
4. TEST-03 with 4 TEST_CASEs asserting both the fail-closed default and the InsecureAllowed opt-in — VERIFIED
5. Desktop build clean post all fix commits — VERIFIED

Two optional human checks remain (banner visual smoke test, Switch toolchain IN-03 build), both explicitly non-gating. Status is `human_needed` per decision-tree rules (human items present), but neither item blocks phase goal achievement or progression to Phase 4.

---

_Verified: 2026-06-05T18:00:00Z_
_Verifier: Claude (gsd-verifier)_
_Re-verification: Yes — after CR-01 fail-open → fail-closed reversal and 11 fix commits (3829744..3fa978f)_
