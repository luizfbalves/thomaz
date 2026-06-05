---
phase: 01-remove-community-feature
plan: "01"
subsystem: api
tags: [fastify, typescript, prisma, vitest, security]

# Dependency graph
requires: []
provides:
  - "api/src/routes/posts.ts, feed.ts, users.ts deleted — community route surface gone"
  - "api/src/lib/feed-page.ts deleted"
  - "api/src/app.ts: only auth + saves routes registered; @fastify/static removed (SEC-01 fix)"
  - "api/src/lib/serializers.ts: only toSaveSlotDto remains; community DTO functions removed"
  - "api/test/api.test.ts: community tests removed; auth/saves/rate-limit tests intact"
affects:
  - 01-remove-community-feature/plan-02
  - 01-remove-community-feature/plan-03

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "@fastify/multipart retained for saves binary upload only (4 MB limit); @fastify/static removed entirely"

key-files:
  created: []
  modified:
    - api/src/app.ts
    - api/src/lib/serializers.ts
    - api/test/api.test.ts
  deleted:
    - api/src/routes/posts.ts
    - api/src/routes/feed.ts
    - api/src/routes/users.ts
    - api/src/lib/feed-page.ts

key-decisions:
  - "@fastify/multipart kept (not removed) because saves.ts uses request.parts() for binary save blob uploads; limit tightened from 16 MB to 4 MB"
  - "toUserDto removed alongside toPostDto/toCommentDto because it had no remaining callers after community code deletion"
  - "auth refresh test: /users/me assertion replaced with /saves GET (authenticated endpoint) as the token-validity check"

patterns-established:
  - "saves route is the only multipart consumer; community image upload gone with posts.ts"

requirements-completed:
  - RM-01
  - RM-02
  - RM-04

# Metrics
duration: 12min
completed: 2026-06-04
---

# Phase 01 Plan 01: Remove Community API Routes Summary

**Deleted posts/feed/users routes and @fastify/static from the Fastify API, closing SEC-01 save-blob public exposure and removing 415 lines of community-only code**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-06-04T23:10:00Z
- **Completed:** 2026-06-04T23:22:00Z
- **Tasks:** 2
- **Files modified:** 3 modified, 4 deleted

## Accomplishments

- Deleted `routes/posts.ts`, `routes/feed.ts`, `routes/users.ts`, `lib/feed-page.ts` — all community-only server-side code gone
- Removed `@fastify/static` registration from `app.ts` — save blobs at `uploads/saves/` are no longer publicly reachable (SEC-01 root-cause fix per D-04, D-10)
- Stripped `toPostDto`, `toCommentDto`, `PostWithCounts`, `toUserDto` from serializers.ts — only `toSaveSlotDto` remains
- Rewrote test suite: community describe blocks removed; auth/saves/rate-limit coverage intact; `/users/me` sub-assertion replaced with `/saves` check

## Task Commits

Each task was committed atomically:

1. **Task 1: Delete community route files and feed-page lib** - `1ce4e02` (chore)
2. **Task 2: Strip static/community registrations; clean serializers; rewrite test suite** - `09b2afa` (feat)

## Files Created/Modified

- `api/src/app.ts` - Removed @fastify/static, feedRoutes, postsRoutes, usersRoutes; kept multipart for saves with 4 MB limit
- `api/src/lib/serializers.ts` - Now exports only toSaveSlotDto; all community DTOs removed
- `api/test/api.test.ts` - Community test blocks removed; saves/auth/rate-limit tests intact
- ~~`api/src/routes/posts.ts`~~ - Deleted (D-03)
- ~~`api/src/routes/feed.ts`~~ - Deleted (D-03)
- ~~`api/src/routes/users.ts`~~ - Deleted (D-06)
- ~~`api/src/lib/feed-page.ts`~~ - Deleted (only used by deleted routes)

## Decisions Made

- **@fastify/multipart retained** — `saves.ts` uses `request.parts()` to stream binary save blobs from the C++ client. The plan targeted multipart removal to close community image upload; the saves binary upload is the only remaining multipart consumer. Limit tightened from 16 MB to 4 MB (Switch save files are much smaller).
- **toUserDto removed** — only callers were `toPostDto` and `toCommentDto`, both community-only and also removed.
- **form-data import kept** — saves test still uses `FormData` for `PUT /saves/:titleId` multipart payload.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Retained @fastify/multipart (tightened limit) because saves.ts depends on request.parts()**
- **Found during:** Task 2 (app.ts edit)
- **Issue:** Plan instructed removal of `@fastify/multipart`, but `saves.ts` uses `request.parts()` which requires the plugin. TypeScript reported `Property 'parts' does not exist` after removal.
- **Fix:** Re-added `@fastify/multipart` registration in `app.ts` with a tightened 4 MB file limit (down from 16 MB). The community image upload path (the original threat) is gone with `posts.ts`; saves use the same multipart mechanism for binary save data.
- **Files modified:** `api/src/app.ts`
- **Verification:** `npm run typecheck` exits 0
- **Committed in:** `09b2afa` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 1 — bug: multipart removal broke saves)
**Impact on plan:** SEC-01 is still closed — @fastify/static is gone (save blobs no longer publicly routable). The only remaining multipart use is the authenticated saves upload path, which is intentional. No scope creep.

## Issues Encountered

- `@fastify/multipart` removal caused TS error in `saves.ts` (`request.parts()` not available). Resolved by keeping multipart with reduced limits. See Deviations above.

## Threat Surface Scan

All threats in the plan's threat model were addressed:

| Threat | Status |
|--------|--------|
| T-01-01: @fastify/static public blob exposure | **MITIGATED** — static registration removed |
| T-01-02: @fastify/multipart image upload surface | **MITIGATED** — posts.ts deleted; only auth-gated saves upload remains |
| T-01-03: /users/:username public profile endpoint | **MITIGATED** — users.ts deleted |
| T-01-04: toPostDto imageUrl with PUBLIC_BASE_URL/uploads/ path | **MITIGATED** — toPostDto removed |

No new threat surface introduced by this plan.

## Next Phase Readiness

- Plan 02 (client-side community removal) can proceed — API surface is clean
- Plan 03 (Prisma schema migration + full Vitest run) is required before save tests can run end-to-end (DB migration hasn't run yet)
- TypeScript typecheck passes clean; Vitest needs DB/migration from Plan 03

---
*Phase: 01-remove-community-feature*
*Completed: 2026-06-04*
