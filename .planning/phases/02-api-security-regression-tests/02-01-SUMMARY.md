---
phase: 02-api-security-regression-tests
plan: "01"
subsystem: database, api
tags: [prisma, postgres, migration, pino, fastify, logging, revoked-token]

# Dependency graph
requires:
  - phase: 01-remove-community-feature
    provides: Clean schema (community models removed) that this phase adds RevokedToken to
provides:
  - RevokedToken Prisma model in schema.prisma (jti PK, userId, expiresAt, createdAt, @@index([expiresAt]))
  - Migration 20260605004126_revoked_tokens applied to test Postgres (localhost:5433)
  - prisma.revokedToken delegate (generated Prisma client, typed) ready for Plan 02 blocklist lookups
  - envToLogger map in app.ts with shared redact object for req.headers.authorization and req.headers.cookie
  - pino-pretty devDependency for development log transport
affects:
  - 02-02 (SEC-02 token revocation — needs prisma.revokedToken delegate)
  - 02-03 (regression tests — needs silent test logger and working DB)

# Tech tracking
tech-stack:
  added:
    - pino-pretty ^13.1.3 (devDependency — development log transport)
  patterns:
    - envToLogger map keyed by NODE_ENV with shared redact object (production branch is object not bare true)
    - RevokedToken as relationless model (bare userId string, no FK/cascade dependency per D-07)
    - prisma migrate dev for local authoring + migrate deploy for CI/production (Pitfall 1 compliance)

key-files:
  created:
    - api/prisma/migrations/20260605004126_revoked_tokens/migration.sql
  modified:
    - api/prisma/schema.prisma
    - api/src/app.ts
    - api/package.json

key-decisions:
  - "RevokedToken model uses bare userId String (no User relation/FK) per D-07 — avoids cascade-delete dependency"
  - "production branch of envToLogger is { redact } object, NOT bare true — enforces header redaction in production (Pitfall 2)"
  - "test branch of envToLogger is false — keeps Vitest suite silent (D-10)"
  - "prisma migrate dev used for local authoring; migration auto-applied to test DB (localhost:5433)"

patterns-established:
  - "Pattern: envToLogger[env.NODE_ENV] with shared redact object as const — use this for all future Fastify logger config"
  - "Pattern: RevokedToken migration follows hand-numbered style matching existing migrations (no db push)"

requirements-completed: [DEBT-04]

# Metrics
duration: 15min
completed: 2026-06-05
---

# Phase 02 Plan 01: Foundation Summary

**RevokedToken Prisma model + applied migration + envToLogger pino map with header redaction lays the DB and logging foundation for Phase 2 security plans**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-06-05T00:40:00Z
- **Completed:** 2026-06-05T00:43:18Z
- **Tasks:** 2
- **Files modified:** 4 (schema.prisma, migration.sql new, app.ts, package.json + package-lock.json)

## Accomplishments

- Added `RevokedToken` Prisma model (jti PK, userId, expiresAt, createdAt, @@index([expiresAt])) — no User relation/FK per D-07
- Authored migration `20260605004126_revoked_tokens` via `prisma migrate dev`; migration applied to test Postgres (localhost:5433) with no pending migrations; `prisma generate` ran automatically providing typed `prisma.revokedToken` delegate
- Replaced `Fastify({ logger: false })` with `envToLogger[env.NODE_ENV]` map: production emits redacted pino JSON, development uses pino-pretty, test stays silent
- Shared `redact` object removes `req.headers.authorization` and `req.headers.cookie` from all logs — production branch is `{ redact }` object (not bare `true`) per Pitfall 2 / D-11
- Added `pino-pretty ^13.1.3` as devDependency; `pino` NOT added as direct dep (Fastify 5 bundles it)

## Task Commits

1. **Task 1: RevokedToken model, migration, generate client** - `17ad4ae` (feat)
2. **Task 2: envToLogger map + pino-pretty devDep** - `c7f33d9` (feat)

**Plan metadata:** (docs commit below)

## Files Created/Modified

- `api/prisma/schema.prisma` — Added `model RevokedToken` after `RefreshToken`; no User relation (D-07)
- `api/prisma/migrations/20260605004126_revoked_tokens/migration.sql` — CREATE TABLE RevokedToken with PK on jti, CREATE INDEX on expiresAt; no UNIQUE index, no FK
- `api/src/app.ts` — Replaced `logger: false` with `envToLogger` const map + shared `redact` object; `pino-pretty` transport for development
- `api/package.json` — Added `pino-pretty ^13.1.3` to devDependencies

## Decisions Made

- RevokedToken `userId` is a bare `String` (no `User` relation) per D-07 — keeps the model relationless and avoids cascade-delete dependencies; `userId` still present for future "revoke all sessions" audit
- `production` branch of `envToLogger` is `{ redact }` object — the bare `true` value enables logging with no redaction, violating DEBT-04 (Pitfall 2 from RESEARCH.md)
- Used `prisma migrate dev --name revoked_tokens` for authoring (auto-generates timestamp-prefixed dir + SQL); consistent with `20260604233032_remove_community_models` naming style
- `pino` not added as a direct dependency — Fastify 5 bundles `pino ^9` transitively (verified in RESEARCH.md)

## Deviations from Plan

None — plan executed exactly as written. Both tasks followed the specification in 02-01-PLAN.md and the patterns in 02-PATTERNS.md without any unplanned changes.

## Issues Encountered

None. Test Postgres was reachable at localhost:5433 at execution start. Migration applied cleanly in one pass. Typecheck and Vitest suite both passed without issues.

## Known Stubs

None — no stub values or placeholder text introduced. The `envToLogger` map is fully wired to `env.NODE_ENV` and the `prisma.revokedToken` delegate is generated and typed.

## Threat Flags

No new threat surface introduced beyond what was planned. The `redact` object is applied to both `development` and `production` branches — no branch leaks Authorization or cookie headers. The `test` branch disables logging entirely.

## Next Phase Readiness

- `prisma.revokedToken` delegate is available and typed — Plan 02 (SEC-02 token revocation) can call `revokedToken.findUnique`, `revokedToken.deleteMany`, and `revokedToken.upsert` without any additional schema/generate steps
- `envToLogger` map is wired — authorization headers are never written to logs in any environment
- Migration is applied to test Postgres — SEC-02 regression tests can run against the real `RevokedToken` table
- Existing 6-test Vitest suite is still green; test logger is silent (logger: false for NODE_ENV=test)

---
*Phase: 02-api-security-regression-tests*
*Completed: 2026-06-05*
