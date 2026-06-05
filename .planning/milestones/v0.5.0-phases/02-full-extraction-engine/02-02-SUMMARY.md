---
phase: 02-full-extraction-engine
plan: "02"
subsystem: platform/themes
tags: [szs-validation, sarclib, tdd, host-test, neutral-tu]
dependency_graph:
  requires: []
  provides: [szs_validate_tu, host_szs_doctest]
  affects: [firmware_extract_switch.cpp, tests]
tech_stack:
  added: []
  patterns: [neutral-TU (no libnx in shared validator), TDD RED/GREEN, SarcLib Yaz0+SARC structural validation]
key_files:
  created:
    - source/platform/themes/szs_validate.hpp
    - source/platform/themes/szs_validate.cpp
    - tests/test_szs_validate.cpp
  modified:
    - tests/Makefile
decisions:
  - "Comment text in szs_validate.cpp avoided literal 'switch.h' string so the neutral-TU grep acceptance check passes cleanly (the doc comment was rephrased to 'no libnx headers')."
metrics:
  duration: "6 minutes"
  completed: "2026-06-05"
  tasks_completed: 2
  files_changed: 4
---

# Phase 02 Plan 02: szs Structural Validator (Neutral TU + Host Doctest) Summary

**One-liner:** D-04 structural validation via Yaz0+SARC in a neutral, libnx-free TU with a host doctest covering valid, Yaz0-wrapped, garbage, short, and bad-Yaz0 buffers.

## What Was Built

Phase 1 shipped `is_valid_szs` as a 4-byte magic check inside `firmware_extract_switch.cpp` (Switch-only). That check passes truncated/corrupt buffers (Pitfall 4). D-04 requires the stronger check: Yaz0-decompress when applicable, then SARC-unpack and verify non-empty. This plan extracts that check into a neutral TU usable from both the Switch app and the host doctest.

### Task 1: Neutral szs_validate TU (TDD — RED then GREEN)

Created `source/platform/themes/szs_validate.hpp` declaring `bool is_structurally_valid_szs(const std::vector<std::uint8_t>&)` in `namespace thomaz` with only `<cstdint>` and `<vector>` includes (no SarcLib in the header — Pitfall 5).

Created `source/platform/themes/szs_validate.cpp` implementing D-04:
- Returns `false` immediately for `buf.size() < 4`
- Copies buffer; calls `Yaz0::IsYaz0` + `Yaz0::Decompress` when Yaz0-compressed
- Calls `SARC::Unpack` and returns `!sd.files.empty()`
- Wraps both SarcLib calls in `try/catch(...)` returning `false` — never throws (T-02-05)
- Zero libnx headers — compiles in both Switch app and host doctest (T-02-06)

TDD flow:
- RED: stub returning `false` always — `make run && ./run --test-case="*szs*"` → 2 failures (correct)
- GREEN: real implementation — all 6 szs TEST_CASEs + 187 total host tests pass

### Task 2: Host Doctest + Makefile C++20 Upgrade

`tests/test_szs_validate.cpp` — 6 TEST_CASEs:
1. Known-good bare SARC (via `SARC::Pack`) → `true`
2. Yaz0-compressed wrap of good SARC → `true`
3. 64-byte `0xAA` garbage → `false`
4. 3-byte buffer (< 4) → `false`
5. Yaz0 magic + junk body → `false` (Yaz0::Decompress throws)
6. Empty buffer → `false`

`tests/Makefile` changes:
- `-std=c++17` → `-std=c++20` (SarcLib uses `std::span`; C++20 backward-compat with C++17 sources per CMakeLists.txt:125-127)
- Added `-I../lib/switchthemes` to CXXFLAGS
- Added to SRCS: `../source/platform/themes/szs_validate.cpp`, `../lib/switchthemes/SarcLib/Yaz0.cpp`, `../lib/switchthemes/SarcLib/Sarc.cpp`, `../lib/switchthemes/BinaryReadWrite/Buffer.cpp`

## Verification

```
$ cd tests && make clean && make run && ./run --test-case="*szs*" --success
[doctest] test cases: 10 | 10 passed | 0 failed | 177 skipped
[doctest] Status: SUCCESS!

$ ./run
[doctest] test cases: 187 | 187 passed | 0 failed | 0 skipped
[doctest] Status: SUCCESS!
```

Acceptance checks all pass:
- `NEUTRAL_TU_OK` — no `switch.h`, has `SARC::Unpack`, has `catch`, `size() < 4` guard
- `grep -c 'std=c++17' tests/Makefile` → 0
- `grep -n 'std=c++20' tests/Makefile` → line 2
- `grep -n 'switchthemes' tests/Makefile` → CXXFLAGS + SRCS entries

## Commits

| Hash | Message |
|------|---------|
| `ce51fee` | test(02-02): add failing szs_validate doctest (RED phase) |
| `273e0d3` | feat(02-02): implement is_structurally_valid_szs neutral TU (GREEN phase) |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Comment text tripped the neutral-TU grep acceptance check**
- **Found during:** Task 1 verification (`NEUTRAL_TU_OK` check)
- **Issue:** The doc comment in `szs_validate.cpp` contained the literal string `switch.h` (as `<switch.h>`). The plan's acceptance check `! grep -q "switch.h"` matched the comment text, not an actual include, causing a false failure.
- **Fix:** Rephrased the doc comment from "includes no `<switch.h>` or libnx headers" to "includes no libnx headers" — preserves the documentation intent without tripping the grep.
- **Files modified:** `source/platform/themes/szs_validate.cpp`
- **Commit:** `273e0d3` (included in GREEN commit)

## TDD Gate Compliance

| Gate | Commit | Status |
|------|--------|--------|
| RED (`test(...)`) | `ce51fee` | PASS — 2 test failures confirmed |
| GREEN (`feat(...)`) | `273e0d3` | PASS — all 6 szs tests + 187 total pass |
| REFACTOR | — | Not needed — implementation was clean |

## Known Stubs

None. `is_structurally_valid_szs` is fully implemented and all test cases pass.

## Threat Surface Scan

No new network endpoints, auth paths, or privileged file access. The validator is pure in-memory logic. T-02-04, T-02-05, T-02-06 from the plan's threat register are all mitigated:
- T-02-04: garbage/truncated buffers rejected (structural Yaz0+SARC check, host-tested)
- T-02-05: try/catch wraps both SarcLib calls — never throws to caller
- T-02-06: `NEUTRAL_TU_OK` check passes — no libnx in this TU

## Self-Check: PASSED
