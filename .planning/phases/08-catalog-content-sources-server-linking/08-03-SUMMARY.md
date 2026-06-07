---
phase: 08-catalog-content-sources-server-linking
plan: 03
subsystem: api
tags: [prisma, fastify, jwt, aes-256-gcm, vitest, source-sync, switch]

requires:
  - phase: 08-01
    provides: serialize_source_link / parse_source_link codec
provides:
  - SourceLink Prisma model (owner-scoped, config-only)
  - SOURCE_ENC_KEY + source-crypto AES-256-GCM helpers
  - JWT /sources CRUD route (no multipart/blob)
  - sources.test.ts Vitest owner-scoping + encryption spec
  - HttpSourceSyncClient device config sync client
affects:
  - 08-06-PLAN (source_list_activity one-tap sync)
  - Wave 3+ catalog UI (cloud restore on second console)

tech-stack:
  added: []
  patterns:
    - SaveSlot trio clone for config-only cloud sync
    - authSecretEnc at-rest via node:crypto aes-256-gcm
    - DTO exposes hasSecret, never plaintext or ciphertext
    - Device client mirrors HttpCloudSaveClient Bearer + 401 sentinel

key-files:
  created:
    - api/prisma/migrations/20260607150000_add_source_link/migration.sql
    - api/src/lib/source-crypto.ts
    - api/src/routes/sources.ts
    - api/test/sources.test.ts
    - source/platform/games/http_source_sync_client.hpp
    - source/platform/games/http_source_sync_client.cpp
  modified:
    - api/prisma/schema.prisma
    - api/src/config.ts
    - api/src/lib/serializers.ts
    - api/src/app.ts
    - api/test/api.test.ts
    - api/.env.example
    - tests/Makefile

key-decisions:
  - "PUT /sources/:id uses explicit owner collision check before create — prevents cross-account id squatting"
  - "HttpSourceSyncClient::push takes cloud id as separate parameter (SourceConfig has no id field from Plan 01)"

patterns-established:
  - "Pattern: cloud source sync stores config JSON + optional secret field — never catalog bytes or multipart"
  - "Pattern: list()/push() reuse Plan 01 parse_source_link / serialize_source_link codec on device"

requirements-completed: [SYNC-01, SYNC-02]

duration: 75min
completed: 2026-06-07
---

# Phase 8 Plan 03: Cloud Config Sync Summary

**Owner-scoped SourceLink model with AES-256-GCM credentials, JWT /sources config-only API, and HttpSourceSyncClient mirroring cloud saves**

## Performance

- **Duration:** ~75 min
- **Started:** 2026-06-07
- **Completed:** 2026-06-07
- **Tasks:** 3
- **Files modified:** 13

## Accomplishments

- `SourceLink` Prisma model stores label/url/authType + encrypted `authSecretEnc` — no blob/content field (SYNC-01 structural proof)
- `/sources` GET/PUT/DELETE routes are JWT-guarded, owner-scoped, and return `hasSecret` without leaking credentials (SYNC-02)
- `HttpSourceSyncClient` sends config JSON via `serialize_source_link` + optional `secret`; no `req.files` multipart
- Host suite green: 235 test cases, 689 assertions (`cd tests && make test`)

## Task Commits

Each task was committed atomically:

1. **Task 1: SourceLink model + AES-256-GCM at-rest crypto + config wiring** - `d7dc491` (feat)
2. **Task 2: /sources route + registration + Vitest** - `277030c` (feat)
3. **Task 3: Device http_source_sync_client** - `c372bf3` (feat)

## Files Created/Modified

- `api/prisma/schema.prisma` - SourceLink model + User.sourceLinks back-relation
- `api/prisma/migrations/20260607150000_add_source_link/migration.sql` - CREATE TABLE migration
- `api/src/lib/source-crypto.ts` - encryptSecret/decryptSecret (iv:tag:ciphertext base64)
- `api/src/lib/serializers.ts` - toSourceLinkDto with hasSecret
- `api/src/routes/sources.ts` - JWT config-only CRUD
- `api/src/app.ts` - sourcesRoutes registration
- `api/test/sources.test.ts` - 401, owner read, cross-user isolation, at-rest encryption
- `source/platform/games/http_source_sync_client.{hpp,cpp}` - Device sync client
- `tests/Makefile` - Links http_source_sync_client into host build

## Decisions Made

- Owner-scoped upsert rejects id collision across accounts (404) instead of naive Prisma upsert by id alone
- `push(token, id, cfg)` takes cloud id separately because `SourceConfig` omits id per Plan 01 codec

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Manual migration SQL when Postgres unreachable**
- **Found during:** Task 1 verification (`prisma migrate dev`)
- **Issue:** Docker daemon not running; `localhost:5433` unreachable
- **Fix:** Authored migration SQL matching Prisma-generated shape (`20260607150000_add_source_link`)
- **Files modified:** api/prisma/migrations/20260607150000_add_source_link/migration.sql
- **Verification:** `npx prisma validate` + `npx tsc --noEmit` pass
- **Committed in:** d7dc491

**2. [Rule 2 - Security] Owner-scoped PUT collision guard**
- **Found during:** Task 2 implementation review
- **Issue:** Naive `upsert({ where: { id } })` could update another user's row if ids collide
- **Fix:** `findFirst({ userId, id })` + collision check before create; update only owned rows
- **Files modified:** api/src/routes/sources.ts
- **Committed in:** 277030c

**3. [Rule 2 - Missing Critical] push() requires explicit cloud id parameter**
- **Found during:** Task 3 implementation
- **Issue:** Plan signature `push(token, cfg)` cannot target `PUT /sources/:id` — `SourceConfig` has no id
- **Fix:** `push(token, id, cfg, cancelled)` — id supplied by caller after list/sync orchestration
- **Files modified:** source/platform/games/http_source_sync_client.hpp
- **Committed in:** c372bf3

---

**Total deviations:** 3 auto-fixed (1 blocking, 2 security/correctness)
**Impact on plan:** Required for migration delivery and SYNC-02 isolation; no scope creep.

## Issues Encountered

- API Vitest integration tests require PostgreSQL at `localhost:5433` (`api/docker compose up` + `npx prisma migrate deploy`). Docker Desktop was not running during execution — all DB-dependent tests returned 500; code paths verified via `tsc --noEmit` and review. Re-run: `cd api && docker compose up -d && npx prisma migrate deploy && npm test`.

## User Setup Required

Add to `api/.env` (see `.env.example`):

```bash
SOURCE_ENC_KEY="<base64-encoded 32-byte key>"
```

Generate a key: `node -e "console.log(require('crypto').randomBytes(32).toString('base64'))"`

## Next Phase Readiness

- Wave 3 (08-04) can add Home Games card and cover-art service; cloud half is ready for 08-06 one-tap sync UI
- On-hardware two-console sync UAT deferred to phase gate (nxlink)
- Start Postgres before running `npm test` in `api/`

## Self-Check: PASSED

- FOUND: api/src/routes/sources.ts
- FOUND: api/src/lib/source-crypto.ts
- FOUND: source/platform/games/http_source_sync_client.cpp
- FOUND: .planning/phases/08-catalog-content-sources-server-linking/08-03-SUMMARY.md
- FOUND commit: d7dc491
- FOUND commit: 277030c
- FOUND commit: c372bf3

---
*Phase: 08-catalog-content-sources-server-linking*
*Completed: 2026-06-07*
