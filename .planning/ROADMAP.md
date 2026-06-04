# Roadmap: thomaz — Hardening Milestone

## Overview

This milestone resolves every issue surfaced by the codebase audit without adding new features. Four phases deliver the fixes in dependency order: community-feature removal first (strips the root cause of the save-blob exposure and clears dead code), then API security hardening with regression tests, then isolated C++ platform fixes, then the larger C++ activity refactor. Every fix is verifiable by host tests (Vitest) or a clean desktop build (`-DUSE_SDL2=ON`); on-hardware validation is a separate manual checklist.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3, 4): Planned milestone work
- Decimal phases (X.Y): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Remove Community Feature** - Strip posts/feed/comments/likes from API and client; preserve auth/session infrastructure shared with cloud saves
- [ ] **Phase 2: API Security + Regression Tests** - Harden the live API against the remaining HIGH-severity security issues and co-ship regression tests
- [ ] **Phase 3: C++ Platform Hardening** - Fix isolated C++ platform-layer issues (fs_util extraction, TLS warning, cloudBusy atomicity) and their host tests
- [ ] **Phase 4: C++ Activity Hardening** - Refactor all activities to the runAsync base-class pattern, replace unsafe casts, add curl cancellation, and cover the conflict path

## Phase Details

### Phase 1: Remove Community Feature
**Goal**: The community feature (posts, feed, comments, likes) is entirely removed from both the API and the client; auth/session infrastructure and cloud saves remain fully functional
**Depends on**: Nothing (first phase)
**Requirements**: RM-01, RM-02, RM-03, RM-04
**Success Criteria** (what must be TRUE):
  1. `routes/posts.ts`, `routes/feed.ts`, `routes/users.ts`, and `@fastify/multipart` are gone from the API; `auth.ts`, `saves.ts`, and all account-only endpoints respond normally
  2. `@fastify/static` serving is removed; the `Post`, `Like`, and `Comment` Prisma models are dropped via a clean migration; `User`, `RefreshToken`, and `SaveSlot` remain intact
  3. Client feed code (`core/feed/feed_json`, `core/feed/feed_types`, `platform/feed/http_feed_client`, `fake_feed_client`, `feed_client.hpp`) is deleted; `core/feed/session_codec` and `platform/feed/auth_store` still exist and compile; `IFeedClient` is replaced by `IAuthClient` in all activities
  4. The API Vitest suite passes (auth and cloud-saves tests green, no dead references to removed feed/post code); a clean desktop build with `-DUSE_SDL2=ON` succeeds
**Plans**: 3 plans

Plans:
- [x] 01-01-PLAN.md — Delete community API routes (posts.ts, feed.ts, users.ts, feed-page.ts); remove @fastify/multipart and @fastify/static from app.ts; clean serializers.ts and test suite (Wave 1)
- [x] 01-02-PLAN.md — Replace IFeedClient with IAuthClient in C++ activities; migrate Session/AuthResult to session_codec.hpp; delete 8 community feed client files; verify desktop build (Wave 1, parallel)
- [ ] 01-03-PLAN.md — [BLOCKING] Drop Post/Like/Comment from schema.prisma and apply Prisma migration; run Vitest suite and final grep sweep (Wave 2)

**Planning decisions resolved:**
- `/users/me` endpoint: C++ client does NOT call `/users/me` (grep confirmed zero matches in source/). Entire users.ts deleted per D-06.
- `IFeedClient` → `IAuthClient`: The interface is renamed and moved to `source/platform/auth_client.hpp`; Session/AuthResult types move into `session_codec.hpp` (which is preserved).
- session_codec + auth_store: Preserved in place per D-09.

### Phase 2: API Security + Regression Tests
**Goal**: The live API has no remaining HIGH-severity security issues: save blobs require authentication, tokens can be revoked on logout, production logging is enabled, and regression tests guard these fixes
**Depends on**: Phase 1
**Requirements**: SEC-01, SEC-02, DEBT-04, TEST-01, TEST-02
**Success Criteria** (what must be TRUE):
  1. `GET /uploads/saves/<userId>/<titleId>.bin` (or any direct path to a save blob) returns 404; owner access via the authenticated API route returns 200; cross-user access returns 403 — no static file serving exposes blobs (TEST-01 guards this)
  2. A token used after `POST /auth/logout` returns 401; other valid tokens for the same user still return 200; pre-deploy tokens without a `jti` claim are unaffected (pass through the blocklist check)
  3. Saves `PUT` with a mismatched revision returns 400 (`revision_required`) or 409 (`revision_conflict`); a matching revision updates successfully (200); a new slot creates successfully (200) — all four branches covered by TEST-02
  4. API running in `production` environment emits pino JSON request logs; `test` environment stays silent (existing Vitest suite passes with no spurious output)
**Plans**: TBD

**Planning flags:**
- **jti scope (SEC-02):** Logout revokes only the current access token via `jti` claim + Postgres `RevokedToken` table. Refresh tokens are already DB-backed via `revokeRefreshToken` — no change there. Tokens minted before deploy (without `jti`) must pass the blocklist check unblocked, not be rejected.
- **SEC-01 verify-only:** The fix for SEC-01 (removing `@fastify/static`) landed in Phase 1. Phase 2 provides the verification layer: TEST-01 asserts that no static route exposes save blobs. The SEC-01 requirement is assigned here because the regression guard is what Phase 2 delivers.
- **No Redis:** Token blocklist uses Postgres `RevokedToken` table. `findUnique` on primary-key `jti` is sub-millisecond; no new infrastructure dependency.

### Phase 3: C++ Platform Hardening
**Goal**: Duplicated filesystem helpers are consolidated into a shared `fs_util` platform utility, the TLS fail-safe shows a visible on-screen warning, `cloudBusy` is `std::atomic<bool>`, and all three are verified by a clean desktop build and host tests
**Depends on**: Phase 2
**Requirements**: SEC-03, CONC-01, DEBT-01, DEBT-02, TEST-03
**Success Criteria** (what must be TRUE):
  1. `source/platform/fs_util.hpp` and `fs_util.cpp` exist; `ensure_parent_dirs` and `copy_tree` are defined there; all previously duplicated copies are removed from their original call sites
  2. `save_detail_activity.hpp` declares `cloudBusy` as `std::atomic<bool>{false}`; all read/write sites use `load()`/`store()` or `compare_exchange_strong`
  3. When the CA bundle probe fails (`ca_ok == false`), a visible warning is emitted via `brls::Logger::warning` (or `brls::Application::notify`); no `CURLOPT_SSL_VERIFYPEER` lines appear outside `#ifdef __SWITCH__`
  4. A doctest covering the TLS fail-safe branch (`ca_ok == false`) passes in the host test suite (TEST-03)
  5. Desktop build with `-DUSE_SDL2=ON` compiles clean with zero errors and zero new warnings
**Plans**: TBD

**Planning flags:**
- **Second `copy_tree` location (DEBT-02):** Research could not locate `save_service_switch.cpp` at the expected path. Verify at Phase 3 implementation start. If absent, DEBT-02 removes only the `mod_store.cpp` copy — still satisfies the requirement.
- **`ensure_parent_dirs` edge case (DEBT-01):** `theme_install.cpp` uses a character-by-character loop variant. Write a doctest for a representative theme path (e.g., `romfs:/themes/a/b/c`) before removing the local copy, confirming the canonical substring-at-slash form is equivalent.
- **S1 serialize constraint:** DEBT-01 and DEBT-02 share `fs_util.hpp`/`.cpp` — do them in a single commit.
- **S2 constraint (cross-phase):** CONC-01 (atomicize `cloudBusy` in `save_detail_activity.hpp`) must land before Phase 4's CONC-02, which removes the `alive` member from the same header.

### Phase 4: C++ Activity Hardening
**Goal**: All activities inherit the `ThomazActivity` base class with its `runAsync` wrapper (making the `alive` guard impossible to forget), unsafe C-style view casts are null-guarded, in-flight curl requests cancel on activity destruction, and the conflict-resolution path is covered by a host test
**Depends on**: Phase 3
**Requirements**: CONC-02, CONC-03, DEBT-03, TEST-04
**Success Criteria** (what must be TRUE):
  1. `source/app/thomaz_activity.hpp` defines a `ThomazActivity` base class with a protected `runAsync(worker, onSync)` method that auto-captures `alive` by value before dispatch; all activities that previously used `brls::async` directly now call `this->runAsync(...)` instead
  2. The flagged activities (`game_list`, `save_manager`, `save_detail`, `mod_browser`) use null-guarded `dynamic_cast` (or `brls::View::cast<T>()` if available in vendored Borealis) in place of every C-style `(T*)this->getView(...)` cast; a null result is handled safely (log + return)
  3. In-flight curl requests abort when their activity is destroyed — the shared `cancelled` flag is set in the destructor, and the `CURLOPT_XFERINFOFUNCTION` callback returns 1; happy-path requests complete normally
  4. A host doctest covers the cloud-save conflict-resolution / `plan_push` branch (TEST-04)
  5. Desktop build with `-DUSE_SDL2=ON` compiles clean with zero errors and zero new warnings after all activity inheritance changes
**Plans**: TBD

**Planning flags:**
- **`brls::View::cast<T>()` existence (DEBT-03):** Check `lib/borealis/library/include/borealis/core/view.hpp` before Phase 4 implementation. If the method is absent, `dynamic_cast` + null guard is the correct replacement in all cases.
- **CONC-03 after CONC-02:** The curl cancellation flag (CONC-03) relies on the ownership model established by the `runAsync` wrapper (CONC-02). Implement CONC-02 first.
- **DEBT-03 / CONC-02 shared files:** Both touch the four activity `.cpp` files at different lines. Serialize in one PR or coordinate carefully to avoid merge conflicts.
- **S2 constraint:** CONC-02 removes the `alive` member from `save_detail_activity.hpp` — requires Phase 3's CONC-01 to have already atomicized `cloudBusy` in that same header.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Remove Community Feature | 2/3 | In Progress|  |
| 2. API Security + Regression Tests | 0/? | Not started | - |
| 3. C++ Platform Hardening | 0/? | Not started | - |
| 4. C++ Activity Hardening | 0/? | Not started | - |
