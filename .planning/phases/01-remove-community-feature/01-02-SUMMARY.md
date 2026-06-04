---
phase: 01-remove-community-feature
plan: "02"
subsystem: client-cpp
tags: [refactor, cleanup, feed-removal, auth, interface]
dependency_graph:
  requires: [01-01]
  provides: [IAuthClient interface, desktop build green, feed files deleted]
  affects: [source/app/, source/platform/, source/core/feed/]
tech_stack:
  added: []
  patterns: [clean-architecture, platform-interface-swap]
key_files:
  created:
    - source/platform/auth_client.hpp
    - source/platform/http_auth_client.hpp
    - source/platform/http_auth_client.cpp
    - source/platform/fake_auth_client.hpp
    - source/platform/fake_auth_client.cpp
  modified:
    - source/core/feed/session_codec.hpp
    - source/platform/feed/auth_store.hpp
    - source/main.cpp
    - source/app/auth_activity.hpp
    - source/app/auth_activity.cpp
    - source/app/home_activity.hpp
    - source/app/home_activity.cpp
    - source/app/save_manager_activity.hpp
    - source/app/save_manager_activity.cpp
    - source/app/save_detail_activity.hpp
    - source/app/save_detail_activity.cpp
  deleted:
    - source/core/feed/feed_json.cpp
    - source/core/feed/feed_json.hpp
    - source/core/feed/feed_types.hpp
    - source/platform/feed/http_feed_client.cpp
    - source/platform/feed/http_feed_client.hpp
    - source/platform/feed/fake_feed_client.cpp
    - source/platform/feed/fake_feed_client.hpp
    - source/platform/feed/feed_client.hpp
decisions:
  - "JSON helpers (build_credentials_body, parse_auth_response) inlined into http_auth_client.cpp rather than kept in a separate core header, because feed_json.{cpp,hpp} was being deleted and the logic is auth-only"
  - "Session and AuthResult moved to session_codec.hpp (not a new file) to minimize the number of headers callers must include"
metrics:
  duration_seconds: 308
  completed_date: "2026-06-04"
  tasks_completed: 2
  files_created: 5
  files_modified: 11
  files_deleted: 8
---

# Phase 01 Plan 02: Replace IFeedClient with IAuthClient; Delete Community Feed Client Files Summary

**One-liner:** IAuthClient interface extracted from IFeedClient; 8 community-feed client files deleted; desktop build green with zero errors.

## What Was Done

This plan executed the client-side half of the community feed removal (D-08, D-09). The challenge was that `IFeedClient` was the compile-time type threaded through the entire activity chain for auth. The type had to be replaced with a clean `IAuthClient` before the feed files could be deleted without breaking the build.

### Task 1: Migrate types and create IAuthClient (commit b285627)

- **session_codec.hpp** — embedded `Session` and `AuthResult` struct definitions directly (previously they were in `feed_types.hpp`); removed the `#include "core/feed/feed_types.hpp"` line.
- **auth_store.hpp** — switched from `#include "core/feed/feed_types.hpp"` to `#include "core/feed/session_codec.hpp"`, which now defines `Session`.
- Created **platform/auth_client.hpp** — `IAuthClient` interface in `namespace thomaz`, with `registerUser` and `login` pure virtuals; re-exports `feed::AuthResult` unqualified via `using feed::AuthResult`.
- Created **platform/http_auth_client.{hpp,cpp}** — `HttpAuthClient : IAuthClient`; identical constructor signature and HTTP paths (`/auth/login`, `/auth/register`) as the deleted `HttpFeedClient`; JSON helpers inlined from the deleted `feed_json.cpp`.
- Created **platform/fake_auth_client.{hpp,cpp}** — `FakeAuthClient : IAuthClient`; deterministic in-memory stub ported from `FakeFeedClient`.

### Task 2: Wire IAuthClient, delete 8 feed files, verify build (commit c0354b0)

Updated every call site in the activity chain:

| File | Change |
|------|--------|
| source/main.cpp | `#include "platform/http_auth_client.hpp"`, `HttpFeedClient` → `HttpAuthClient` |
| source/app/auth_activity.hpp | `IFeedClient*` → `IAuthClient*`; include path updated |
| source/app/auth_activity.cpp | Constructor and local variable type updated |
| source/app/home_activity.hpp | `IFeedClient*` member and parameter → `IAuthClient*` |
| source/app/home_activity.cpp | Constructor signature updated |
| source/app/save_manager_activity.hpp | `IFeedClient*` member and parameter → `IAuthClient*` |
| source/app/save_manager_activity.cpp | Constructor and local variable type updated |
| source/app/save_detail_activity.hpp | `IFeedClient*` member and parameter → `IAuthClient*` |
| source/app/save_detail_activity.cpp | Constructor signature updated |

Deleted 8 community-feed files:
- `source/core/feed/feed_json.{cpp,hpp}`
- `source/core/feed/feed_types.hpp`
- `source/platform/feed/http_feed_client.{cpp,hpp}`
- `source/platform/feed/fake_feed_client.{cpp,hpp}`
- `source/platform/feed/feed_client.hpp`

Preserved (untouched):
- `source/core/feed/session_codec.{cpp,hpp}`
- `source/platform/feed/auth_store.{cpp,hpp}`

Desktop build result: `[100%] Built target thomaz` — zero errors, binary at `build-desktop/thomaz` (71 MB).

## Verification Results

| Check | Result |
|-------|--------|
| `grep -r "IFeedClient" source/` | 0 matches |
| `grep -r "feed_client.hpp" source/` | 0 matches |
| `grep -r "feed_types.hpp" source/` | 0 matches |
| `ls source/core/feed/` | session_codec.cpp, session_codec.hpp (2 files only) |
| `ls source/platform/feed/` | auth_store.cpp, auth_store.hpp (2 files only) |
| `cmake --build build-desktop` | exit 0, binary produced |

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing functionality] Inlined JSON helpers into http_auth_client.cpp**

- **Found during:** Task 1
- **Issue:** `http_feed_client.cpp` called `feed::build_credentials_body` and `feed::parse_auth_response` from `feed_json.hpp/cpp`. Since `feed_json.{cpp,hpp}` were scheduled for deletion in Task 2, the new `http_auth_client.cpp` could not include them. The plan said "copy the implementation verbatim" but did not address the missing JSON dependency.
- **Fix:** Inlined both JSON helper functions as file-scope anonymous-namespace functions inside `http_auth_client.cpp`. The logic is identical to `feed_json.cpp`.
- **Files modified:** source/platform/http_auth_client.cpp
- **Commit:** b285627

## Known Stubs

None. All new auth client implementations are functional (HttpAuthClient makes real HTTP calls; FakeAuthClient is an intentional desktop-only stub, not a production placeholder).

## Threat Flags

None. The rename from IFeedClient to IAuthClient is a compile-time refactor only. HTTP payloads, JWT handling, and session storage are unchanged. No new network endpoints or trust boundaries were introduced.

## Self-Check: PASSED

- source/platform/auth_client.hpp — FOUND
- source/platform/http_auth_client.hpp — FOUND
- source/platform/http_auth_client.cpp — FOUND
- source/platform/fake_auth_client.hpp — FOUND
- source/platform/fake_auth_client.cpp — FOUND
- build-desktop/thomaz — FOUND
- Task 1 commit b285627 — FOUND
- Task 2 commit c0354b0 — FOUND
- grep IFeedClient source/ — 0 matches
- ls source/core/feed/ — 2 files (session_codec.cpp, session_codec.hpp)
- ls source/platform/feed/ — 2 files (auth_store.cpp, auth_store.hpp)
