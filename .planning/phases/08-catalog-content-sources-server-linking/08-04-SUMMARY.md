---
phase: 08-catalog-content-sources-server-linking
plan: 04
subsystem: ui
tags: [i18n, borealis, cover-art, titledb, home-card, switch]

requires:
  - phase: 08-01
    provides: TitleKind, title_id helpers
  - phase: 08-02
    provides: catalog_cache SD pattern, IHttpClient stream seam
provides:
  - catalog/sources i18n namespace (en-US + pt-BR, 20 keys)
  - thomaz/tile_games theme token
  - Home gamesCard wired to SourceListActivity
  - source_list_activity.hpp forward declaration
  - cover_art 3-tier resolution (titledb -> libnx -> placeholder)
affects:
  - 08-05-PLAN (catalog grid consumes cover_art + i18n)
  - 08-06-PLAN (source_list_activity.cpp body + source_list.xml)

tech-stack:
  added: []
  patterns:
    - New catalog/sources i18n namespace separate from games (My Games)
    - Regional titledb streamed once to SD; compact icon_index.json derived cache
    - Art fetch capped at 4 MiB with to_decodable_image transcode
    - Placeholder tier returns ok=true with empty bytes

key-files:
  created:
    - source/app/source_list_activity.hpp
    - source/platform/games/cover_art.hpp
    - source/platform/games/cover_art.cpp
  modified:
    - resources/i18n/en-US/thomaz.json
    - resources/i18n/pt-BR/thomaz.json
    - source/app/theme.cpp
    - resources/xml/activity/home.xml
    - source/app/home_activity.hpp
    - source/app/home_activity.cpp

key-decisions:
  - "gamesCard placed in column B above cheatsCard with entranceDelay 230/290 cascade"
  - "titledb US.en.json (~86 MiB) downloaded via stream() to SD; icon_index.json compact map built on first parse"
  - "titledb_regional_url() single constant in cover_art.cpp for plan-time endpoint swaps"

patterns-established:
  - "Pattern: Phase 8 copy lives under catalog/ + sources/ namespaces — never games/"
  - "Pattern: cover_art SD cache at /switch/thomaz/cache/art/{TITLEID}.img"

requirements-completed: [CAT-01, SRC-04]

duration: 35min
completed: 2026-06-07
---

# Phase 8 Plan 04: Shared UI Scaffolding Summary

**catalog/sources i18n (both locales), indigo tile_games Home card, SourceListActivity header wiring, and 3-tier cover_art with titledb stream-cache**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-06-07
- **Completed:** 2026-06-07
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments

- 20-key `catalog`/`sources` namespace added to en-US and pt-BR with verified key parity; existing `games` namespace untouched
- `thomaz/tile_games` indigo token (`nvgRGB(0x49,0x5C,0xD6)`) registered; `gamesCard` in home.xml pushes `SourceListActivity`
- `source_list_activity.hpp` declared on `ThomazActivity` base for Plan 06 body
- `cover_art` resolves titledb iconUrl (stream-cached regional JSON + derived icon_index) → libnx JPEG → placeholder (`ok=true`)
- titledb endpoint reachable (HTTP 200, ~86 MiB `US.en.json`); art capped at 4 MiB with `to_decodable_image`
- Host suite green: 235 test cases, 689 assertions

## Task Commits

Each task was committed atomically:

1. **Task 1: i18n + tile_games + Home card** - `011604a` (feat)
2. **Task 2: cover_art 3-tier resolution** - `622fe1e` (feat)

**Plan metadata:** `a410cf7` (docs: complete plan)

## Files Created/Modified

- `resources/i18n/en-US/thomaz.json` - catalog/sources English copy (20 keys)
- `resources/i18n/pt-BR/thomaz.json` - catalog/sources Portuguese copy (parity)
- `source/app/theme.cpp` - `thomaz/tile_games` color token
- `resources/xml/activity/home.xml` - focusable `gamesCard` with tile_games background
- `source/app/home_activity.{hpp,cpp}` - gamesCard → `SourceListActivity` push
- `source/app/source_list_activity.hpp` - Activity declaration for Plan 06
- `source/platform/games/cover_art.{hpp,cpp}` - 3-tier art resolution service

## Decisions Made

- Stream-download regional titledb to SD (avoids buffering 86 MiB in `HttpResponse.body`); build compact `icon_index.json` on first parse
- `SourceListActivity` .cpp deferred to Plan 06 per plan — Switch link completes when 08-06 lands

## Deviations from Plan

None - plan executed as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Wave 4 Plan 05 can build CatalogActivity against cover_art + i18n scaffolding
- Wave 5 Plan 06 implements `source_list_activity.cpp` + `source_list.xml` (header already wired from Home)
- On-hardware UAT (gamesCard tile hue, cover art tiers) deferred to phase gate via nxlink
- First titledb fetch may hit curl 30s stream timeout on slow links — 3-tier fallback keeps catalog usable

## Self-Check: PASSED

- FOUND: source/platform/games/cover_art.cpp
- FOUND: source/app/source_list_activity.hpp
- FOUND: resources/i18n/en-US/thomaz.json (catalog namespace)
- FOUND: .planning/phases/08-catalog-content-sources-server-linking/08-04-SUMMARY.md
- FOUND commit: 011604a
- FOUND commit: 622fe1e

---
*Phase: 08-catalog-content-sources-server-linking*
*Completed: 2026-06-07*
