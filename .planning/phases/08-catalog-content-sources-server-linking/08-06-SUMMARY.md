---
phase: 08-catalog-content-sources-server-linking
plan: 06
subsystem: ui
tags: [borealis, source-list, local-source, sync, switch]

requires:
  - phase: 08-02
    provides: source_store empty-by-default, index_fetcher
  - phase: 08-03
    provides: HttpSourceSyncClient one-tap cloud sync
  - phase: 08-04
    provides: catalog/sources i18n, Home gamesCard wiring
  - phase: 08-05
    provides: CatalogActivity cache-first browse surface
provides:
  - local_source SD scanner synthesizing ParsedIndex from .nsp/.nsz
  - source_list.xml + SourceListActivity (empty default, add/remove/sync/open)
  - local://sd peer row + CatalogActivity local scan routing
affects:
  - phase-9 (install queue consumes catalog/source selection)
  - phase-gate (on-hardware UAT via nxlink)

tech-stack:
  added: []
  patterns:
    - Local pseudo-source uses local://sd marker; scan_local_files feeds group_catalog
    - Source rows display redacted_host_from_url only — never raw credentials
    - Sync pushes via HttpSourceSyncClient with persisted remoteId for idempotent PUT
    - Empty-by-default list lands focus on Add Source (SRC-04)

key-files:
  created:
    - source/platform/games/local_source.hpp
    - source/platform/games/local_source.cpp
    - resources/xml/activity/source_list.xml
    - source/app/source_list_activity.cpp
  modified:
    - source/app/source_list_activity.hpp
    - source/app/catalog_activity.cpp
    - source/core/games/source_link.hpp
    - source/platform/games/source_store.cpp
    - tests/Makefile

key-decisions:
  - "local://sd sentinel URL routes CatalogActivity to scan_local_files instead of fetch_index"
  - "remoteId persisted in sources.json (device-only) for stable cloud PUT upserts"
  - "Local SD peer always rendered; never stored in source_store (not a seeded remote)"

patterns-established:
  - "Pattern: bounded SD dirent scan mirrors cheat_store stat walk with RecurseBounds maxEntries"
  - "Pattern: SourceListActivity busy-guards Add/Sync; remove confirm defaults focus to Cancel"

requirements-completed: [SRC-02, SRC-03, SRC-04, SYNC-01]

duration: 50min
completed: 2026-06-07
---

# Phase 8 Plan 06: Source List + Local SD Summary

**Empty-by-default SourceListActivity with credential-redacted remote rows, local://sd peer browsing via the shared catalog, and one-tap cloud config sync**

## Performance

- **Duration:** ~50 min
- **Started:** 2026-06-07
- **Completed:** 2026-06-07
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- `scan_local_files()` walks `local_source_dir()` for `.nsp`/`.nsz`, synthesizing `IndexFile{url=path,size=stat}` that flows through `core::group_catalog`
- `SourceListActivity` loads empty `source_store`, shows SRC-04 empty state, focus on Add Source; local SD peer always visible
- Add-source flow: URL swkbd + optional Header/Referrer auth (BasicInUrl auto-detected); rows show redacted host only
- One-tap sync via `HttpSourceSyncClient::push` with `remoteId` persistence; success flips Sync chip to `thomaz/good`
- Picking any source pushes `CatalogActivity`; `local://sd` routes to `scan_local_files()` with truncation note support
- Remove uses confirm dialog with Cancel first (default No); cloud remove best-effort when `remoteId` present

## Task Commits

Each task was committed atomically:

1. **Task 1: local_source SD scanner** - `edb6235` (feat)
2. **Task 2: source_list.xml + SourceListActivity** - `24659a1` (feat)

**Plan metadata:** `0f49f3d` (docs: complete plan)

## Files Created/Modified

- `source/platform/games/local_source.{hpp,cpp}` - Bounded SD scan → synthesized ParsedIndex
- `resources/xml/activity/source_list.xml` - Add/Sync chips, empty state, results list
- `source/app/source_list_activity.{hpp,cpp}` - Full source-management Activity body
- `source/app/catalog_activity.cpp` - Local source scan routing + frame title
- `source/core/games/source_link.hpp` - `remoteId` field (device-only, not API codec)
- `source/platform/games/source_store.cpp` - Persist/load `remoteId`
- `tests/Makefile` - Compile `local_source.cpp` in host suite

## Decisions Made

- `local://sd` URL scheme marks the pseudo-source; label comes from i18n at display time
- `remoteId` stored in `sources.json` enables idempotent `PUT /sources/:id` on re-sync
- Local peer is runtime-only (not saved to `sources.json`) to preserve SRC-04 empty default

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added remoteId persistence for cloud sync upserts**
- **Found during:** Task 2 (doSync wiring)
- **Issue:** Without stable server ids, every sync would create duplicate SourceLink rows
- **Fix:** Added `remoteId` to `SourceConfig` + `source_store` JSON; generate FNV-based id on first push
- **Files modified:** source/core/games/source_link.hpp, source/platform/games/source_store.cpp, source/app/source_list_activity.cpp
- **Committed in:** 24659a1

**2. [Rule 2 - Missing Critical] CatalogActivity local://sd routing**
- **Found during:** Task 2 (open local peer)
- **Issue:** Plan objective requires local browse through same catalog surface; CatalogActivity only called fetch_index
- **Fix:** `refreshFromNetwork()` branches to `scan_local_files()` when `is_local_source()`
- **Files modified:** source/app/catalog_activity.cpp
- **Committed in:** 24659a1

---

**Total deviations:** 2 auto-fixed (2 missing critical)
**Impact on plan:** Required for SYNC-01 idempotency and SRC-03 end-to-end browse. No scope creep.

## Issues Encountered

- `xmllint` not installed on host — XML validated structurally against theme_browser shell pattern
- Host suite green: 235 test cases, 689 assertions (`cd tests && make test`)

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 8 all 6 plans complete — phase gate UAT deferred to nxlink (empty list → add server → browse catalog → sync → remove)
- Phase 9 can consume catalog grouping + source selection for install queue enqueue
- SRC-02/03/04 and SYNC-01 device wiring satisfied at code level; hardware confirmation pending

## Self-Check: PASSED

- FOUND: source/platform/games/local_source.hpp
- FOUND: source/platform/games/local_source.cpp
- FOUND: resources/xml/activity/source_list.xml
- FOUND: source/app/source_list_activity.cpp
- FOUND: .planning/phases/08-catalog-content-sources-server-linking/08-06-SUMMARY.md
- FOUND commit: edb6235
- FOUND commit: 24659a1

---
*Phase: 08-catalog-content-sources-server-linking*
*Completed: 2026-06-07*
