# Codebase Concerns

**Analysis Date:** 2026-06-04

## Tech Debt

**Duplicated `ensure_parent_dirs` utility:**
- Issue: The same `ensure_parent_dirs(path)` helper is copy-pasted verbatim into four separate translation units, each in its own anonymous namespace.
- Files: `source/platform/cheat_store.cpp`, `source/platform/mods/mod_download.cpp`, `source/platform/mods/libarchive_extractor.cpp`, `source/platform/themes/theme_install.cpp`
- Impact: Any bug fix or behavior change must be replicated in four places. Implementations are structurally identical but diverge in subtle details (e.g., `theme_install.cpp` walks up to the last char; others stop at strict boundaries).
- Fix approach: Extract into a shared `source/platform/fs_util.hpp` inline helper and include it where needed.

**C-style casts for Borealis view lookups:**
- Issue: `getView()` returns `brls::View*`, and several activity files cast the result with C-style casts (e.g., `(brls::Box*)this->getView("gameListBox")`), bypassing any runtime type safety.
- Files: `source/app/game_list_activity.cpp` (lines 84–85), `source/app/save_manager_activity.cpp` (lines 46–47), `source/app/save_detail_activity.cpp` (line 85), `source/app/mod_browser_activity.cpp` (line 45)
- Impact: If the XML view ID is mistyped or the wrong type is in layout, the cast silently produces a bad pointer and crashes on next use rather than failing early.
- Fix approach: Replace with `brls::View::cast<T>()` or `dynamic_cast<T*>` with a null guard. The existing `if (auto* view = ...)` pattern used in most other places is already correct.

**`copy_tree` duplicated in two translation units:**
- Issue: A recursive directory copy helper (`copy_tree`) exists independently in `source/platform/save_service_switch.cpp` (static linkage) and `source/platform/mods/mod_store.cpp` (also static linkage).
- Files: `source/platform/save_service_switch.cpp`, `source/platform/mods/mod_store.cpp`
- Impact: Divergent behavior under edge cases (e.g., error handling on file open failure differs). Neither version is tested in isolation.
- Fix approach: Factor into a shared platform utility.

**`caption` field in posts has no max-length schema guard:**
- Issue: The post upload handler in `source/app/routes/posts.ts` reads the `caption` multipart field into a bare `String(part.value)` without length capping before it reaches the DB `create`.
- Files: `api/src/routes/posts.ts` (lines 48–49)
- Impact: An attacker can store arbitrarily long caption strings in the database, exhausting text column space or causing unexpected behavior.
- Fix approach: Add `z.string().max(500)` (or a suitable limit) to the post body schema and validate before the DB write.

**Production logging disabled:**
- Issue: The Fastify instance is created with `logger: false` unconditionally, including production.
- Files: `api/src/app.ts` (line 29)
- Impact: No request logs, error traces, or response-time data are emitted in production. Diagnosing API issues on Lightsail requires PM2 process logs only.
- Fix approach: Pass `logger: env.NODE_ENV !== 'test'` and configure pino's serializers for production-appropriate output.

## Security Considerations

**Save blobs are publicly accessible via static file serving:**
- Risk: The API serves the entire `UPLOAD_DIR` directory tree at `/uploads/` via `@fastify/static`. Save blobs are stored at `uploads/saves/<userId>/<titleId>.bin` — predictable paths. Any caller who guesses or enumerates a userId + titleId can download another user's save without authentication.
- Files: `api/src/app.ts` (lines 43–47), `api/src/lib/save-storage.ts` (lines 11–16)
- Current mitigation: None. The saves subdirectory is under the same prefix used for post images.
- Recommendations: Either move save blobs outside `UPLOAD_DIR` (a directory not served statically), or add a route-level auth check and remove the saves subdirectory from the static root. Post images can remain static-served since they are intentionally public.

**TLS verification silently disabled on CA bundle failure (Switch):**
- Risk: If the romfs CA bundle (`romfs:/cacert.pem`) cannot be opened at startup (packaging error, mount issue), all HTTPS connections silently proceed with `SSL_VERIFYPEER=0` and `SSL_VERIFYHOST=0`. This is a documented intentional "fail-safe" trade-off.
- Files: `source/platform/curl_tls.hpp` (lines 31–35)
- Current mitigation: The CA probe is intentionally fail-safe per the comment. The risk is limited to a packaging defect scenario; the attacker would also need to be on-path.
- Recommendations: Log a visible on-screen warning to the user ("Network certificate verification unavailable") when `ca_ok == false`, so users are not silently exposed.

**MIME type spoofing for image uploads:**
- Risk: The post upload handler trusts the `Content-Type` reported by the multipart part (`part.mimetype`) rather than inspecting file magic bytes. A client can upload any binary content with `Content-Type: image/jpeg`.
- Files: `api/src/routes/posts.ts` (lines 40, 60–62)
- Current mitigation: Size limit (5 MB) is enforced in `api/src/lib/storage.ts`.
- Recommendations: Use a magic-byte check (e.g., `sharp` or a small JPEG SOI header probe) before persisting the file.

**JWT access token lifetime is 365 days by default:**
- Risk: Stolen tokens remain valid for up to one year. The comment in config explains this is intentional for console UX (no auto-refresh on device).
- Files: `api/src/config.ts` (lines 17–18)
- Current mitigation: Rate limiting on auth endpoints; refresh token rotation implemented in `api/src/lib/refresh-tokens.ts`.
- Recommendations: Implement token revocation (blocklist) for logout/compromise scenarios, since the 365-day window makes token expiry impractical as a security control.

## Performance Bottlenecks

**Archive listing traversed twice per extraction:**
- Problem: `extract_archive` calls `list_archive_entries` first (full pass over the archive) to get `total` for the progress callback, then opens and iterates the archive a second time to extract.
- Files: `source/platform/mods/libarchive_extractor.cpp` (lines 67–68)
- Cause: `total` is needed upfront for progress percentage, but libarchive is stream-based and doesn't expose entry count without a pass.
- Improvement path: Pass `-1` as total when unknown and adjust UI to show indeterminate progress, or cache the entry list from the first pass and replay it (requires buffering all entry metadata).

**Cloud save upload packages the entire active save on every push, even when status check is asynchronous:**
- Problem: In `doUpload`, the UI thread detaches a `brls::async` to fetch `CloudStatus`, returns to the main thread, then dispatches another `brls::async` (in `pushAtRevision`) to call `svc->packageActiveSave`. This means two network round trips and one blocking save-read before any data is sent. On slow SD cards with large saves this blocks the async pool thread for the entire read.
- Files: `source/app/save_detail_activity.cpp` (lines 260–338)
- Improvement path: Cache the last-known `CloudStatus` and skip the status prefetch when the cached value is recent enough (e.g., within 60 s).

## Fragile Areas

**`SaveDetailActivity::cloudBusy` flag with non-atomic access:**
- Files: `source/app/save_detail_activity.hpp`, `source/app/save_detail_activity.cpp`
- Why fragile: `cloudBusy` is a plain `bool` member read and written from both `brls::async` workers (via `brls::sync` closures) and UI event handlers. The `brls::sync` dispatch posts back to the main thread, so in practice it is main-thread-only, but there is no enforcement or documentation of this invariant. A future refactor that removes the `brls::sync` wrapper would silently introduce a data race.
- Safe modification: Document the threading contract explicitly. Consider replacing with a `std::atomic<bool>`.

**`alive` shared_ptr pattern as activity lifetime guard:**
- Files: `source/app/save_detail_activity.cpp`, `source/app/mod_browser_activity.cpp`, `source/app/theme_browser_activity.cpp` (pattern used throughout)
- Why fragile: All async lambda captures check `if (!alive->load()) return` to guard against use-after-free when an activity is popped while a background task is in flight. This is correct but requires every new `brls::async` block in every activity to remember to capture and check `alive`. Forgetting this in any new async operation silently introduces a use-after-free.
- Safe modification: Add a wrapper (e.g., `runAsync`) on the activity base that auto-captures `alive` and wraps the sync callback, preventing the pattern from being omitted.

**Token stored in session without expiry check on client:**
- Files: `source/platform/feed/auth_store.hpp`, `source/platform/feed/auth_store.cpp`
- Why fragile: `load_session()` returns a stored `AuthSession` with a `token` field. The comment in `SaveDetailActivity::requireSession()` notes: "has_value() only proves a session is stored, not that its token is still valid; an expired token returns 401 downstream and we re-prompt then." Because the JWT default is 365 days, this almost never triggers — but if the server secret changes or the token is otherwise invalidated, the UI shows cloud operations as available and fails silently until a 401 is returned mid-operation.
- Safe modification: Store and check a `expiresAt` timestamp locally; show "session expired" proactively before attempting a cloud operation.

**`brls::async` pool exhaustion on simultaneous operations:**
- Files: Multiple activity `.cpp` files
- Why fragile: Borealis' async thread pool is fixed-size. If a user opens and quickly navigates between multiple activities (e.g., save detail + mod browser), concurrent background tasks (cloud status fetch, GameBanana resolve) all queue into the same pool. There is no cancellation mechanism: tasks for a destroyed activity still run to completion even after the `alive` guard aborts the sync callback. On slow networks this can fill the pool.
- Safe modification: Implement request cancellation (e.g., via an `std::atomic<bool> cancelled` separate from `alive`) so in-flight curl requests are aborted on activity destruction.

## Test Coverage Gaps

**No API integration tests for the saves `PUT` conflict / revision path:**
- What's not tested: The `revision_required` and `revision_conflict` branches in `api/src/routes/saves.ts` (lines 145–154) have no test cases in `api/test/api.test.ts`.
- Files: `api/src/routes/saves.ts`, `api/test/api.test.ts`
- Risk: Optimistic-locking logic could regress silently. The revision counter is the only guard against concurrent overwrites from multiple clients.
- Priority: High

**No test covers the TLS fail-safe branch (`ca_ok == false`):**
- What's not tested: The `else` branch in `source/platform/curl_tls.hpp` (lines 31–35) that disables verification when the CA bundle is missing is exercised only at runtime on a packaging failure. There is no unit or integration test that verifies the fallback behavior.
- Files: `source/platform/curl_tls.hpp`
- Risk: A regression in bundle loading logic could silently disable TLS verification in production builds without failing any CI test.
- Priority: Medium

**No end-to-end test for the cloud save upload conflict resolution dialog:**
- What's not tested: The conflict dialog path in `SaveDetailActivity::doUpload` (lines 275–292) — when the remote revision has advanced since the local sync — is not covered by any test. The unit test suite covers `classify`/`plan_push` logic in `tests/test_save_sync.cpp` but not the UI conflict dialog path.
- Files: `source/app/save_detail_activity.cpp`, `tests/test_save_sync.cpp`
- Risk: The conflict branch (`plan.isConflict == true`) could be broken by a future change to `SyncSituation` or `PushPlan` without any test failure.
- Priority: Medium

**No test for save blob public URL accessibility (security regression):**
- What's not tested: There is no test asserting that `GET /uploads/saves/<userId>/<titleId>.bin` returns 403 or 404 for unauthenticated callers.
- Files: `api/test/api.test.ts`, `api/src/app.ts`
- Risk: The security concern (saves publicly accessible via static file serving) would not be caught by CI if a future fix is accidentally reverted.
- Priority: High

---

*Concerns audit: 2026-06-04*
