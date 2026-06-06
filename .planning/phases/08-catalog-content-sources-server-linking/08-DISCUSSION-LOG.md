# Phase 8: Catalog, Content Sources & Server Linking - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-06
**Phase:** 8-Catalog, Content Sources & Server Linking
**Areas discussed:** Catalog look & cover art, Catalog freshness/fetch

---

## Area selection

| Option | Description | Selected |
|--------|-------------|----------|
| Source mgmt & navigation | Where games live in the hub; single vs multi-source; how local SD fits | |
| Catalog look & cover art | Grid/list; flat vs grouped-by-base-title; cover-art source | ✓ |
| Credential security | SYNC-02 "protected at rest" — device + cloud encryption trade-off | |
| Catalog freshness/fetch | Live vs cached index + refresh; large/nested-index handling | ✓ |

**User's choice:** Catalog look & cover art, Catalog freshness/fetch
**Notes:** Source management/navigation and credential security delegated to Claude's discretion (documented as discretion defaults in CONTEXT.md).

---

## Catalog look & cover art

### Structure — how to present a flat Tinfoil index

| Option | Description | Selected |
|--------|-------------|----------|
| Grouped by base title | One art card per game (mask low title-ID bits); updates/DLC nested in detail. Needs grouping logic in core/games. | ✓ |
| Flat list, kind-tagged | Each file its own entry tagged base/update/DLC; simplest (1:1 with index) but noisy. | |

**User's choice:** Grouped by base title
**Notes:** Approved the grouped preview (grid of game cards; detail lists Base/Update/DLC rows with sizes).

### Cover art source for not-yet-installed titles

| Option | Description | Selected |
|--------|-------------|----------|
| Online titledb → icon by title ID | Map title ID → cover from a public title-metadata source; on-SD art cache. One external metadata dep (art only). | |
| Installed icons + placeholder | Local libnx icon when installed, placeholder otherwise. No new dep, but most entries show no real art. | |
| Both: online art, fall back to local | Online titledb → installed icon → placeholder. Best visual + graceful offline; 3 code paths. | ✓ |

**User's choice:** Both: online art, fall back to local
**Notes:** Headline feature; user accepted extra moving parts to show art for every entry. Exact titledb/icon endpoint left to plan-time research (default: public title-ID→icon source); keep fail-closed TLS.

### Default layout

| Option | Description | Selected |
|--------|-------------|----------|
| Art grid, list toggle | Cover-art grid default (models theme/mod browser) + toggle to compact list for big catalogs. | ✓ |
| Art grid only | Grid only; rely on search/filter to tame size. | |
| Compact list only | Dense text list w/ small thumbnail; undersells cover-art value. | |

**User's choice:** Art grid, list toggle
**Notes:** —

### Search & filter scope (CAT-02)

| Option | Description | Selected |
|--------|-------------|----------|
| Search + sort + content filter | Text search (name/title ID) + sort (name/size) + filter chips (has upd / has DLC / base-only) from index data. | ✓ |
| Search + sort only | Text search + sort; no filter chips. | |
| Search only | Text search by name only. | |

**User's choice:** Search + sort + content filter
**Notes:** Clarified that "update available" badges for *installed* titles are Phase 11 — Phase 8 only knows what the server offers.

---

## Catalog freshness/fetch

### Index load strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Cache on SD + manual refresh | First open fetches + caches; later opens instant; visible Refresh re-fetches. Mirrors cheat-db cache. | |
| Cache + auto-refresh if stale | SD cache + background re-fetch when older than a threshold; manual refresh too. | ✓ |
| Always live fetch | Re-fetch every open, no cache. Always current, but slow + no offline. | |

**User's choice:** Cache + auto-refresh if stale
**Notes:** —

### Staleness threshold

| Option | Description | Selected |
|--------|-------------|----------|
| Every open (if online) | Paint cache instantly, then always bg-refresh when online; cache-only offline. | ✓ |
| ~6 hours | Bg refresh only if cache > ~6h. | |
| ~24 hours | Bg refresh once a day; lowest data use. | |

**User's choice:** Every open (if online)
**Notes:** Chose freshest option over minimizing re-downloads.

### Nested `directories` handling

| Option | Description | Selected |
|--------|-------------|----------|
| Recurse + flatten, bounded | Follow sub-indexes, merge into one grouped catalog, with max depth + max entries/requests cap + warn. | ✓ |
| Recurse + flatten, unbounded | Follow all dirs, any depth, no cap. Risky on a console. | |
| Top-level files only (MVP) | Parse root files only; ignore directories. Misses folder-organized titles. | |

**User's choice:** Recurse + flatten, bounded
**Notes:** Consistent with the unified grouped-catalog choice (not a folder browser).

---

## Claude's Discretion

- **Source management & navigation** — defaulted: new "Games" Home card; list of multiple
  linkable sources (empty by default per SRC-04); local SD as a peer pseudo-source through the
  same catalog/detail surface.
- **Credential security** — defaulted: device config under `app_settings` (`/switch/thomaz/config/`);
  cloud record owner-scoped + encrypted at rest, config-only (mirrors cloud-saves pattern). Full
  E2E not required for MVP. Concrete at-rest scheme chosen at plan time.

## Deferred Ideas

- Full E2E credential encryption (device-held key) — future hardening.
- USB-HDD source (GAME-F02), XCI/XCZ source (GAME-F01) — out of scope v1.2.
- Encrypted-index Tinfoil variant — out of scope for MVP.
- Update/DLC "available" badges for installed titles — Phase 11.
