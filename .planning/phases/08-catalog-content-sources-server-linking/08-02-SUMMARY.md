---
phase: 08-catalog-content-sources-server-linking
plan: 02
subsystem: platform
tags: [libcurl, streaming, tinfoil, index-fetch, source-store, catalog-cache, switch]

requires:
  - phase: 08-01
    provides: parse_index, may_descend, SourceConfig codec
provides:
  - IHttpClient stream() non-buffering Range seam
  - index_fetcher capped redirect-safe catalog fetch
  - source_store empty-by-default SD persistence
  - catalog_cache per-source SD index cache
affects:
  - 08-03-PLAN (http_source_sync_client consumes source_store)
  - 08-05-PLAN (catalog UI consumes catalog_cache cache-first paint)
  - 08-06-PLAN (source_list_activity consumes source_store)
  - 10 (install engine consumes stream() seam)

tech-stack:
  added: []
  patterns:
    - Parallel stream() path beside untouched request() buffering path
    - Header-auth manual redirect loop with same_host guard
    - FNV-1a stable cache filenames from source URL
    - Host doctest compiles platform/games pure helpers without borealis

key-files:
  created:
    - source/platform/games/index_fetcher.{hpp,cpp}
    - source/platform/games/index_fetch_util.{hpp,cpp}
    - source/platform/games/source_store.{hpp,cpp}
    - source/platform/games/catalog_cache.{hpp,cpp}
    - tests/test_index_fetcher.cpp
    - tests/test_source_store.cpp
  modified:
    - source/platform/http_client.hpp
    - source/platform/http_client_curl.cpp
    - tests/Makefile

key-decisions:
  - "IHttpClient::stream returns StreamResult{ok=false} by default so existing test doubles compile without overrides"
  - "source_store logging gated behind __SWITCH__ so host doctest suite links without borealis headers"
  - "catalog_cache uses FNV-1a hex hash of source URL for stable per-source filenames across builds"

patterns-established:
  - "Pattern: index fetch uses request() with 8 MiB cap; content install uses stream() (Phase 10)"
  - "Pattern: custom Header auth disables auto-follow and re-attaches header only when same_host(origin, target)"

requirements-completed: [SRC-01, SRC-02, SRC-04]

duration: 55min
completed: 2026-06-07
---

# Phase 8 Plan 02: Platform Data Layer Summary

**Non-buffering stream() seam plus capped index_fetcher, empty-by-default source_store, and per-source catalog_cache on SD**

## Performance

- **Duration:** ~55 min (resumed from partial Task 1)
- **Started:** 2026-06-07
- **Completed:** 2026-06-07
- **Tasks:** 3
- **Files modified:** 14

## Accomplishments

- `IHttpClient::stream()` delivers body chunks via sink callback with Range resume and Accept-Ranges probe; `request()` buffering path untouched
- `fetch_index` merges nested Tinfoil directories with recurse_plan bounds, 8 MiB cap, cycle detection, and same-host Header-auth redirect guard
- `load_sources()` returns empty vector on missing config (SRC-04); `catalog_cache` reads/writes per-source index JSON up to 8 MiB
- Host suite green: 235 test cases, 689 assertions

## Task Commits

Each task was committed atomically:

1. **Task 1: Streaming/Range stream() seam** - `ae58c65` (feat)
2. **Task 2: index_fetcher** - `304905a` (feat)
3. **Task 3: source_store + catalog_cache** - `8346419` (feat)

**Plan metadata:** `b5abc9a` (docs: complete plan)

## Files Created/Modified

- `source/platform/http_client.hpp` - StreamRequest/StreamResult, followRedirects, maxBodyBytes, response headers
- `source/platform/http_client_curl.cpp` - stream() sink trampoline, capped request body, header callback
- `source/platform/games/index_fetcher.{hpp,cpp}` - Bounded multi-directory index fetch orchestration
- `source/platform/games/index_fetch_util.{hpp,cpp}` - same_host + redacted_host_from_url pure helpers
- `source/platform/games/source_store.{hpp,cpp}` - sources.json persistence, empty default
- `source/platform/games/catalog_cache.{hpp,cpp}` - Per-source SD index cache
- `tests/test_index_fetcher.cpp` - same_host doctest
- `tests/test_source_store.cpp` - empty-default doctest
- `tests/Makefile` - Links platform/games helpers into host suite

## Decisions Made

- Default `IHttpClient::stream()` stub (lower churn vs pure-virtual + override in every fake)
- `source_store` Switch-only logging to keep host doctest build free of borealis dependency
- FNV-1a 64-bit hash for catalog cache filenames (stable unlike `std::hash`)

## Deviations from Plan

None - plan executed as written.

## Issues Encountered

- Host doctest build failed linking `source_store.cpp` with borealis.hpp — resolved by gating Logger behind `__SWITCH__`
- `catalog_cache.cpp` needed `<cstdint>` for `std::uint64_t` on MinGW host toolchain

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Wave 2 Plan 03 can add cloud config sync client against `source_store` + `serialize_source_link`
- Wave 4/5 catalog Activities can call `catalog_cache::read_cached_index` for cache-first paint
- Phase 10 can consume proven `stream()` interface for install engine
- On-hardware index fetch + Range probe deferred to phase gate (nxlink)

## Self-Check: PASSED

- FOUND: source/platform/games/index_fetcher.cpp
- FOUND: source/platform/games/source_store.cpp
- FOUND: source/platform/games/catalog_cache.cpp
- FOUND: .planning/phases/08-catalog-content-sources-server-linking/08-02-SUMMARY.md
- FOUND: ae58c65
- FOUND: 304905a
- FOUND: 8346419

---
*Phase: 08-catalog-content-sources-server-linking*
*Completed: 2026-06-07*
