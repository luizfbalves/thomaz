# Phase 1: Remove Community Feature - Context

**Gathered:** 2026-06-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove the community feature (posts, feed, comments, likes) in its entirety — from the API, the client, and the database — while preserving auth/session and cloud-save functionality. Removing the image-upload path (`@fastify/static` + `@fastify/multipart`) also resolves the SEC-01 save-blob public-exposure issue at its root.

**In scope:** deleting community code/models/endpoints/plugins; confirming nothing else depends on them; keeping the build and existing API tests green.
**Not in scope:** the security/concurrency/tech-debt fixes (Phases 2–4); re-designing auth; relocating the preserved auth files (optional, not required).
</domain>

<decisions>
## Implementation Decisions

### Scope of removal
- **D-01:** Remove the **entire** community feature — posts, feed, comments, and likes. Not a partial/read-only keep.
- **D-02:** Removal runs as the **first phase**, before all hardening work, because it obsoletes two planned fixes (VAL-01/VAL-02) and resolves SEC-01's root cause.

### Server (API) — remove
- **D-03:** Delete `api/src/routes/posts.ts` and `api/src/routes/feed.ts`.
- **D-04:** Remove the `@fastify/multipart` plugin and the `@fastify/static` plugin registration from `api/src/app.ts` (both exist only to serve post images). Removing `@fastify/static` is what closes SEC-01 — with nothing served statically, save blobs at `uploads/saves/` are no longer publicly reachable.
- **D-05:** Drop the `Post`, `Like`, and `Comment` Prisma models from `api/prisma/schema.prisma` with a clean migration. Keep `User`, `RefreshToken`, `SaveSlot`.
- **D-06:** `api/src/routes/users.ts` is mostly community (`/users/:username`, feed pages via `fetchFeedPage`). Remove the community endpoints. **Planning flag:** check whether the client calls `/users/me` (account endpoint) before deciding to keep or drop it — keep only if used.

### Server (API) — keep
- **D-07:** Keep `auth.ts`, `saves.ts`, and account-only auth/session machinery. Auth and cloud saves must remain fully functional.

### Client — remove
- **D-08:** Delete client community-feed code: `source/core/feed/feed_json.{cpp,hpp}`, `source/core/feed/feed_types.hpp`, `source/platform/feed/http_feed_client.{cpp,hpp}`, `source/platform/feed/fake_feed_client.{cpp,hpp}`, `source/platform/feed/feed_client.hpp` (IFeedClient). No feed activities exist in `source/app/` (already removed).

### Client — KEEP (landmine — these live under feed/ dirs but are auth/session)
- **D-09:** **Preserve** `source/core/feed/session_codec.{cpp,hpp}` and `source/platform/feed/auth_store.{cpp,hpp}`. They are auth/session infrastructure used by cloud saves and auth (`cloud_save_client`, `http_cloud_save_client`, `save_detail_activity`, `auth_activity`, `app_header`, `settings_activity`, `main.cpp`). Removing them would break login and cloud saves. Relocating them out of the `feed/` directories is optional and NOT required for this phase.

### Save-blob exposure (SEC-01 interaction)
- **D-10:** **No save-blob directory migration.** Earlier discussion considered moving blobs to a `SAVES_DIR`; that is unnecessary once `@fastify/static` is removed (nothing is served statically). Blobs stay in `uploads/saves/`. SEC-01 is resolved here; Phase 2 adds TEST-01 as the regression guard.

### Verification
- **D-11:** Phase is done when: the API Vitest suite passes (auth + saves intact, no references to removed code), a clean desktop build (`-DUSE_SDL2=ON`) succeeds, and there are no dead references to removed feed/post symbols. Hardware check is a separate manual checklist.

### Claude's Discretion
- Order of removal (DB migration vs route deletion vs client deletion) and commit granularity — planner/executor decide, as long as the build/tests stay green at each step.
- Whether to also remove now-unused dependencies from `api/package.json` (`@fastify/static`, `@fastify/multipart`) — recommended if nothing else uses them.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scope & decisions
- `.planning/PROJECT.md` — milestone scope; community removal in Active + Key Decisions
- `.planning/REQUIREMENTS.md` §Community Feature Removal — RM-01..RM-04 acceptance text; VAL-01/VAL-02 now Out of Scope
- `.planning/ROADMAP.md` §Phase 1 — goal, success criteria, planning flags

### Codebase map
- `.planning/codebase/ARCHITECTURE.md` — feed/auth wiring, IFeedClient, error-handling model
- `.planning/codebase/STRUCTURE.md` — where feed/posts code lives; "where to add/remove code"
- `.planning/codebase/CONCERNS.md` — SEC-01 (save-blob static exposure) root cause

### Files in play (verified by scan 2026-06-04)
- API remove: `api/src/routes/posts.ts`, `api/src/routes/feed.ts`, community parts of `api/src/routes/users.ts`, `@fastify/multipart` + `@fastify/static` in `api/src/app.ts`, `Post`/`Like`/`Comment` in `api/prisma/schema.prisma`
- Client remove: `source/core/feed/feed_json.*`, `source/core/feed/feed_types.hpp`, `source/platform/feed/http_feed_client.*`, `fake_feed_client.*`, `feed_client.hpp`
- Client KEEP: `source/core/feed/session_codec.*`, `source/platform/feed/auth_store.*`

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- Existing API Vitest scaffold (`app.inject()`) — use to assert auth/saves endpoints still work after removal.
- `SaveServiceFake` / fake clients and the host doctest suite — desktop build path validates client compiles without feed code.

### Established Patterns
- Clean architecture: removing `core/feed` (community) must not touch `core/saves` or auth. `IFeedClient` is injected; confirm `main.cpp` no longer constructs/wires it (scan found no FeedClient wiring in `main.cpp` — likely already unwired/dead).
- API routes are registered per-file in `app.ts` (`usersRoutes`, etc.); removal = drop the import + `register` call.

### Integration Points
- `api/src/app.ts` — plugin/route registration (multipart, static, feed/posts/users routes) is the central removal site.
- `api/prisma/schema.prisma` + `migrations/` — model removal needs a generated migration; verify FK relations from `Post`/`Like`/`Comment` to `User` are dropped cleanly.
- `main.cpp` — confirm no dangling construction of removed feed clients.

</code_context>

<specifics>
## Specific Ideas

- The whole motivation: "não temos mais função de post, na verdade deve ser removido todo código relacionado." Treat this as a clean deletion, not a deprecation.
- Bonus outcome valued by the user: removing `@fastify/static` doubles as the SEC-01 fix — call this out so Phase 2 only needs to *verify* (TEST-01), not re-implement.

</specifics>

<deferred>
## Deferred Ideas

- **Logout revocation scope (token-only; tokens without `jti` pass through):** decided during this discussion but belongs to **Phase 2 / SEC-02**, not removal. Captured in PROJECT.md Key Decisions so Phase 2 inherits it.
- **Relocating `session_codec` / `auth_store` out of `feed/` directories** into an `auth/`-named location — a tidiness refactor, not required now; consider in a later cleanup if desired.

</deferred>

---

*Phase: 1-Remove Community Feature*
*Context gathered: 2026-06-04*
