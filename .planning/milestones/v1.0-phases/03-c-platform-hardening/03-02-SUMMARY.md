---
phase: 03-c-platform-hardening
plan: "02"
subsystem: testing
tags: [tls, curl, doctest, security, c++, host-test, seam]

requires:
  - phase: 03-c-platform-hardening/03-01
    provides: fs_util consolidation (pattern for shared platform headers)

provides:
  - "thomaz::TlsPolicy struct and tls_policy(bool) pure inline function in source/platform/tls_policy.hpp (D-06 seam)"
  - "thomaz::tls_insecure_flag() / tls_is_insecure() process-global latch in curl_tls.hpp (outside #ifdef __SWITCH__)"
  - "apply_curl_tls() refactored to route both Switch and desktop branches through tls_policy()"
  - "TEST-03 host doctest in tests/test_tls_policy.cpp covering fail-safe {0,0} and secure {1,2} branches"

affects:
  - "03-c-platform-hardening/03-03 (SEC-03 banner reads tls_is_insecure())"

tech-stack:
  added: []
  patterns:
    - "D-06 seam: pure curl-free host-compilable header for platform decisions; no #ifdef __SWITCH__, no curl include"
    - "Process-global one-way latch via inline bool& returning static local — safe for ODR, readable from any TU"

key-files:
  created:
    - source/platform/tls_policy.hpp
    - tests/test_tls_policy.cpp
  modified:
    - source/platform/curl_tls.hpp

key-decisions:
  - "tls_policy.hpp is completely curl-free and Switch-guard-free — comments must not contain 'curl/curl.h' or '__SWITCH__' strings to pass acceptance grep checks"
  - "tls_insecure_flag() placed outside #ifdef __SWITCH__ in curl_tls.hpp so desktop/host builds can read it (always false on desktop)"
  - "awk verify scan reports the #else branch as 'OUTSIDE' (s resets on #else); this is a known false positive — all CURLOPT_SSL_VERIFYPEER uses are inside the #ifdef/__else__/#endif structural block per manual read"

patterns-established:
  - "Pure seam pattern: extract decision logic to a curl-free/platform-free header so host doctests can exercise it without Switch deps (mirrors cfw_paths.hpp purity model)"
  - "One-way insecure latch: inline bool& with static local, set once in fail-safe path, never cleared — matches once-per-process static ca_ok probe cadence"

requirements-completed: [TEST-03]

duration: 10min
completed: 2026-06-05
---

# Phase 03 Plan 02: TLS Policy Seam + TEST-03 Summary

**Pure curl-free tls_policy(bool) seam extracted to host-compilable header; apply_curl_tls refactored to route both branches through it; process-global insecure latch added; TEST-03 doctest covers fail-safe {0,0} and secure {1,2} branches on host**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-06-05T15:23:00Z
- **Completed:** 2026-06-05T15:33:54Z
- **Tasks:** 2
- **Files modified:** 3 (1 modified, 2 created)

## Accomplishments

- Extracted TLS verification decision into `source/platform/tls_policy.hpp`: pure, curl-free, Switch-guard-free inline function `tls_policy(bool ca_present) -> TlsPolicy{verifypeer, verifyhost}` — the D-06 seam
- Refactored `apply_curl_tls()` in `curl_tls.hpp` to consume `tls_policy()` on both the Switch CA-ok, Switch fail-safe, and desktop branches; added `tls_insecure_flag()`/`tls_is_insecure()` latch outside `#ifdef __SWITCH__` for Plan 03's SEC-03 banner
- Added `tests/test_tls_policy.cpp` (TEST-03): two doctests asserting `tls_policy(false)=={0,0}` and `tls_policy(true)=={1,2}`; `make test` green at 177 passed / 0 failed

## Task Commits

Each task was committed atomically:

1. **Task 1: Extract tls_policy seam, add insecure latch, refactor apply_curl_tls** - `ab81a85` (feat)
2. **Task 2: TEST-03 host doctest for the ca_present==false fail-safe branch** - `5b41567` (test)

## Files Created/Modified

- `source/platform/tls_policy.hpp` (created) — `thomaz::TlsPolicy` struct + `inline tls_policy(bool)` seam; no curl includes, no `#ifdef __SWITCH__`
- `source/platform/curl_tls.hpp` (modified) — includes tls_policy.hpp; adds `tls_insecure_flag()`/`tls_is_insecure()` outside `#ifdef`; routes both Switch branches and desktop branch through `tls_policy()`; sets insecure latch in ca_ok==false path only
- `tests/test_tls_policy.cpp` (created) — TEST-03 doctest; includes only the curl-free seam header; 2 TEST_CASEs

## Decisions Made

- Comment text in `tls_policy.hpp` and `test_tls_policy.cpp` must not contain literal `curl/curl.h` or `__SWITCH__` strings — the acceptance grep checks are exact string matches that trip on comments. Comments were reworded to avoid false positives.
- The awk verify scan (`/#else/{s=0}`) treats the desktop `#else` branch as "outside" the `#ifdef` block and reports a false-positive OUTSIDE line for that branch. This is a known limitation of the awk script in the plan. All `CURLOPT_SSL_VERIFYPEER` uses in `curl_tls.hpp` are structurally inside the `#ifdef __SWITCH__`/`#else`/`#endif` region — confirmed by reading the file. The must_haves criterion "No CURLOPT_SSL_VERIFYPEER literal lines remain outside #ifdef __SWITCH__" is satisfied.

## Deviations from Plan

None — plan executed exactly as written. The comment-wording adjustments to avoid grep false positives are a natural consequence of strict acceptance-criteria string matching, not a deviation from the intended behavior.

## Issues Encountered

- **Comment text vs. grep acceptance checks:** Initial comments in `tls_policy.hpp` contained the string `curl/curl.h` (in a prose description of what the file avoids) and `__SWITCH__` (same context). Similarly, `test_tls_policy.cpp` contained `curl/curl.h` in a prose comment. Both were caught immediately by the acceptance-criteria grep and fixed by rewording the comments.
- **awk verify scan false positive:** The plan's awk script sets `s=0` on `#else`, so the desktop branch's `CURLOPT_SSL_VERIFYPEER` use is reported as OUTSIDE. The plan's own acceptance note says "confirm by reading that the only verify-literals are within the #ifdef/#else region" — which is satisfied. Documented in decisions.

## Next Phase Readiness

- `tls_is_insecure()` is now callable from any TU that includes `curl_tls.hpp` — Plan 03 (SEC-03 UI banner) can read it to warn the user when the CA bundle was absent at startup
- All 177 host doctests pass; no regressions introduced

---
*Phase: 03-c-platform-hardening*
*Completed: 2026-06-05*
