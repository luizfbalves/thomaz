# Pitfalls Research

**Domain:** Hardening pass — thomaz Switch homebrew hub (Borealis C++20 client + Fastify/Prisma API)
**Researched:** 2026-06-04
**Confidence:** HIGH (all pitfalls derived from direct codebase inspection, not general advice)

---

## Critical Pitfalls

### Pitfall 1: Static-root split breaks post-image serving when save blobs are moved

**Concerns.md item:** "Save blobs are publicly accessible via static file serving" (Security)

**What goes wrong:**
`@fastify/static` serves the entire `UPLOAD_DIR` tree at `/uploads/`. Post images live at
`uploads/<uuid>.jpg` (flat); save blobs live at `uploads/saves/<userId>/<titleId>.bin` (subdirectory).
The natural fix is to remove the `saves/` subtree from the static root. Two approaches both have
traps:

- **Option A — move blobs outside UPLOAD_DIR:** Any code that constructs a blob path via
  `join(env.UPLOAD_DIR, blobKey)` (currently `readSaveBlob`, `writeSaveBlob`, `deleteSaveBlob`
  in `save-storage.ts`) will break silently if the path calculation is changed in only some of those
  three functions. The `blobKey` is stored as `saves/<userId>/<titleId>.bin` in the DB column;
  changing the storage root without migrating existing keys orphans every previously uploaded blob.

- **Option B — add an authed `/saves/:userId/:titleId.bin` download route and exclude the
  `saves/` prefix from the static root:** The most common mistake here is constructing the
  download path by naively joining user-supplied `userId` and `titleId` parameters, which opens a
  path-traversal vector (`../` in the userId would escape the saves directory). `save-storage.ts`
  already normalises via `blobRelKey`, but a new route handler that bypasses that function and
  does a raw `join(savesDir, req.params.userId, req.params.titleId)` will be vulnerable.

**Why it happens:**
The codebase has three call sites (`readSaveBlob`, `writeSaveBlob`, `deleteSaveBlob`) that all agree
on path construction today — the fix touches `app.ts` (static root) and adds a new route, creating
a divergence window where the static route is locked down but the new authed route uses a different
path calculation.

**How to avoid:**
- Keep all blob path calculations inside `save-storage.ts`. The new download route must call
  `readSaveBlob(env, blobKey)` where `blobKey` is looked up from the DB by `(userId, titleId)` —
  never constructed from raw request params.
- Add the Vitest regression test for unauthenticated `GET /uploads/saves/...` returning 403/404
  **before** shipping the fix, so any accidental revert is caught immediately.
- Never introduce a second path-construction formula — the DB `blobKey` column is the single
  source of truth.

**Warning signs:**
- Any new route handler that calls `join(savesDir(env), req.params.anything)` directly.
- The existing `GET /uploads/saves/...` regression test starts failing (means static root
  was accidentally re-widened).
- Post image URLs stop returning 200 in the API test after the static-root change.

**Phase to address:** Security phase (save blob auth fix)

**Verification idea:**
```
Vitest: GET /uploads/saves/<realUserId>/<realTitleId>.bin (no auth header) → expect 404 or 403
Vitest: POST /posts with valid JPEG → expect imageUrl in response → GET that URL → expect 200
```

---

### Pitfall 2: Magic-byte check consumes the multipart stream before the file buffer is built

**Concerns.md item:** "MIME type spoofing for image uploads" (Security)

**What goes wrong:**
`@fastify/multipart` exposes each file part as a Node.js `Readable` stream (`part.file`). The current
code in `posts.ts` already buffers the entire stream into `imageBuffer` via `for await (const chunk
of part.file)`. Adding a magic-byte check by piping `part.file` to a separate consumer (e.g.,
`file-type`, `sharp`, or a manual `read(4)`) before the buffering loop will drain the stream, leaving
nothing for the buffer loop — resulting in an empty `imageBuffer` and a spurious 400 `missing_image`
error for every valid upload.

Additionally, reading the entire file to validate magic bytes (when the check only needs the first
4 bytes) re-introduces the "read whole file into memory" problem if the check is placed inside a
separate read pass.

**Why it happens:**
Developers instinctively add the check at the top of the part handler ("check first, then buffer"),
not realising the stream is already partially or fully consumed by the check.

**How to avoid:**
Buffer the file first (the loop that currently exists), then inspect `imageBuffer.slice(0, 4)` for
the JPEG SOI marker (`0xFF 0xD8`). This requires zero additional reads and no stream forking.
The magic-byte check is two lines after the existing buffer loop; do not restructure the loop order.

The minimal valid check:
```typescript
// After: imageBuffer = Buffer.concat(chunks)
const JPEG_SOI = [0xff, 0xd8];
if (imageBuffer[0] !== JPEG_SOI[0] || imageBuffer[1] !== JPEG_SOI[1]) {
  return reply.status(400).send(actionError("invalid_image_type"));
}
```

Do not introduce `sharp` or `file-type` as a dependency for this — they are heavy and the JPEG SOI
check is two bytes.

**Warning signs:**
- A test uploading a valid JPEG (e.g., `Buffer.from([0xff, 0xd8, 0xff, 0xd9])`) returns 400.
- Any code path that calls `part.file.read()` or `part.file.pipe()` before the chunk-collection loop.
- Adding a `sharp(part.file)` call before the buffer loop.

**Phase to address:** Security phase (magic-byte upload validation)

**Verification idea:**
```
Vitest: POST /posts with Buffer([0xff, 0xd8, 0xff, 0xd9]) (minimal JPEG) → expect 200
Vitest: POST /posts with Buffer([0x89, 0x50, 0x4e, 0x47]) (PNG magic bytes) → expect 400
Vitest: POST /posts with a text file renamed to .jpg → expect 400
```

---

### Pitfall 3: Access-token blocklist does not invalidate in-the-wild 365-day tokens

**Concerns.md item:** "JWT access token lifetime is 365 days by default" (Security)

**What goes wrong:**
The fix scope is "token revocation / blocklist on logout/compromise — JWT lifetime unchanged."
A common implementation mistake is to build the blocklist only around the refresh token (which
already has `revokeRefreshToken` in `refresh-tokens.ts`) and call the work done. This misses the
access token entirely: after logout, the refresh token is revoked, but the access token remains
valid for up to a year. Any client holding an old access token can continue to call all authed
API routes.

A second mistake is implementing the blocklist as an in-memory `Set<string>` in the Fastify
process. PM2 restarts or Lightsail reboots wipe the set, reinstating all "revoked" tokens.

A third mistake is storing the full JWT string in the blocklist. JWTs are ~200 bytes each. With
365-day lifetimes, the blocklist grows unbounded until process restart — effectively a memory leak
in a long-running production process.

**Why it happens:**
- Revoking the refresh token is already done; it feels like logout is "handled."
- In-memory sets are the first instinct for a blocklist.
- The full token string is the obvious thing to store.

**How to avoid:**
- Store the JWT `jti` claim (or a short hash of it) in a Postgres table, with the token's own
  `exp` timestamp as a `purge_after` column. A cleanup job (or a simple DELETE WHERE `purge_after <
  NOW()` on each request, batched) prevents unbounded growth.
- The `authenticate` preHandler must check the blocklist table after `jwtVerify()` succeeds.
- Do not add a jti-check to the `optionalAuth` path unless the feed is also being locked down
  (it is currently intentionally public — touching that would be a behavior change).
- Existing 365-day tokens already in the wild do not need special handling; they simply will not be
  in the blocklist and continue to work until revoked or expired. That is the intended behavior per
  PROJECT.md.

**Warning signs:**
- Logout endpoint returns 200 but a subsequent `GET /saves` with the old access token still
  returns 200 (test this explicitly).
- Blocklist stored in `Map`/`Set` on the module scope.
- Blocklist rows have no `purge_after` / `expiresAt` column (unbounded growth).
- `authenticate` decorator does not query the blocklist after `jwtVerify`.

**Phase to address:** Security phase (token revocation)

**Verification idea:**
```
Vitest: login → get token → logout (POST /auth/logout with refreshToken) →
        GET /saves with old access token → expect 401
Vitest: login → get token → logout → login again → GET /saves with new token → expect 200
Vitest (growth): insert 1000 expired blocklist rows, run purge, assert table is empty
```

---

### Pitfall 4: Races between refresh rotation and access-token revocation at logout

**Concerns.md item:** "JWT access token lifetime is 365 days by default" (Security); refresh-token
rotation in `refresh-tokens.ts`

**What goes wrong:**
The logout route (`POST /auth/logout`) currently only revokes the refresh token. If the implementation
adds access-token revocation to logout, the sequence matters:

1. Client POSTs logout with `{ refreshToken }`.
2. Server deletes refresh token (existing `revokeRefreshToken`).
3. Server adds access-token jti to blocklist.

If the client simultaneously fires a `/auth/refresh` call (race), the refresh succeeds before step 2
and issues a new access token. The new access token's jti is never added to the blocklist, so the
client retains a valid session despite "logging out." This is a race that only matters if a client
issues logout and refresh simultaneously, which is an edge case — but the implementation must at
minimum be aware of it and document that logout is best-effort for in-flight refreshes.

**Why it happens:**
Adding blocklist logic to logout without considering that rotation and revocation are not atomic in
Postgres (two separate deletes / inserts with no transaction spanning them).

**How to avoid:**
Wrap the logout operation in a single Prisma transaction: delete the refresh token and insert the
access-token jti into the blocklist atomically. This does not close the race entirely (the client
refresh may have already completed before the transaction), but it prevents the common split-second
window where the refresh token is gone but the blocklist entry is not yet written.

**Warning signs:**
- Logout `revokeRefreshToken` and blocklist insert are in two separate `await` calls with no
  transaction wrapper.
- The logout route does not accept or parse the access token at all (means the jti is never
  recorded).

**Phase to address:** Security phase (token revocation, same as Pitfall 3)

**Verification idea:**
Difficult to test the exact race in Vitest without artificial delays; instead test the happy path
atomicity: within a single Vitest test, call logout, then immediately attempt a refresh with the old
refresh token, and confirm both the refresh fails (409/401) and the old access token is rejected.

---

### Pitfall 5: `std::atomic<bool>` on `cloudBusy` gives false safety for compound state

**Concerns.md item:** "`cloudBusy` threading contract made safe/explicit" (Fragile Areas)

**What goes wrong:**
`cloudBusy` is a plain `bool` today, accessed only from the Borealis main thread via `brls::sync`
closures — which is safe. Replacing it with `std::atomic<bool>` is the recommended fix per
CONCERNS.md. The trap is thinking that `atomic<bool>` makes the compound check-then-set pattern
safe across threads:

```cpp
if (this->cloudBusy.load()) return;  // check
this->cloudBusy.store(true);         // set — NOT atomic with the check above
```

This is still a TOCTOU race if `cloudBusy` is read and written from different threads simultaneously.
On the current architecture all writes go through `brls::sync` (main thread), so there is no
multi-writer problem — but if a future edit removes a `brls::sync` wrapper, the atomic flag gives
developers a false sense that the compound operation is safe.

**Why it happens:**
`std::atomic<bool>` is correctly associated with thread safety, so developers stop thinking about
the compound operation once they see `atomic` in the type.

**How to avoid:**
- The real fix is **documentation**: add a comment in `save_detail_activity.hpp` that `cloudBusy`
  is a main-thread-only flag (reads and writes always occur in `brls::sync` or directly in UI event
  handlers, never in a bare `brls::async` body).
- Using `std::atomic<bool>` is still a good change (it makes UB impossible if the invariant is ever
  accidentally broken), but the comment is the primary prevention.
- If truly wanting to be safe, use `compare_exchange_strong` for the check-then-set in `doUpload`:
  ```cpp
  bool expected = false;
  if (!this->cloudBusy.compare_exchange_strong(expected, true)) return;
  ```
  This is safe regardless of threading model.

**Warning signs:**
- `cloudBusy` is loaded and stored in a bare `brls::async` lambda body (not inside a `brls::sync`).
- A new `doSomethingCloud()` function uses `cloudBusy` without wrapping the final write in `brls::sync`.

**Phase to address:** Concurrency phase (`cloudBusy` fix)

**Verification idea:**
Desktop build + doctest: cannot directly test threading on the fake platform, but a static analysis
check or a comment + code review is the verification. The real guard is: `grep -n "cloudBusy" source/`
must show every write inside a `brls::sync` or directly in an event handler, never in a bare `brls::async`.

---

### Pitfall 6: `runAsync` wrapper that captures `alive` by value after `this` is moved

**Concerns.md item:** "`alive` lifetime-guard pattern made hard to omit" (Fragile Areas)

**What goes wrong:**
Adding a `runAsync` helper to the activity base class to auto-capture `alive` is the right fix. The
common mistake is capturing `this` inside the wrapper and then capturing `alive` from `this->alive`
inside the async body:

```cpp
// WRONG — this may be destroyed before the async body runs
void ActivityBase::runAsync(std::function<void()> work) {
    brls::async([this, work]() {  // 'this' captured raw
        work();
        brls::sync([this]() {     // 'this' may be dangling
            // ...
        });
    });
}
```

The fix is correct only if `alive` is captured by value (copied as a `shared_ptr`) before the
async dispatch, not accessed through `this` inside the async body.

**Why it happens:**
Writing `[this, alive = this->alive]` in every call site is tedious; the wrapper is meant to hide
this, but the wrapper author may use `this` inside the body as a shortcut.

**How to avoid:**
The wrapper must copy `alive` before dispatching:
```cpp
void ActivityBase::runAsync(std::function<void(std::function<void(std::function<void()>)>)> fn) {
    auto alive = this->alive; // copy shared_ptr here, on the main thread
    brls::async([alive, fn]() {
        fn([alive](std::function<void()> cb) {
            brls::sync([alive, cb]() {
                if (!alive->load()) return;
                cb();
            });
        });
    });
}
```
The wrapper must never mention `this` inside the async or sync lambdas — only the copied `alive`.

**Warning signs:**
- `runAsync` implementation captures `this` inside the `brls::async` body.
- Existing call sites that have `[this, alive = this->alive, ...]` are replaced with
  `runAsync(...)` but the wrapper re-introduces `this` capture.
- Desktop build + running the fake platform and navigating away from an activity mid-operation
  causes a segfault or ASAN heap-use-after-free.

**Phase to address:** Concurrency phase (`alive` wrapper fix)

**Verification idea:**
Desktop build with AddressSanitizer: navigate to `SaveDetailActivity`, trigger an upload, immediately
press B to pop the activity, verify no ASAN errors. This is the most direct test for the lifetime
guard.

---

### Pitfall 7: Cancelling libcurl from the wrong thread causes undefined behavior

**Concerns.md item:** "`brls::async` pool exhaustion on simultaneous operations" (Fragile Areas)

**What goes wrong:**
The fix is to add `std::atomic<bool> cancelled` per activity and abort in-flight curl requests on
activity destruction. The trap: libcurl easy handles are not thread-safe. Calling `curl_easy_cleanup`
or modifying the handle's `CURLOPT_*` options from the main thread while the async thread is blocking
in `curl_easy_perform` is undefined behavior and frequently causes crashes.

The correct cancellation mechanism for libcurl is a progress callback:
```c
// In the async thread, before curl_easy_perform:
curl_easy_setopt(handle, CURLOPT_PROGRESSFUNCTION, my_progress_cb);
curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
// my_progress_cb checks `cancelled->load()` and returns 1 to abort
```
Returning non-zero from `CURLOPT_PROGRESSFUNCTION` causes `curl_easy_perform` to abort with
`CURLE_ABORTED_BY_CALLBACK`, cleanly on the async thread.

**Why it happens:**
The obvious implementation calls `curl_easy_cleanup` from the destructor (main thread), not realising
the async thread still owns the handle.

**How to avoid:**
- `cancelled` flag is written by the destructor (main thread) via `cancelled->store(true)`.
- The async thread's curl progress callback reads `cancelled->load()` and returns 1 to abort.
- `curl_easy_cleanup` is called only inside the async thread, after `curl_easy_perform` returns.
- The destructor must not touch the curl handle at all — it only sets the flag.
- This pattern must be added to `HttpClientCurl` or whichever class holds the curl handle, not
  directly in the activity.

**Warning signs:**
- `curl_easy_cleanup` called outside the thread that owns the handle.
- Destructor calling any `curl_easy_*` function.
- Cancellation implemented as `curl_easy_reset` from the main thread.

**Phase to address:** Concurrency phase (async pool / cancellation fix)

**Verification idea:**
Desktop build: trigger a long-running HTTP fetch (e.g., to a slow test server), immediately navigate
away. Without the fix: eventual crash or hang. With the fix: the fetch aborts within one progress
callback tick, the activity destructs cleanly, no ASAN errors.

---

### Pitfall 8: `brls::sync` introducing deadlock when called from inside `brls::sync`

**Concerns.md item:** "`cloudBusy` threading contract" / async pool (Fragile Areas); general risk
when adding the `runAsync` wrapper

**What goes wrong:**
`brls::sync` posts a closure to the Borealis main-thread queue and blocks the calling thread until
the main thread processes it. If the refactored `runAsync` wrapper or any new helper ever calls
`brls::sync` from code that is already running on the main thread (e.g., from an event handler or
from inside an existing `brls::sync` callback), the main thread will block waiting on itself —
deadlock.

This is most likely to appear when the `runAsync` wrapper is used inside a UI dialog callback:
```cpp
dialog->addButton("OK", [this]() {
    // This runs on the main thread!
    this->runAsync([](auto sync) {
        // ... async work ...
        sync([](){ /* OK, still safe */ });
    });
});
```
The above is fine — `runAsync` dispatches to the async pool and only calls `brls::sync` from inside
the async thread. The deadlock occurs only if someone calls a blocking sync primitive from the main
thread directly.

**Why it happens:**
When abstracting async patterns into helpers, it becomes less obvious which thread is executing the
code at each call site.

**How to avoid:**
The `runAsync` wrapper's contract must be documented: "Call only from the main thread (event handlers,
`onContentAvailable`, direct UI callbacks). Never call from inside `brls::async` or `brls::sync`."
Add an `BRLS_ASSERT` or `brls::Application::isMainThread()` check at the top of `runAsync` in debug
builds.

**Warning signs:**
- `runAsync` called from inside a `brls::async` body.
- A helper function that calls `runAsync` without documenting which thread it expects.
- Deadlock / freeze during desktop testing on the fake platform.

**Phase to address:** Concurrency phase (runAsync wrapper implementation)

**Verification idea:**
Desktop build: every `runAsync` call site should be verifiable as a main-thread invocation by code
inspection. A debug-mode `assert(brls::Application::isMainThread())` at the top of `runAsync` will
catch violations at runtime during desktop testing.

---

### Pitfall 9: Pino logging leaking PII or JWT secrets in production

**Concerns.md item:** "Production logging disabled" (Tech Debt)

**What goes wrong:**
Changing `logger: false` to `logger: env.NODE_ENV !== 'test'` enables Fastify's built-in Pino
logger. By default Pino serializes the entire request object, which in Fastify's case includes:

- The `Authorization: Bearer <token>` header in every authed request (request log).
- The `password` field from `POST /auth/login` body if request body logging is enabled.
- The `JWT_SECRET` if it appears in any error object.

The `api/src/lib/serializers.ts` file exists in the codebase, suggesting serializer configuration
is already partially set up, but it may not cover all sensitive fields.

**Why it happens:**
Enabling logging is treated as a simple boolean flip; developers do not audit what Pino serializes
by default.

**How to avoid:**
When enabling the logger, always pass explicit `serializers`:
```typescript
const app = Fastify({
  logger: env.NODE_ENV !== 'test' ? {
    serializers: {
      req(req) {
        return {
          method: req.method,
          url: req.url,
          // explicitly omit: headers (contains Authorization)
        };
      },
      res(res) {
        return { statusCode: res.statusCode };
      },
    },
  } : false,
});
```
Do not log `req.headers` or `req.body` at the top-level logger — auth routes handle credentials and
all authed routes carry the Bearer token.

**Warning signs:**
- `Authorization` appears in a Pino log line during testing.
- `serializers` key absent from the logger config object.
- `req.headers` is included in the serializer output.

**Phase to address:** Tech debt phase (production logging)

**Verification idea:**
In the Vitest test, after enabling the logger, call `POST /auth/login` with real credentials and
assert that no log output contains the word `password` or `Bearer`. Pino can be configured to write
to a writable stream in test mode; capture the stream and inspect it.

---

### Pitfall 10: TLS fail-safe warning changes behavior instead of adding safety net

**Concerns.md item:** "TLS verification silently disabled on CA bundle failure" (Security);
PROJECT.md: "Intentional trade-offs get safety nets, not behavior changes"

**What goes wrong:**
The fix is to add a visible on-screen warning when `ca_ok == false` — behavior is preserved (TLS
stays disabled in the fail-safe path), only a Borealis notification is added. The pitfall is
over-engineering the fix:

- **Making the CA check non-static:** `ca_ok` is a `static const bool` computed once via IIFE. If
  changed to a non-static check, it fires on every request, causing `fopen("romfs:/cacert.pem")`
  per HTTP call — a performance regression and a change to the original author's intent (probe once,
  cache result).
- **Blocking the app on CA failure:** Showing a modal dialog that blocks until the user dismisses
  it (instead of a non-blocking notification) would break the fail-safe intent — the whole point is
  to keep the app functional.
- **Disabling SSL in the desktop `#else` branch by mistake:** `curl_tls.hpp` has an `#ifdef
  __SWITCH__` guard. Any edit that accidentally moves the `VERIFYPEER 0` lines outside that guard
  would silently disable TLS verification in the desktop build, which has no CA bundle issue.

**Why it happens:**
The fix is conceptually simple ("show a warning") but touches a file with platform guards and a
subtle static initialization pattern.

**How to avoid:**
- Keep `ca_ok` as `static const bool` — do not change the check type or timing.
- The notification call goes inside the `else` branch only, after the curl opts are set.
- Do not move any curl opts outside the `#ifdef __SWITCH__` block.
- Desktop build (`-DUSE_SDL2=ON`) must pass cleanly after the change — this is the primary
  regression check.

**Warning signs:**
- `std::fopen("romfs:/cacert.pem")` called more than once per process lifetime.
- A blocking `brls::Dialog` instead of `brls::Application::notify`.
- `CURLOPT_SSL_VERIFYPEER, 0L` appears outside the `#ifdef __SWITCH__` block.
- Desktop build breaks after the change.

**Phase to address:** Security phase (TLS fail-safe warning)

**Verification idea:**
Desktop build (`cmake -DUSE_SDL2=ON && make`) must succeed with exit 0. For the Switch path: a C++
doctest that compiles with `__SWITCH__` defined and calls `apply_curl_tls` with a fake CURL handle
(mock), asserting that `CURLOPT_SSL_VERIFYPEER` is set to 0 when the bundle file is absent — but
given the fake platform constraints, a code review + grep check is sufficient:
```
grep -n "VERIFYPEER\|VERIFYHOST" source/platform/curl_tls.hpp
```
All `0L` assignments must be inside `#ifdef __SWITCH__` and inside `else { ... }` only.

---

### Pitfall 11: Extracting `ensure_parent_dirs` breaks the subtle `theme_install.cpp` variant

**Concerns.md item:** "Duplicated `ensure_parent_dirs` utility" (Tech Debt)

**What goes wrong:**
CONCERNS.md notes that `theme_install.cpp` has a divergent implementation: "walks up to the last
char; others stop at strict boundaries." If the canonical `fs_util.hpp` version is written to match
the majority behavior and `theme_install.cpp` is silently migrated to use it, the theme install path
will change behavior on file paths that include trailing characters after the last `/`.

The majority behavior finds the last `/` via `rfind('/')` and creates dirs up to that position.
The `theme_install.cpp` variant may include the last character (e.g., treating a path that ends in
`/` differently). Silently switching theme install to the majority implementation could cause theme
files to be written to a wrong path, breaking theme installation on Switch.

**Why it happens:**
The implementations look identical at a glance; the subtle difference only manifests on edge-case
paths (trailing slashes, paths ending in directory names without a trailing separator).

**How to avoid:**
Before extracting, write a doctest that exercises both behaviors with a representative theme install
path (a path that `theme_install.cpp` actually constructs). Confirm the new canonical helper produces
the same output as the old `theme_install.cpp` version for those paths. Only after passing that test
should `theme_install.cpp` be migrated.

**Warning signs:**
- `ensure_parent_dirs` extracted and all four call sites migrated in a single commit with no new
  tests for the edge case.
- Desktop build passes but no theme-path-specific tests run.

**Phase to address:** Tech debt phase (`ensure_parent_dirs` extraction)

**Verification idea:**
```cpp
// In tests/test_fs_util.cpp
TEST_CASE("ensure_parent_dirs: theme path ending in filename") {
    // path as constructed by theme_install.cpp
    std::string path = "/tmp/thomaz_test_theme/NintendoSwitch/qlaunch/romfs/lyt/ResidentMenu.szs";
    ensure_parent_dirs(path);
    // assert parent directory exists
    CHECK(std::filesystem::exists("/tmp/thomaz_test_theme/NintendoSwitch/qlaunch/romfs/lyt"));
}
```

---

### Pitfall 12: C-style view cast replacement introducing null-deref instead of eliminating it

**Concerns.md item:** "C-style casts for Borealis view lookups" (Tech Debt)

**What goes wrong:**
The fix replaces `(brls::Box*)this->getView("id")` with `brls::View::cast<brls::Box>(this->getView("id"))`
or a null-guarded `dynamic_cast`. The pitfall is inconsistent replacement:

- If `brls::View::cast<T>()` is unavailable (vendored Borealis fork may not have this method),
  developers fall back to `dynamic_cast<T*>(this->getView("id"))` — which returns `nullptr` on type
  mismatch, silently. The subsequent dereference (`lbl->setText(...)`) then crashes exactly as before,
  just later.
- Using `static_cast` instead of `dynamic_cast` as a "faster alternative" removes the safety check
  entirely — same as the original C-style cast.

The existing `if (auto* view = ...)` pattern used in most other places is the correct reference
implementation for this codebase.

**How to avoid:**
Use the pattern already established in the codebase:
```cpp
if (auto* lbl = dynamic_cast<brls::Label*>(this->getView("lastBackup"))) {
    lbl->setText(lastBackupText(...));
}
```
Check first whether `brls::View::cast<T>()` exists in the vendored Borealis fork before relying
on it. If it does not exist, use the null-guarded `dynamic_cast` pattern. Never use `static_cast`
as a replacement.

**Warning signs:**
- `static_cast<T*>(this->getView(...))` — no null check, same risk as C-style cast.
- `dynamic_cast<T*>(...)` without a null check on the result.
- `brls::View::cast<T>()` called but the vendored Borealis does not define that method (compile error
  or linker error on the NRO build).

**Phase to address:** Tech debt phase (view cast replacement)

**Verification idea:**
Desktop build with `-DUSE_SDL2=ON` must compile cleanly. Manually navigate to each affected screen
in the desktop binary; if any view is missing from XML, the null-guarded path must not crash —
the screen just silently skips the update. A test that loads each affected activity in the fake
platform and calls `onContentAvailable()` would catch null-deref reliably.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Keeping `logger: false` in test env | Test output is clean | No PII/secret leak audit needed | Always keep false in test |
| Inline blocklist jti check in `authenticate` decorator | Fast to implement | If blocklist grows huge, every request hits DB | Acceptable for now; add Prisma index on `jti` column |
| Using `randomUUID()` as jti | Zero extra config | Token is long; storing full string wastes space | Use first 16 chars or sha256 truncated to 8 bytes as blocklist key |
| Single `fs_util.hpp` with inline functions | No link-time overhead | If function grows complex, should be `.cpp` | Fine at current complexity |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| `@fastify/static` + auth route for same prefix | Registering the auth route after `fastify-static` makes static handler win for paths under that prefix | Register the authed save-download route before registering `fastify-static`, or exclude `saves/` prefix from the static root entirely |
| `@fastify/multipart` stream + magic-byte check | Reading the stream twice (once for check, once for buffer) | Buffer first (`Buffer.concat`), then inspect `buffer[0..3]` |
| Prisma blocklist table + `jwtVerify()` | Adding a DB query to every request without an index | Add index on `(jti)` and a partial index `WHERE purge_after > NOW()` |
| libcurl + cancellation | Setting `CURLOPT_*` from a different thread than `curl_easy_perform` | Use progress callback (`CURLOPT_PROGRESSFUNCTION`) on the same thread as `perform` |
| Borealis `brls::sync` + `brls::async` nesting | Calling `brls::sync` from the main thread (deadlock) | Only call `brls::sync` from inside a `brls::async` body |

---

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Building save download path from raw `req.params` | Path traversal — attacker reads arbitrary files under UPLOAD_DIR | Always derive path from DB-stored `blobKey`, never from user input |
| Blocklist in memory only | Revoked tokens reinstated after PM2 restart | Store blocklist in Postgres with `purge_after` column |
| Logging `req.headers` in Pino | Authorization Bearer tokens appear in Lightsail logs | Explicit serializer that omits headers |
| Adding `jti` to JWT payload but not verifying it in `authenticate` | Blocklist exists but is never consulted | Blocklist check must be inside the `authenticate` preHandler, after `jwtVerify` |
| Making TLS warning a blocking dialog | App unusable when bundle probe fails | Use non-blocking `brls::Application::notify` |

---

## "Looks Done But Isn't" Checklist

- [ ] **Save blob auth fix:** Confirm `GET /uploads/saves/...` returns 403/404 without auth — not just
      that the route is registered but that the static handler no longer serves that path.
- [ ] **Magic-byte check:** Confirm valid JPEG still uploads (not just that bad files are rejected).
- [ ] **Token blocklist:** Confirm old access token is rejected after logout — not just that the
      refresh token is revoked.
- [ ] **TLS warning:** Confirm desktop build still passes after `curl_tls.hpp` edit — not just that
      the Switch build compiles.
- [ ] **`ensure_parent_dirs` extraction:** Confirm theme-specific path edge case is tested, not just
      that the file compiles.
- [ ] **View cast replacement:** Confirm each replaced cast has a null guard — not just that `dynamic_cast`
      replaced the C-style cast.
- [ ] **Production logging:** Confirm `Authorization` header does not appear in log output — not just
      that `logger` is no longer `false`.

---

## Pitfall-to-Phase Mapping

| Pitfall | Fix area (CONCERNS.md) | Prevention Phase | Verification |
|---------|------------------------|------------------|--------------|
| Static root breaks post images | Security: save blob auth | Security phase | Vitest: GET post image URL → 200 after fix |
| Path traversal in authed download route | Security: save blob auth | Security phase | Vitest: GET `/uploads/saves/../other` → 400 |
| Stream consumed before buffer (magic bytes) | Security: MIME validation | Security phase | Vitest: valid JPEG upload → 200 |
| Blocklist revokes refresh only, not access token | Security: token revocation | Security phase | Vitest: logout → old access token → 401 |
| Refresh/revoke race non-atomic | Security: token revocation | Security phase | Code review: single Prisma transaction |
| `atomic<bool>` false safety (cloudBusy) | Concurrency: cloudBusy | Concurrency phase | Code review + grep for writes outside brls::sync |
| `runAsync` wrapper captures `this` | Concurrency: alive wrapper | Concurrency phase | Desktop ASAN: nav away mid-request |
| libcurl cancel from wrong thread | Concurrency: async pool | Concurrency phase | Desktop: nav away mid-fetch, no crash |
| `brls::sync` deadlock | Concurrency: runAsync | Concurrency phase | Debug assert isMainThread() in runAsync |
| Pino logs Authorization header | Tech debt: production logging | Tech debt phase | Vitest: login → assert no Bearer in log output |
| TLS warning changes static init | Security: TLS fail-safe | Security phase | Desktop build exit 0; grep VERIFYPEER |
| `ensure_parent_dirs` edge case (theme path) | Tech debt: fs_util extraction | Tech debt phase | doctest: theme path → correct parent dirs |
| View cast no null guard | Tech debt: view cast replacement | Tech debt phase | Desktop: open each affected activity, no crash |

---

*Pitfalls research for: thomaz hardening milestone*
*Researched: 2026-06-04*
