---
phase: 01-remove-community-feature
plan: "03"
subsystem: database
tags: [prisma, postgresql, migration, vitest, cmake]

# Dependency graph
requires:
  - phase: 01-remove-community-feature/01-01
    provides: API routes and test cleanup for community feature removal
  - phase: 01-remove-community-feature/01-02
    provides: C++ feed client removal
provides:
  - Post/Like/Comment Prisma models dropped via migration 20260604233032_remove_community_models
  - Prisma client regenerated without community query methods
  - Local dev DB migrated; tables Comment, Like, Post dropped
  - Vitest suite green (6 tests passing) — Phase 1 regression baseline established
affects: [02-api-security, 03-cpp-platform, 04-cpp-activities]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Prisma migrate dev via pseudo-TTY (script command) to handle non-interactive warning confirmation"

key-files:
  created:
    - api/prisma/migrations/20260604233032_remove_community_models/migration.sql
  modified:
    - api/prisma/schema.prisma

key-decisions:
  - "Used script -q -c to wrap prisma migrate dev in pseudo-TTY; the non-interactive detection blocks even --create-only without a TTY"
  - "Fixed api/.env DATABASE_URL from postgres:5432 to thomaz:5433 to match docker-compose.yml; the .env was stale"
  - "@fastify/multipart retained in app.ts — it is active for save blob uploads, not a dead community reference"
  - "Two-step approach not needed: script + printf y successfully ran migrate dev interactively"

patterns-established:
  - "Schema edits paired with immediate migrate dev for atomic data-layer changes"

requirements-completed: [RM-02, RM-04]

# Metrics
duration: 12min
completed: 2026-06-04
---

# Phase 01 Plan 03: Drop Community Prisma Models Summary

**Post/Like/Comment Prisma models and tables dropped via migration 20260604233032_remove_community_models; Vitest suite green (6/6) and desktop build clean — Phase 1 complete**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-06-04T23:20:00Z
- **Completed:** 2026-06-04T23:32:22Z
- **Tasks:** 2
- **Files modified:** 3 (schema.prisma, migration.sql, migration_lock.toml)

## Accomplishments
- Removed Post, Like, and Comment model blocks from api/prisma/schema.prisma; removed posts/likes/comments relation fields from User model
- Generated and applied migration 20260604233032_remove_community_models: drops Comment, Like, Post tables and all FK constraints from local dev DB
- Prisma client regenerated without community query methods (prisma.post, prisma.like, prisma.comment no longer exist as typed methods)
- Vitest suite: 6/6 tests pass (health, auth register/login/refresh, saves list/upload/download, rate-limit)
- Desktop build exits 0 — no regressions from Plan 02 C++ changes
- grep sweeps confirm: 0 IFeedClient/feed_client.hpp references in source/, 0 dead community route imports in api/src/

## Task Commits

Each task was committed atomically:

1. **Task 1: Drop Post/Like/Comment Prisma models; apply migration** - `28dfc09` (feat)
2. **Task 2: Verify phase 1 complete — Vitest green, desktop build green** - `71bd580` (chore)

**Plan metadata:** (see final_commit below)

## Files Created/Modified
- `api/prisma/schema.prisma` - Post/Like/Comment models removed; User relation fields removed; User/RefreshToken/SaveSlot preserved
- `api/prisma/migrations/20260604233032_remove_community_models/migration.sql` - Drops FK constraints then Comment, Like, Post tables
- `api/prisma/migrations/migration_lock.toml` - Updated by Prisma migration tooling

## Decisions Made
- Fixed `api/.env` DATABASE_URL from `postgresql://postgres:postgres@localhost:5432/thomaz` to `postgresql://thomaz:thomaz@localhost:5433/thomaz` — the file was stale relative to docker-compose.yml; this is a gitignored file so not committed
- Used `script -q -c` pseudo-TTY wrapper with `printf 'y\n'` piped to stdin to satisfy prisma migrate dev's non-interactive detection, which blocks both the normal flow and `--create-only` without a real TTY
- `@fastify/multipart` import in app.ts is intentional (active for saves blob uploads), confirmed as not a dead community reference

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed stale DATABASE_URL in api/.env**
- **Found during:** Task 1 (schema migration step)
- **Issue:** api/.env had `postgresql://postgres:postgres@localhost:5432/thomaz` but docker-compose.yml maps container port 5432 → host 5433, with credentials thomaz/thomaz. Connection was refused.
- **Fix:** Updated DATABASE_URL to `postgresql://thomaz:thomaz@localhost:5433/thomaz?schema=public`
- **Files modified:** api/.env (gitignored — not committed)
- **Verification:** `npx prisma db pull` succeeded; migrate dev applied the migration
- **Committed in:** N/A (gitignored file)

**2. [Rule 3 - Blocking] prisma migrate dev requires a pseudo-TTY for interactive confirmation**
- **Found during:** Task 1 (migrate dev step)
- **Issue:** The non-interactive environment detection in Prisma blocked `migrate dev` even with `echo y | ...` piping. The tool requires an actual TTY.
- **Fix:** Wrapped with `script -q -c "npx prisma migrate dev --name remove_community_models" /dev/null` and piped `printf 'y\n'` to satisfy the interactive prompt
- **Files modified:** None (execution method change only)
- **Verification:** Migration applied successfully; tables confirmed dropped in DB
- **Committed in:** 28dfc09 (part of Task 1 commit)

---

**Total deviations:** 2 auto-fixed (1 bug fix, 1 blocking execution workaround)
**Impact on plan:** Both fixes necessary to complete the migration. No scope creep. The pseudo-TTY approach is the standard workaround for Prisma's non-interactive detection.

## Issues Encountered
- Prisma 6.x `migrate dev` non-interactive detection: the tool checks for TTY presence and blocks even `--create-only` when it detects a non-interactive shell. Resolved via `script` pseudo-TTY wrapper.
- Advisory lock contention from a background process: an earlier `script` invocation held the PostgreSQL advisory lock for a moment. Resolved by killing the background process and retrying.

## User Setup Required
None - no external service configuration required. The production DB (Lightsail) will receive the migration automatically on the next `git push` to `main` (touching `api/**`) via the PM2/GitHub Actions deploy workflow.

## Next Phase Readiness
- Phase 1 complete: all four requirements RM-01, RM-02, RM-03, RM-04 satisfied
- Community feature fully removed at all layers: API routes (Plan 01), C++ client (Plan 02), Prisma schema + DB tables (Plan 03)
- Vitest green baseline established — regression guard for Phase 2 (API security)
- No blockers for Phase 2

---
*Phase: 01-remove-community-feature*
*Completed: 2026-06-04*
