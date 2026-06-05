---
phase: 04-c-activity-hardening
plan: 04
subsystem: platform
tags: [cpp, curl, cancellation, http, cloud-saves, mods]

# Dependency graph
requires:
  - phase: 04-01
    provides: ThomazActivity base class with cancelled shared_ptr + cancelledFlag() accessor
  - phase: 04-02
    provides: Activity migrations to ThomazActivity (including save_detail and mod_detail)
  - phase: 04-03
    provides: Remaining activity migrations

provides:
  - "HttpRequest.cancelled field — optional shared_ptr<atomic<bool>> (default null)"
  - "http_client_curl.cpp XFERINFOFUNCTION abort hook on request/response surface"
  - "mod_download.cpp ProgressCtx.cancelled + xferInfo early-abort"
  - "download_file signature extended with optional cancelled=nullptr last parameter"
  - "ICloudSaveClient/HttpCloudSaveClient/FakeCloudSaveClient methods extended with CancelFlag param"
  - "mod_detail_activity.cpp wires cancelledFlag() into download_file"
  - "save_detail_activity.cpp wires cancelledFlag() into all cloud-save transfers (getStatus/push/pull)"

affects: [04-05]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "XFERINFOFUNCTION cooperative abort via shared_ptr<atomic<bool>> captured by value into per-transfer context"
    - "CancelFlag typedef (shared_ptr<atomic<bool>>) as optional defaulted last parameter on transport methods"

key-files:
  created: []
  modified:
    - source/platform/http_client.hpp
    - source/platform/http_client_curl.cpp
    - source/platform/mods/mod_download.hpp
    - source/platform/mods/mod_download.cpp
    - source/platform/saves/cloud_save_client.hpp
    - source/platform/saves/http_cloud_save_client.hpp
    - source/platform/saves/http_cloud_save_client.cpp
    - source/platform/saves/fake_cloud_save_client.hpp
    - source/platform/saves/fake_cloud_save_client.cpp
    - source/app/mod_detail_activity.cpp
    - source/app/save_detail_activity.cpp

key-decisions:
  - "[04-04]: Cloud-save flag handoff via ICloudSaveClient method extension — ICloudSaveClient.getStatus/pull/push gain optional CancelFlag param (default null); HttpCloudSaveClient propagates it into req.cancelled; saves the activity from knowing about HttpRequest internals"
  - "[04-04]: CancelFlag typedef in cloud_save_client.hpp — avoids verbose shared_ptr<atomic<bool>> repetition in the interface"
  - "[04-04]: FakeCloudSaveClient signature updated to match but ignores the flag — fake never aborts; tests remain deterministic"
  - "[04-04]: CURLOPT_NOPROGRESS 0 always set in http_client_curl.cpp — enables the hook even for callers that don't set cancelled; null-guard in callback keeps happy path returning 0"

patterns-established:
  - "CONC-03 pattern: per-transfer CancelCtx/ProgressCtx carries shared_ptr copy; XFERINFOFUNCTION returns 1 only when cancelled->load(), else 0"

requirements-completed: [CONC-03]

# Metrics
duration: 25min
completed: 2026-06-05
---

# Phase 04 Plan 04: CONC-03 Curl Cancellation Summary

**Cooperative curl abort across both curl surfaces: in-flight mod downloads and cloud-save transfers abort when their activity is destroyed via shared_ptr<atomic<bool>> + CURLOPT_XFERINFOFUNCTION**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-06-05T~18:10Z
- **Completed:** 2026-06-05T~18:35Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments

- Both curl surfaces (mod_download + http_client_curl) now abort in-flight transfers when the owning activity is destroyed
- HttpRequest.cancelled added as an optional field (default null) — all existing callers unaffected
- download_file gains a defaulted cancelled=nullptr last parameter — self_update.cpp, theme_download.cpp, and the old mod_detail call site all compile unchanged
- ICloudSaveClient interface extended with CancelFlag optional param — cloud-save getStatus/pull/push all abortable
- mod_detail_activity and save_detail_activity (doUpload, pushAtRevision, doDownload) all capture cancelledFlag() by value before dispatch and thread it into the transport
- Desktop build clean: zero errors, zero new warnings (-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON)

## Task Commits

1. **Task 1: Add cancelled flag to both curl surfaces (mod_download + http_client_curl)** - `12dcd4f` (feat)
2. **Task 2: Thread cancelled flag from activities into transfers; verify clean build** - `89da4c4` (feat)

## Files Created/Modified

- `source/platform/http_client.hpp` — Added `<atomic>`, `<memory>` includes; added `std::shared_ptr<std::atomic<bool>> cancelled;` field to `HttpRequest`
- `source/platform/http_client_curl.cpp` — Added `CancelCtx` struct + `curlCancelXferInfo` callback; hooked with `CURLOPT_NOPROGRESS 0 / XFERINFOFUNCTION / XFERINFODATA` in `request()`
- `source/platform/mods/mod_download.hpp` — Added `<atomic>`, `<memory>` includes; extended `download_file` signature with `std::shared_ptr<std::atomic<bool>> cancelled = nullptr`
- `source/platform/mods/mod_download.cpp` — Added `<atomic>`, `<memory>` includes; added `cancelled` to `ProgressCtx`; early-abort at top of `xferInfo` returns 1 when `cancelled->load()`; `download_file` definition updated
- `source/platform/saves/cloud_save_client.hpp` — Added `<atomic>`, `<memory>` includes; added `CancelFlag` typedef; extended `ICloudSaveClient` virtual methods with `CancelFlag cancelled = nullptr`
- `source/platform/saves/http_cloud_save_client.hpp` — Override signatures updated to match new interface
- `source/platform/saves/http_cloud_save_client.cpp` — Each method sets `req.cancelled = cancelled` before calling `http->request(req)`
- `source/platform/saves/fake_cloud_save_client.hpp` — Override signatures updated to match new interface
- `source/platform/saves/fake_cloud_save_client.cpp` — Override signatures updated; flag accepted but ignored (fake never aborts)
- `source/app/mod_detail_activity.cpp` — `startDownload()`: captures `cancelledFlag()` by value before `runAsync`; passes as final arg to `download_file(..., cancelled)`
- `source/app/save_detail_activity.cpp` — `doUpload()`, `pushAtRevision()`, `doDownload()`: each captures `cancelledFlag()` by value before `runAsync`; threads into `getStatus`/`push`/`pull` calls

## Decisions Made

- **Cloud-save flag handoff via interface extension:** Extending `ICloudSaveClient` methods with an optional `CancelFlag` param (default null) is the minimal-churn handoff. The activity captures `cancelledFlag()` once and passes it into the client call from inside the worker. HttpCloudSaveClient propagates it to `req.cancelled`. The activity doesn't need to know about `HttpRequest` internals.
- **CancelFlag typedef:** Avoids `std::shared_ptr<std::atomic<bool>>` verbosity in the interface signatures; defined in `cloud_save_client.hpp` next to where it's used.
- **FakeCloudSaveClient ignores flag:** The fake is used in tests and offline desktop; it never aborts. Accepting but ignoring the parameter keeps the interface contract satisfied without breaking test determinism.
- **CURLOPT_NOPROGRESS 0 always set in http_client_curl:** Enabling the progress hook unconditionally is the safest option. The null-guard `if (ctx && ctx->cancelled && ctx->cancelled->load())` in the callback ensures no performance impact for existing callers.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] FakeCloudSaveClient override signatures updated to match extended interface**
- **Found during:** Task 2 (threading cancelled into save_detail_activity.cpp)
- **Issue:** When `ICloudSaveClient` virtual methods were extended with `CancelFlag cancelled = nullptr`, `FakeCloudSaveClient` became a non-conforming derived class (wrong override signatures). The plan mentioned only `HttpCloudSaveClient` + `http_cloud_save_client.cpp` — it didn't explicitly call out updating the fake.
- **Fix:** Updated `fake_cloud_save_client.hpp` override declarations and `fake_cloud_save_client.cpp` definitions to match the new interface; flag accepted but ignored.
- **Files modified:** `source/platform/saves/fake_cloud_save_client.hpp`, `source/platform/saves/fake_cloud_save_client.cpp`
- **Verification:** Desktop build compiles with zero errors.
- **Committed in:** `89da4c4` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (Rule 2 — missing critical interface conformance)
**Impact on plan:** Required for build correctness; no scope creep.

## Build Evidence

```
cmake -S . -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-desktop
```

Result: `[100%] Built target thomaz` — zero errors, zero new warnings in `source/` files.
All pre-existing warnings are from third-party libraries (`lib/switchthemes/`, SDL2).

## Existing Callers Confirmed Unchanged

The following callers pass no `cancelled` arg and are unaffected (default null → callback returns 0):

- `source/app/self_update.cpp` — calls `download_file(url, dest, nullptr, &err)` (4-arg form)
- `source/app/theme_download.cpp` — calls `download_file(url, dest, nullptr, &err)` (4-arg form)
- Any caller of `IHttpClient::get(url)` convenience method — builds `HttpRequest` with no `cancelled` field (zero-initialized to null)

## Known Stubs

None.

## Threat Flags

None. This plan adds a local cooperative abort path only — no new network input parsed, no new endpoints, no new auth paths.

## Next Phase Readiness

CONC-03 is complete. Both curl surfaces (mod_download file streaming + http_client_curl request/response) honor the cancelled flag. The two transfer-owning activities (mod_detail, save_detail) hand their base-class cancelled flag into the transport.

Phase 04 Plan 05 (remaining tests / verifier sign-off) can proceed.

## Self-Check: PASSED

All 12 modified/created files confirmed present on disk. Both task commits (12dcd4f, 89da4c4) confirmed in git log.

---
*Phase: 04-c-activity-hardening*
*Completed: 2026-06-05*
