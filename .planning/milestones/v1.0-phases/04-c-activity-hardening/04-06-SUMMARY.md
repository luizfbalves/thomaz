---
phase: 04-c-activity-hardening
plan: "06"
subsystem: platform/themes + app/activities
tags: [cancellation, concurrency, conc-03, d-03, wr-02, theme-download, browse-gets]
dependency_graph:
  requires: [04-04]
  provides: [CONC-03-complete, D-03-satisfied]
  affects: [theme_download, theme_detail_activity, theme_browser_activity, mod_browser_activity, cheat_detail_activity, game_list_activity, http_client_curl, mod_download]
tech_stack:
  added: []
  patterns:
    - "cancelledFlag() captured BY VALUE (shared_ptr copy) before runAsync dispatch"
    - "makeFetcher extended with optional cancelled param; sets req.cancelled inside lambda"
    - "CURLE_ABORTED_BY_CALLBACK handled silently in both curl surfaces (WR-02)"
key_files:
  created: []
  modified:
    - source/platform/themes/theme_download.hpp
    - source/platform/themes/theme_download.cpp
    - source/app/theme_detail_activity.cpp
    - source/app/theme_browser_activity.cpp
    - source/app/mod_browser_activity.cpp
    - source/app/cheat_detail_activity.cpp
    - source/app/game_list_activity.cpp
    - source/platform/http_client_curl.cpp
    - source/platform/mods/mod_download.cpp
decisions:
  - "makeFetcher in theme_detail_activity.cpp and theme_browser_activity.cpp are separate modified copies (not a shared helper) — each file's local anonymous-namespace copy extended independently with cancelled param; low churn, no header change needed"
  - "UrlFetcher lambdas in mod_browser/cheat_detail/game_list use explicit HttpRequest with req.cancelled set (replacing http->get(url) with client->request(req)) — same UrlFetcher signature, body change only"
  - "WR-02 applied (within 4-line budget): CURLE_ABORTED_BY_CALLBACK handled in mod_download.cpp before generic rc!=CURLE_OK branch; http_client_curl.cpp clears partial body on abort"
metrics:
  duration: "~10 minutes"
  completed: "2026-06-05T18:45:00Z"
  tasks: 4
  files: 9
---

# Phase 04 Plan 06: Gap-Closure CONC-03 D-03 Summary

Wire the `cancelled` flag into all five remaining activity-owned network transfer sites (themes and browse GETs), completing D-03 full cancellation scope — every transfer aborts in-flight when its owning activity is destroyed.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Extend download_theme + wire theme_detail + theme_browser | 079a727 | theme_download.hpp, theme_download.cpp, theme_detail_activity.cpp, theme_browser_activity.cpp |
| 2 | Wire mod_browser, cheat_detail, game_list browse GETs | 36992e0 | mod_browser_activity.cpp, cheat_detail_activity.cpp, game_list_activity.cpp |
| 3 | WR-02: silent cooperative-abort in http_client_curl + mod_download | 5627246 | http_client_curl.cpp, mod_download.cpp |
| 4 | Desktop build + host tests + grep sweep (verification only) | — | (no changes) |

## What Was Built

### Task 1: Theme Layer Cancellation

**theme_download.hpp/cpp**: Extended `download_theme` signature with optional `std::shared_ptr<std::atomic<bool>> cancelled = nullptr`. The download loop now calls `download_file(part.download_url, dest, nullptr, &err, cancelled)` — the 5-arg form. Existing callers that omit the second arg are unaffected (defaulted).

**theme_detail_activity.cpp**: Three runAsync sites wired:
- Preview fetch: `auto cancelled = this->cancelledFlag();` captured before `runAsync`; worker uses explicit `HttpRequest` with `req.cancelled = cancelled` instead of `client->get(url)`.
- Detail-resolve: `cancelled` captured before `runAsync`; the file-local `makeFetcher` was extended to accept `cancelled` and set `req.cancelled` on the POST request. Worker calls `makeFetcher(client, cancelled)`.
- startDownload: `cancelled` captured before `runAsync`; worker calls `download_theme(d, cancelled)` (new 2-arg form).

**theme_browser_activity.cpp**: Two runAsync sites wired:
- runQuery: `cancelled` captured before `runAsync`; file-local `makeFetcher` extended to accept `cancelled` and set `req.cancelled`. Worker calls `makeFetcher(client, cancelled)`.
- loadThumb: `cancelled` captured before `runAsync`; worker uses explicit `HttpRequest` with `req.cancelled = cancelled` replacing `client->get(u)`.

Both `makeFetcher` functions are separate anonymous-namespace copies in their respective files — they were extended independently (not merged into a shared helper). Low churn, no header changes needed.

### Task 2: Browse-GET Cancellation

Pattern applied identically across three files: `auto cancelled = this->cancelledFlag();` captured BY VALUE before each `runAsync`, then the `UrlFetcher` lambda is modified to build an explicit `HttpRequest` (setting `req.cancelled = cancelled`) and call `http->request(req)` / `client->request(req)` instead of `->get(url)`. The `UrlFetcher` type signature is unchanged; only the body changes.

- **mod_browser_activity.cpp**: Three runAsync sites (onContentAvailable, runGameSearch, runGlobalSearch) — all three UrlFetcher lambdas updated.
- **cheat_detail_activity.cpp**: One runAsync site (onContentAvailable) — UrlFetcher lambda updated. The `r.status == 0` / `r.ok()` response checks are unchanged (they still work with `client->request()`).
- **game_list_activity.cpp**: One runAsync site (loadCheatIndexAsync) — `client->get(core::db_index_url())` replaced with explicit `HttpRequest` + `req.cancelled`.

### Task 3: WR-02 Cooperative-Abort Silencing

Applied within the 4-line budget (3 lines total across both files):

**http_client_curl.cpp**: Added `else if (rc == CURLE_ABORTED_BY_CALLBACK) { response.body.clear(); }` branch before the generic `else { response.status = 0; }` block. Ensures any partial response body accumulated during a cooperative abort is discarded; `status` stays 0.

**mod_download.cpp**: Added `if (rc == CURLE_ABORTED_BY_CALLBACK) { /* leave *err empty */ }` as the first branch inside the `if (!ok)` / `if (err)` block — BEFORE the `else if (rc != CURLE_OK) *err = curl_easy_strerror(rc)` branch. Cooperative teardown aborts no longer populate `*err` with a curl error string that could reach a toast.

### Task 4: Verification (No Changes)

All verification steps passed:
- Grep sweep: all 7 gap files > 0 occurrences of `cancelled` (counts: hpp=3, cpp=2, theme_detail=13, theme_browser=9, mod_browser=12, cheat_detail=4, game_list=3)
- Desktop build: `cmake -S . -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Debug` → `[100%] Built target thomaz`, zero errors, zero warnings in source/
- Host tests: `./tests/run` → 185 test cases / 552 assertions, all pass, exit 0
- self_update.cpp: `grep -c "download_theme"` returns 0 — unaffected (calls download_file directly with 4-arg form)

## Deviations from Plan

None — plan executed exactly as written. WR-02 (Task 3) was applied (not skipped); the change totaled 3 lines across both files, within the 4-line budget.

## Architecture: makeFetcher Approach

Both `theme_detail_activity.cpp` and `theme_browser_activity.cpp` had file-local anonymous-namespace `makeFetcher` functions with the same GraphQL POST pattern. The plan offered two options: (a) extend each separately, or (b) create a shared helper. **Option (a) was chosen** — each file's local copy was extended in-place. Rationale: no shared header was needed (both are anonymous-namespace), zero additional coupling, and the plan explicitly labeled the separate-copies approach as "low-churn."

## D-03 Completion

With this plan, D-03 is fully satisfied:
- **04-04** covered: mod download (mod_detail_activity), cloud-save transfers (save_detail_activity), and the curl transport infrastructure (XFERINFOFUNCTION hook + cancelledFlag() accessor)
- **04-06** covers: theme download (theme_detail_activity startDownload + theme_download.hpp/cpp), theme preview/detail-resolve GETs (theme_detail_activity), theme browse GETs (theme_browser_activity), mod browse GETs (mod_browser_activity × 3), cheat detail GETs (cheat_detail_activity), cheat index GET (game_list_activity)

Every activity-owned network transfer across the application now aborts in-flight when the owning activity is destroyed — CONC-03 requirement status: **SATISFIED**.

## Build Evidence

```
cmake -S . -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Debug -Wno-dev
cmake --build build-desktop
# [100%] Built target thomaz — zero errors, zero new warnings in source/
./tests/run
# [doctest] test cases: 185 | 185 passed | 0 failed | 0 skipped
# [doctest] assertions: 552 | 552 passed | 0 failed
# [doctest] Status: SUCCESS!
```

## Known Stubs

None.

## Threat Flags

None — no new network endpoints, auth paths, or trust boundaries introduced. All changes route existing transfers through the pre-existing XFERINFOFUNCTION abort hook (04-04 delivery).

## Self-Check: PASSED

- source/platform/themes/theme_download.hpp — FOUND (modified)
- source/platform/themes/theme_download.cpp — FOUND (modified)
- source/app/theme_detail_activity.cpp — FOUND (modified)
- source/app/theme_browser_activity.cpp — FOUND (modified)
- source/app/mod_browser_activity.cpp — FOUND (modified)
- source/app/cheat_detail_activity.cpp — FOUND (modified)
- source/app/game_list_activity.cpp — FOUND (modified)
- source/platform/http_client_curl.cpp — FOUND (modified)
- source/platform/mods/mod_download.cpp — FOUND (modified)
- Task 1 commit 079a727 — FOUND
- Task 2 commit 36992e0 — FOUND
- Task 3 commit 5627246 — FOUND
- Desktop build: [100%] Built target thomaz, zero errors
- Host tests: 185/185 passed, exit 0
