---
phase: 02-full-extraction-engine
plan: "01"
subsystem: themes
tags: [cfw_paths, firmware_extract, doctest, extraction-engine, target_map]

# Dependency graph
requires:
  - phase: 01-privileged-extraction-spike
    provides: firmware_extract.hpp ExtractResult + extract_base_layout() API; cfw_paths target_map() with 7 arms
provides:
  - "target_map() with 8 arms including common (D-01a reconciliation)"
  - "ExtractAllResult struct + extract_all_base_layouts() declaration in neutral firmware_extract.hpp"
  - "Desktop no-op extract_all_base_layouts() in firmware_extract_fake.cpp"
  - "Host doctest coverage for common mapping and flat base_szs_path"
affects: [02-02, 02-03, 02-04, phase-03-ui-integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ExtractAllResult systemic-vs-per-part split (D-02/D-02a): ok=false only on systemic abort; failed_parts collects per-part warnings"
    - "Interface-first ordering: header contract declared before Switch implementation (Plan 03)"
    - "Neutral header invariant: firmware_extract.hpp limited to <string>/<vector>, zero Switch/SarcLib symbols"

key-files:
  created: []
  modified:
    - source/platform/themes/cfw_paths.cpp
    - tests/test_cfw_paths.cpp
    - source/platform/themes/firmware_extract.hpp
    - source/platform/themes/firmware_extract_fake.cpp

key-decisions:
  - "common arm uses title-ID 0100000000001000 (qlaunch) and szs common.szs — confirmed against ThemeTargetInfo::QlaunchCommon in lib/switchthemes/Common.cpp"
  - "ExtractAllResult is additive — existing ExtractResult + extract_base_layout() are preserved for live caller in theme_detail_activity.cpp"
  - "ok=false implies written_parts is empty — documented as hard invariant in both header doc comment and fake no-op"

patterns-established:
  - "Interface-first: declare ExtractAllResult in neutral header before writing the Switch driver (Plan 02-03)"
  - "Additive target_map() arms: existing 7 callers unaffected; base_present_for/base_szs_path derive automatically"

requirements-completed: [EXTRACT-01, EXTRACT-02, EXTRACT-03]

# Metrics
duration: 15min
completed: 2026-06-05
---

# Phase 02 Plan 01: Full Extraction Engine Contracts Summary

**Reconciled target_map() to 8 arms including common.szs (D-01a) and declared ExtractAllResult + extract_all_base_layouts() interface contract for the multi-title extraction engine**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-06-05T16:00:00Z
- **Completed:** 2026-06-05T16:07:31Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Added the missing `common` arm to `target_map()` — title-ID `0100000000001000`, szs `common.szs` — reconciling the apply-path table with `ThemeTargetInfo::QlaunchCommon` in the vendored lib
- Extended `tests/test_cfw_paths.cpp` with two new `TEST_CASE`s asserting the common mapping and the flat `base_szs_path` — all 181 doctest assertions pass
- Declared `ExtractAllResult` struct and `extract_all_base_layouts()` entry point in the neutral `firmware_extract.hpp` using only `bool`/`std::string`/`std::vector<std::string>` (no libnx, no SarcLib) — interface contract ready for Plan 03's Switch driver
- Added desktop no-op `extract_all_base_layouts()` to `firmware_extract_fake.cpp` returning `{false, "Firmware extraction is only available on Switch.", {}, {}}`
- Preserved existing `ExtractResult` + `extract_base_layout()` for live caller in `theme_detail_activity.cpp`

## Task Commits

Each task was committed atomically:

1. **Task 1: Add common arm to target_map() and lock with host doctest** - `f71ae0e` (feat)
2. **Task 2: Declare ExtractAllResult + extract_all_base_layouts() with desktop no-op** - `ee5af63` (feat)

## Files Created/Modified

- `source/platform/themes/cfw_paths.cpp` — Added `if (target == "common") return TargetMap{"0100000000001000", "common.szs"};` before `return std::nullopt;` (8th arm)
- `tests/test_cfw_paths.cpp` — Added `TEST_CASE("target_map common arm resolves to qlaunch title and common.szs")` and `TEST_CASE("base_szs_path common is flat inside base_layout_dir")`
- `source/platform/themes/firmware_extract.hpp` — Added `#include <vector>`, `struct ExtractAllResult` with 4 members, `ExtractAllResult extract_all_base_layouts()` declaration with doc comment
- `source/platform/themes/firmware_extract_fake.cpp` — Added `ExtractAllResult extract_all_base_layouts()` no-op definition

## Decisions Made

- **common arm uses qlaunch title-ID** — Confirmed against `ThemeTargetInfo::QlaunchCommon` in `lib/switchthemes/Common.cpp:57-59`. The `GetTargetsForTitleId` function prepends `common.szs` to qlaunch's list, confirming the mapping is correct and consistent.
- **Additive approach for ExtractAllResult** — The existing `ExtractResult` + `extract_base_layout(const std::string& target)` are live callers in `theme_detail_activity.cpp:428-431` and MUST be preserved. The new multi-target API is purely additive.
- **Systemic-vs-per-part failure contract documented in-header** — The `ok=false implies written_parts is empty` invariant is codified in the doc comment so Plan 03's implementer cannot misinterpret the contract.

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `target_map()` reconciled with 8 arms; `base_present_for` can now resolve `common`
- `ExtractAllResult` + `extract_all_base_layouts()` interface contract locked; Plan 02-02 (NCA filter widening) and Plan 02-03 (Switch driver) can proceed
- Desktop build contract preserved: fake no-op present, header carries no Switch symbols
- Host doctest suite: 181 tests passing

---
*Phase: 02-full-extraction-engine*
*Completed: 2026-06-05*
