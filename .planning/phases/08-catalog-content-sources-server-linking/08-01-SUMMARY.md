---
phase: 08-catalog-content-sources-server-linking
plan: 01
subsystem: testing
tags: [nlohmann-json, doctest, tinfoil, title-id, catalog, switch]

requires: []
provides:
  - parse_index tolerant Tinfoil JSON parser
  - title_kind / base_title_id / dlc_content_index bitmask helpers
  - group_catalog grouped-by-base catalog model
  - apply_view sort/filter/search transforms
  - may_descend recurse bounds check
  - serialize_source_link / parse_source_link config-only codec
affects:
  - 08-02-PLAN (platform index fetch consumes parse_index + recurse_plan)
  - 08-03-PLAN (API route consumes source_link codec)
  - 08-04-PLAN (cover art + UI consume grouped catalog)
  - 08-05-PLAN (catalog Activities consume apply_view)
  - 08-06-PLAN (source list sync consumes source_link)

tech-stack:
  added: []
  patterns:
    - Pure core/games namespace under host doctest gate
    - Tolerant nlohmann parse with allow_exceptions=false
    - Config-only sync codec (credential never in serialized JSON)

key-files:
  created:
    - source/core/games/index_parse.{hpp,cpp}
    - source/core/games/title_id.{hpp,cpp}
    - source/core/games/catalog.{hpp,cpp}
    - source/core/games/catalog_view.{hpp,cpp}
    - source/core/games/recurse_plan.{hpp,cpp}
    - source/core/games/source_link.{hpp,cpp}
    - tests/test_index_parse.cpp
    - tests/test_title_id.cpp
    - tests/test_catalog.cpp
    - tests/test_catalog_view.cpp
    - tests/test_recurse_plan.cpp
    - tests/test_source_link.cpp
    - tests/fixtures/tinfoil_index_sample.json
  modified:
    - tests/Makefile

key-decisions:
  - "Fixture tinfoil_index_sample.json is synthetic (both file shapes + directories + MOTD + unknown keys) pending validation against a real user server"
  - "Title ID for grouping extracted from [16-hex] bracket segments in Tinfoil-style URLs"

patterns-established:
  - "Pattern: core/games pure logic with return-value status, no I/O, no Switch headers"
  - "Pattern: source_link codec writes label/url/authType only — authSecret stays out of sync JSON"

requirements-completed: [SRC-01, CAT-02, SYNC-01]

duration: 45min
completed: 2026-06-07
---

# Phase 8 Plan 01: Pure core/games Foundation Summary

**Host-proven Tinfoil index parse, title-ID grouping, catalog view transforms, recurse bounds, and config-only source-link codec in `source/core/games/`**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-06-07
- **Completed:** 2026-06-07
- **Tasks:** 3
- **Files modified:** 20

## Accomplishments

- Six pure `core/games/` units with zero platform I/O — parser, title-ID logic, grouping, view transforms, recurse bounds, sync codec
- 24 new doctest cases (233 total suite, all green via `cd tests && make clean && make test`)
- Threat mitigations T-08-01 (tolerant parse), T-08-02 (credential absent from serialize), T-08-03 (recurse bounds) host-proven

## Task Commits

Each task was committed atomically:

1. **Task 1: Index parse + title-ID logic** - `6d01ee9` (feat)
2. **Task 2: Catalog grouping + view + recurse bounds** - `9b65699` (feat)
3. **Task 3: Source-link codec + Makefile wiring** - `0600af4` (feat)

## Files Created/Modified

- `source/core/games/index_parse.{hpp,cpp}` - Tolerant Tinfoil index JSON → ParsedIndex
- `source/core/games/title_id.{hpp,cpp}` - Base/update/DLC kind + base-ID mask
- `source/core/games/catalog.{hpp,cpp}` - Flat index files → GroupedTitle vector
- `source/core/games/catalog_view.{hpp,cpp}` - Sort, filter, search over grouped catalog
- `source/core/games/recurse_plan.{hpp,cpp}` - Pure depth/entries/requests bound check
- `source/core/games/source_link.{hpp,cpp}` - Config-only API JSON codec
- `tests/fixtures/tinfoil_index_sample.json` - Synthetic representative index fixture
- `tests/Makefile` - Wildcard `core/games/*.cpp` + Windows doctest signal flag

## Decisions Made

- Synthetic Tinfoil fixture used instead of a live server capture (real server not reachable at execution time); exercises both file shapes, directories, MOTD, and unknown keys

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] DOCTEST_CONFIG_NO_POSIX_SIGNALS for MSYS host build**
- **Found during:** Task 1 verification (`make test`)
- **Issue:** doctest FatalConditionHandler fails to compile under MSYS g++ 15 (incomplete sigaction)
- **Fix:** Added `-DDOCTEST_CONFIG_NO_POSIX_SIGNALS` to `tests/Makefile` CXXFLAGS
- **Files modified:** tests/Makefile
- **Verification:** `cd tests && make clean && make test` → 233/233 passed
- **Committed in:** 6d01ee9 (Task 1 commit)

**2. [Rule 1 - Bug] 16-hex title-ID search used unpadded hex**
- **Found during:** Task 2 verification
- **Issue:** `apply_view` search by full 16-char title ID failed because `std::hex` omitted leading zeros
- **Fix:** Format base/row IDs with `std::setw(16)` zero-fill before substring match
- **Files modified:** source/core/games/catalog_view.cpp
- **Verification:** test_catalog_view search case passes
- **Committed in:** 9b65699 (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 bug)
**Impact on plan:** Both required for green host gate; no scope creep.

## Issues Encountered

- PowerShell heredoc and git identity required `-c user.name/email` inline flags for commits (local git config absent in executor shell)

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Wave 2 (08-02) can consume `parse_index`, `group_catalog`, `may_descend`, and `RecurseBounds` defaults
- Wave 2 (08-03) can consume `serialize_source_link` / `parse_source_link` for API contract
- Replace synthetic fixture with a real-server capture when a Tinfoil host is available (research flag)

## Self-Check: PASSED

- FOUND: source/core/games/index_parse.cpp
- FOUND: source/core/games/source_link.cpp
- FOUND: tests/fixtures/tinfoil_index_sample.json
- FOUND: .planning/phases/08-catalog-content-sources-server-linking/08-01-SUMMARY.md
- FOUND commit: 6d01ee9
- FOUND commit: 9b65699
- FOUND commit: 0600af4

---
*Phase: 08-catalog-content-sources-server-linking*
*Completed: 2026-06-07*
