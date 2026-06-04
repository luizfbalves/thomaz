---
phase: 01-remove-community-feature
reviewed: 2026-06-04T00:00:00Z
depth: standard
files_reviewed: 16
files_reviewed_list:
  - api/prisma/migrations/20260604233032_remove_community_models/migration.sql
  - api/prisma/schema.prisma
  - api/src/app.ts
  - api/src/lib/serializers.ts
  - api/test/api.test.ts
  - source/app/auth_activity.cpp
  - source/app/auth_activity.hpp
  - source/app/home_activity.cpp
  - source/app/home_activity.hpp
  - source/app/save_detail_activity.cpp
  - source/app/save_detail_activity.hpp
  - source/app/save_manager_activity.cpp
  - source/app/save_manager_activity.hpp
  - source/core/feed/session_codec.hpp
  - source/main.cpp
  - source/platform/auth_client.hpp
  - source/platform/feed/auth_store.hpp
findings:
  critical: 0
  warning: 1
  info: 3
  total: 4
status: issues_found
---

# Phase 01: Code Review Report

**Reviewed:** 2026-06-04
**Depth:** standard
**Files Reviewed:** 16 (plus 4 supporting files read for cross-reference)
**Status:** issues_found (1 Warning, 3 Info — no Critical)

## Summary

This phase removed the community/feed feature across three layers. The diff is overwhelmingly deletion; I focused on the modified/added files where regressions hide.

The core refactors are sound:

- **IFeedClient -> IAuthClient swap (Plan 02)** is a clean compile-time rename. The interface, both concrete clients (`HttpAuthClient`, `FakeAuthClient`), the session/`AuthResult` types moved into `session_codec.hpp`, and every call site in the activity chain (`main.cpp` -> `HomeActivity` -> `SaveManagerActivity` -> `SaveDetailActivity` -> `AuthActivity`) are type-consistent. The HTTP payload, JSON parsing, JWT/session handling, and `onSessionChanged` persistence are byte-for-byte preserved from the deleted `HttpFeedClient`. `grep` confirms **zero** dangling references to `IFeedClient`, `feed_client.hpp`, `feed_types.hpp`, `feed_json`, `HttpFeedClient`, or `FakeFeedClient`. The preserved auth landmines (`session_codec.*`, `auth_store.*`) are intact and their include paths are correct. The async/lifetime guard pattern (`std::shared_ptr<std::atomic_bool> alive` checked inside `brls::sync` before touching `this` or any captured view pointer) is preserved correctly in every activity.

- **app.ts (Plan 01)** correctly removed `@fastify/static` (the SEC-01 root cause — save blobs under `uploads/saves/` are no longer publicly routable) and the feed/posts/users route registrations. `@fastify/multipart` is correctly retained for the authenticated `PUT /saves/:titleId` upload path, registered *before* the saves routes that consume it, with a tightened 4 MB limit. No broken routes, no leftover feed wiring.

- **Prisma schema + migration (Plan 03)** drop exactly the 5 community FK constraints and the 3 community tables (Comment, Like, Post). Cross-checked against the init migration: all 5 FKs (`Post_authorId`, `Like_userId`, `Like_postId`, `Comment_postId`, `Comment_authorId`) are accounted for; the two community indexes drop automatically with their tables. `User`, `RefreshToken`, and `SaveSlot` and their relations are untouched. No collateral damage.

- **serializers.ts** correctly retains only `toSaveSlotDto`; all community DTOs are gone. **api.test.ts** retains health/auth/saves/rate-limit coverage with a sound logout-rotation flow and a valid token-validity check via `GET /saves`.

The one Warning is config drift: a now-orphaned required env var. The three Info items are non-blocking dead-code / consistency notes.

## Warnings

### WR-01: `PUBLIC_BASE_URL` is now a required env var with zero runtime consumers

**File:** `api/src/config.ts:8` (consequence of `api/src/lib/serializers.ts` change; also referenced at `api/test/api.test.ts:20`)
**Issue:** `PUBLIC_BASE_URL` was consumed only by the removed `toPostDto` (to build `imageUrl` from `PUBLIC_BASE_URL/uploads/...`). After this phase, `grep -rn PUBLIC_BASE_URL api/ --include=*.ts` finds it in exactly two places: the Zod schema declaration (`config.ts`) and the test setup (`api.test.ts`). It has **no runtime reader**. Because it is declared as `z.string().url()` with **no `.default()`**, `loadConfig()` will throw at startup if the production environment does not set it. This is now a required-but-dead config knob: it does nothing functionally, yet a deploy that trims it (reasonable, since it appears unused) will fail config validation and crash the API on boot. It is also misleading to future maintainers who will assume it is load-bearing.
**Fix:** Remove the field from the config schema now that no code reads it:
```ts
// api/src/config.ts — delete this line:
PUBLIC_BASE_URL: z.string().url(),
```
And drop the corresponding `process.env.PUBLIC_BASE_URL = ...` setup line from `api/test/api.test.ts:20`. If a future phase needs a public base URL, reintroduce it scoped to its consumer.
**Note:** `config.ts` was not in this phase's explicit modified-files list, but the orphaning is a direct consequence of the reviewed `serializers.ts` change (removal of `toPostDto`), so it belongs in this review.

## Info

### IN-01: `FakeAuthClient` is compiled but never instantiated (dead code)

**File:** `source/platform/fake_auth_client.cpp` / `source/platform/fake_auth_client.hpp`
**Issue:** `main.cpp` constructs `thomaz::HttpAuthClient` on **both** Switch and desktop (lines 96–100; there is no `#ifdef __SWITCH__` branch selecting the fake). No test references `FakeAuthClient` either. The file is pulled into the build via `file(GLOB_RECURSE MAIN_SRC source/*.cpp)` in `CMakeLists.txt`, so it compiles, but it has zero callers. This is **not a regression**: the deleted `FakeFeedClient` had the identical status (the pre-refactor `main.cpp` at HEAD~6 also used `HttpFeedClient` unconditionally), so the dead-stub pattern was carried over faithfully rather than introduced here.
**Fix:** Either wire `FakeAuthClient` into the desktop path (`#ifndef __SWITCH__ -> std::make_unique<FakeAuthClient>()`) so the desktop build does not require a live API for login, or delete `fake_auth_client.{cpp,hpp}` if a real-HTTP desktop client is intended. Low priority; document the intent either way.

### IN-02: `IAuthClient*` members and parameters are still named `feed`

**File:** `source/app/home_activity.hpp:33` (`IAuthClient* feed;`), `source/app/save_manager_activity.hpp:34`, `source/app/save_detail_activity.hpp:51`, plus the matching constructor parameters (e.g. `home_activity.hpp:23` `IAuthClient* feed`).
**Issue:** The type was renamed from `IFeedClient` to `IAuthClient`, but the member/parameter identifier remained `feed`. The name now misdescribes its role (it carries the auth client, not a feed). This is a readability/consistency wart, not a bug — wiring is correct and the value flows to the right place (`SaveManagerActivity` -> `SaveDetailActivity` -> `AuthActivity`).
**Fix:** Rename `feed` -> `authClient` (or `auth`) across these three activities and `main.cpp:96`'s `feedClient` local for clarity. Pure rename; no behavior change. Defer if churn is unwanted.

### IN-03: Hardcoded Portuguese error strings in `http_auth_client.cpp` bypass i18n

**File:** `source/platform/http_auth_client.cpp:45-53` (`auth_error_message`)
**Issue:** `auth_error_message` returns hardcoded `pt-BR` literals ("Sem conexão com o servidor.", "Usuário ou senha inválidos.", etc.) rather than i18n keys, while the rest of the UI (`auth_activity.cpp`) uses `_i18n`. `AuthActivity::submit` displays `r.error` directly when non-empty (`auth_activity.cpp:104`), so these raw strings reach the screen untranslated. This was inlined verbatim from the deleted `feed_json.cpp` and is **not introduced by this phase** (preserved behavior), but it is worth recording as tech debt surfaced during the move.
**Fix:** Return stable i18n keys (e.g. `"thomaz/auth/err_network"`) from `auth_error_message` and resolve them in the activity, or document that these are intentionally non-localized. Out of scope for a removal phase; flag for a later cleanup.

---

_Reviewed: 2026-06-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
