# Architecture Research

**Domain:** Hardening milestone — existing clean-architecture Switch homebrew + Fastify API
**Researched:** 2026-06-04
**Confidence:** HIGH (all findings derive from direct source-code inspection)

---

## System Overview

The system has two independent codebases: the NRO/desktop C++ binary and the Node.js cloud API.
This hardening pass touches both. The layering below shows where each fix lands.

```
┌───────────────────────────────────────────────────────────────────────┐
│  App Layer  source/app/*_activity.cpp                                 │
│  • C-style cast fixes (game_list, save_manager, save_detail, mod_browser)│
│  • runAsync base wrapper (all activities that use brls::async)         │
│  • cloudBusy comment/atomic doc (save_detail_activity)                │
└──────────────────────────┬────────────────────────────────────────────┘
                           │ calls
┌──────────────────────────▼────────────────────────────────────────────┐
│  Platform Layer  source/platform/                                      │
│  • NEW: fs_util.hpp  ← ensure_parent_dirs (4 copies → 1)              │
│  • NEW: fs_util.cpp  ← copy_tree (2 copies → 1)                       │
│  • curl_tls.hpp  ← TLS warning when ca_ok == false (Switch only)      │
└──────────────────────────┬────────────────────────────────────────────┘
                           │ depends on
┌──────────────────────────▼────────────────────────────────────────────┐
│  Core Layer  source/core/  (unchanged by this milestone)               │
└───────────────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────────────┐
│  API: api/src/                                                         │
│  app.ts          ← static plugin scope fix + logger flag              │
│  lib/save-storage.ts  ← SAVES_DIR moved outside UPLOAD_DIR           │
│  plugins/auth.ts ← blocklist check added to authenticate decorator   │
│  routes/auth.ts  ← POST /auth/logout writes to blocklist             │
│  routes/posts.ts ← caption length cap + magic-byte image validation  │
│  prisma/schema.prisma ← RevokedToken model for blocklist             │
│  lib/token-blocklist.ts  (NEW)                                        │
└───────────────────────────────────────────────────────────────────────┘
```

---

## Fix Catalogue: Target Locations and Integration Approach

### C++ Side

#### FIX-C1: `ensure_parent_dirs` — extract to shared platform helper
**Target files:**
- NEW `source/platform/fs_util.hpp` — declare and inline `ensure_parent_dirs(const std::string&)`
- DELETE the anonymous-namespace copies in `cheat_store.cpp`, `mod_download.cpp`, `libarchive_extractor.cpp`, `theme_install.cpp`

**Integration approach:**
`ensure_parent_dirs` is currently a POSIXonly `sys/stat.h` / `mkdir` function — it belongs in
`platform/` (not `core/`) because it calls OS APIs. The header-only inline form matches the
existing pattern of `curl_tls.hpp` (platform-level header, no separate `.cpp`). Each of the four
consumer files already `#include <sys/stat.h>`, so the new include replaces the verbatim body.

The `theme_install.cpp` variant has a subtly different loop (`acc.push_back` character-by-character
vs. the substring-at-slash approach in the other three). The canonical implementation should match
the substring-at-slash form (`cheat_store.cpp` variant is clearest), and `theme_install.cpp`
needs a one-time behavioral check to confirm both forms are equivalent for valid paths before the
local copy is removed.

**Desktop build impact:** No change — all four callers already compile on desktop; `fs_util.hpp`
adds no new headers.

**Test:** Add a doctest case in `tests/test_fs_util.cpp` covering `ensure_parent_dirs` on a temp
path. This is the only debted helper not currently exercised.

---

#### FIX-C2: `copy_tree` — extract to shared platform utility
**Target files:**
- `source/platform/fs_util.hpp` — declare `copy_tree(src, dst, err*)`
- `source/platform/fs_util.cpp` — implement (combine the two existing bodies; prefer `mod_store.cpp`
  version which has `is_dir` helper and better error reporting via `err*` out-param)
- DELETE static `copy_tree` from `mod_store.cpp`
- DELETE equivalent from `save_service_switch.cpp` (if it exists — CONCERNS says it does, though
  the file was not found at the standard path; verify during implementation)

**Integration approach:**
Unlike `ensure_parent_dirs`, `copy_tree` is too large for an inline and needs a `.cpp`. The
`fs_util.cpp` compilation unit is new and must be added to `CMakeLists.txt` (the build uses
`GLOB_RECURSE` on `source/`, so adding the file is sufficient if that glob covers `platform/`
directly — verify in `CMakeLists.txt`).

**Desktop build impact:** Safe; `copy_tree` uses only POSIX `opendir`/`dirent`, already used on
both targets.

**Shares file with FIX-C1.** Both edits write to `fs_util.hpp`/`.cpp` — serialize in the same
commit or PR.

---

#### FIX-C3: C-style casts — replace with null-guarded `dynamic_cast` or `brls::View::cast<T>()`
**Target files (exact lines per CONCERNS.md):**
- `source/app/game_list_activity.cpp` lines 84–85 (`brls::Box*`, `brls::Label*`)
- `source/app/save_manager_activity.cpp` lines 46–47
- `source/app/save_detail_activity.cpp` line 85
- `source/app/mod_browser_activity.cpp` line 45 (`brls::Label*`)

**Integration approach:**
Each site is a `(T*)this->getView("id")` pattern. The replacement is:

```cpp
// Before:
brls::Box* box = (brls::Box*)this->getView("gameListBox");

// After:
auto* box = dynamic_cast<brls::Box*>(this->getView("gameListBox"));
if (!box) return; // or log + return, matching existing null-guard convention
```

`brls::View::cast<T>()` is the idiomatic Borealis form if it exists in this vendored fork;
check `lib/borealis/library/include/borealis/core/view.hpp` before using it. The
`dynamic_cast` + null guard is universally safe regardless.

**Independent of all other fixes.** The four activity files touched here are not shared with
any other fix in this milestone.

**Desktop build impact:** None — `dynamic_cast` works on desktop; the current C-style casts
also compile on desktop, so no regression possible.

---

#### FIX-C4: `runAsync` base-class wrapper — auto-capture alive guard
**Target files:**
- Decide between two integration points:
  - **Option A (preferred):** Add `runAsync` as a protected method on a new base class
    `ThomazActivity` that inherits from `brls::Activity`, and have each existing activity
    inherit `ThomazActivity` instead. `ThomazActivity` owns the `alive` shared_ptr.
  - **Option B (lighter):** Add a free function template
    `thomaz::runAsync(alive, std::function<...>)` in a new `source/app/activity_util.hpp`,
    call it from each activity. Keeps the `alive` member per-activity.

  Option A is cleaner but touches every activity's inheritance line in all `.hpp` files.
  Option B touches call sites only. Given the goal is to make the pattern hard to omit, Option A
  is the correct choice: a future developer adding `brls::async` in an activity will see
  `runAsync` available without hunting for a utility header.

**Target files for Option A:**
- NEW `source/app/thomaz_activity.hpp` — base class with `alive` + `runAsync(worker, onSync)`
- `source/app/game_list_activity.hpp` — change `public brls::Activity` to `public ThomazActivity`
- `source/app/save_detail_activity.hpp` — same; remove the now-redundant `alive` member
- `source/app/mod_browser_activity.hpp` — same
- `source/app/theme_browser_activity.hpp` — same
- All corresponding `.cpp` files — replace `brls::async([this, alive, ...]` patterns with
  `this->runAsync([this, ...] { worker }, [this, ...] { sync_callback })`

**This fix touches the most files.** It is independent of C1/C2 (no shared file) but touches
the same `.cpp` files as FIX-C3 (different lines). They can be done in parallel branches or
serialized in any order.

**Desktop build impact:** No change. `brls::async`/`brls::sync` build on desktop.

---

#### FIX-C5: TLS warning when `ca_ok == false`
**Target files:**
- `source/platform/curl_tls.hpp` — add a visible UI warning in the `else` branch

**Integration approach:**
`curl_tls.hpp` is a header-only, `__SWITCH__`-guarded inline. The `ca_ok` probe is `static`,
so a warning can be emitted once from a `static` initializer using `brls::Logger::warning()`:

```cpp
static const bool ca_ok = [] {
    if (std::FILE* f = std::fopen("romfs:/cacert.pem", "rb")) {
        std::fclose(f);
        return true;
    }
    brls::Logger::warning("TLS: CA bundle missing — certificate verification disabled");
    return false;
}();
```

A one-time Borealis Logger warning is the minimal integration. A full on-screen dialog is
more visible but requires Borealis to be initialized (the probe runs before the first curl
call, which is after Borealis init, so it is safe). If an on-screen notification is required,
use `brls::Application::notify(...)` from the first actual curl call site after probe failure,
not from the static initializer (static order risk).

**Strictly isolated file.** No other fix touches `curl_tls.hpp`.

**Live API risk:** None — this is client-only (C++ NRO).

---

#### FIX-C6: `cloudBusy` threading invariant — document or promote to `std::atomic<bool>`
**Target files:**
- `source/app/save_detail_activity.hpp` — change `bool cloudBusy = false` to
  `std::atomic<bool> cloudBusy{false}` OR add a comment documenting the main-thread-only invariant

**Integration approach:**
The code already includes `<atomic>` (for `alive`). Changing `cloudBusy` to `std::atomic<bool>`
is a one-liner in the header and requires updating every read/write site to use `load()`/
`store()`. Reads are in `doUpload`, `doDownload`; writes are guarded by `brls::sync`, so they
are on the main thread. Using `std::atomic<bool>` makes the invariant compiler-enforced rather
than comment-enforced.

**Shares the header with FIX-C4** if FIX-C4 moves `alive` to a base class. Serialize these
two fixes or do FIX-C6 first (it's smaller) then FIX-C4 removes the `alive` member from the
same header.

---

### API Side

#### FIX-A1: Save blobs — remove from static-file root (HIGH security)
**Root cause:** `save-storage.ts` puts blobs under `UPLOAD_DIR/saves/`, and `app.ts` serves the
entire `UPLOAD_DIR` tree at `/uploads/` via `@fastify/static`. Post images (`UPLOAD_DIR/*.jpg`)
must stay static-served; saves must not.

**Target files:**
- `api/src/lib/save-storage.ts` — change `savesDir` to use a new `SAVES_DIR` env var (or a
  fixed sibling of `UPLOAD_DIR` like `../saves/` relative to the process cwd), completely
  outside the static-serve root
- `api/src/config.ts` — add `SAVES_DIR: z.string().default("./saves")` (separate from
  `UPLOAD_DIR`)
- `api/src/app.ts` — no change to the static plugin scope; it already serves only `UPLOAD_DIR`
  which will no longer contain saves after this fix
- `api/src/lib/storage.ts` — add `ensureSavesDir(env)` parallel to `ensureUploadDir`

**Do NOT add a route-level auth check as an alternative.** A belt-and-suspenders auth check on
the saves route is already present (`preHandler: [app.authenticate]`), but that protects the
API route, not the static file URL. Moving the directory is the correct fix.

**Interaction with `@fastify/static`:** After the fix, `UPLOAD_DIR` holds only post images
(`*.jpg`), which are intentionally public. The static plugin scope (`UPLOAD_DIR` → `/uploads/`)
is unchanged and continues to serve post images correctly.

**Live API deploy risk:** MEDIUM. Existing save blobs live at `uploads/saves/<userId>/...`.
Deployment must include a one-time migration step:
1. Copy `api/uploads/saves/` to `api/saves/` on the server.
2. Verify DB `SaveSlot.blobKey` values still resolve (they store relative keys like
   `saves/<userId>/<titleId>.bin`; `readSaveBlob` joins them to `SAVES_DIR` after the fix).
3. Remove `api/uploads/saves/` after verifying reads work.

Existing clients are unaffected: they call API routes, not the static URL directly. The blob
URL was never exposed through the official client code.

---

#### FIX-A2: Authenticated save-blob download route (security regression guard)
**After FIX-A1 moves blobs outside the static root, `GET /uploads/saves/...` returns 404 for
all callers automatically.** The explicit regression-guard test (CONCERNS test gap) becomes a
test that `GET /uploads/saves/<userId>/<titleId>.bin` returns 404 (file gone from that path).
No additional route change is needed for this guard; the test is the deliverable.

**Target files:**
- `api/test/api.test.ts` — add `it("save blob not reachable via static URL")`

**FIX-A2 depends on FIX-A1 being deployed first.** Do not write the test before the directory
move or it asserts the wrong thing.

---

#### FIX-A3: Token blocklist for access-token revocation (HIGH security)
**Target files:**
- `api/prisma/schema.prisma` — add `RevokedToken` model
- `api/prisma/migrations/` — new migration file generated by `prisma migrate dev`
- NEW `api/src/lib/token-blocklist.ts` — `revokeAccessToken(jti)`, `isRevoked(jti): bool`
- `api/src/plugins/auth.ts` — in `authenticate`, after `jwtVerify`, call `isRevoked(payload.jti)`;
  return 401 if true
- `api/src/routes/auth.ts` — `POST /auth/logout` additionally extracts the access token JTI
  (from Authorization header) and calls `revokeAccessToken`

**Prisma model sketch:**
```prisma
model RevokedToken {
  jti       String   @id          // JWT "jti" claim (UUID)
  expiresAt DateTime             // prune after this; indexed for cleanup
  revokedAt DateTime @default(now())

  @@index([expiresAt])
}
```

**Integration approach for `authenticate`:**
The `authenticate` decorator in `plugins/auth.ts` currently calls `request.jwtVerify()` and
catches exceptions. Add the blocklist lookup after a successful verify:

```typescript
app.decorate("authenticate", async function (request, reply) {
  try {
    await request.jwtVerify();
  } catch {
    return reply.status(401).send({ ok: false, error: "unauthorized" });
  }
  const jti = (request.user as JwtPayload).jti;
  if (jti && await isRevoked(app.prisma, jti)) {
    return reply.status(401).send({ ok: false, error: "unauthorized" });
  }
});
```

**JTI must be added to JWT payload.** Currently `JwtPayload` has `sub` and `username` only.
`@fastify/jwt` supports injecting a `jti` claim via `jwtSign` options (`{ jwtid: uuid() }`).
The `signAuthResponse` helper in `api/src/lib/auth-tokens.ts` needs to pass `jwtid`.

**Live API deploy risk for blocklist:** LOW for existing clients. Existing 365-day tokens lack
a `jti` claim; the blocklist check is skipped when `jti` is absent (`if (jti && ...)`), so
old tokens remain valid unchanged. Only tokens issued after the deploy have revocable JTIs.
This is an explicit trade-off: revocation of pre-deploy tokens is not possible without
invalidating all existing sessions. The milestone scope allows this (PROJECT.md: "safety net
only").

**Shares `plugins/auth.ts` with no other fix.** This fix is independent.

---

#### FIX-A4: Caption length cap on POST /posts
**Target files:**
- `api/src/routes/posts.ts` — add `z.string().max(500)` validation before the DB `create`

**Integration approach:**
Currently `caption` is read from multipart fields into a bare `String(part.value)` with no
length check. The fix validates after the multipart loop:

```typescript
const captionParsed = z.string().max(500).safeParse(caption);
if (!captionParsed.success) {
  return reply.status(400).send(actionError("invalid_caption"));
}
// use captionParsed.data
```

**Strictly isolated file.** No other fix touches `posts.ts`.
**Live API risk:** LOW. Clients sending captions ≤ 500 chars are unaffected. The Switch UI
likely caps input via the IME; this is a server-side guard only.

---

#### FIX-A5: Magic-byte image validation on POST /posts
**Target files:**
- `api/src/routes/posts.ts` — add a JPEG SOI magic-byte check before `saveJpeg`
- `api/src/lib/storage.ts` — optionally move the check into `saveJpeg` itself

**Integration approach:**
The handler already checks `imageMime` (from `Content-Type`). A magic-byte check probes the
first two bytes of the buffer:

```typescript
function isJpegMagic(buf: Buffer): boolean {
  return buf.length >= 2 && buf[0] === 0xff && buf[1] === 0xd8;
}
```

No new dependency is needed. `sharp` would give richer validation but adds a native module.
A SOI header probe is sufficient for this threat model (blocking obvious non-JPEG binary blobs
stored as images).

The check belongs in `posts.ts` immediately after the MIME check, before `saveJpeg` writes the
file. Alternatively, it can be folded into `saveJpeg` in `storage.ts` to make it automatic for
any future image-accepting route. Prefer the `storage.ts` location for defense-in-depth.

**Shares `posts.ts` with FIX-A4.** Serialize these two fixes (or do them in one PR).

---

#### FIX-A6: Production logging
**Target files:**
- `api/src/app.ts` line 29 — change `logger: false` to `logger: env.NODE_ENV !== 'test'`

**Integration approach:** One-line change. In production, Fastify uses pino by default with
JSON output, which is PM2-friendly. No serializer configuration is needed for basic request
logging; add pino serializers only if sensitive fields (passwords, tokens) appear in log output
(they should not with current routes, but verify `request.body` is not logged wholesale).

**Shares `app.ts` with FIX-A1** (which changes `save-storage.ts` and `config.ts` but does not
touch the logger line). The logger fix is a single-line edit; it can be a separate commit on
the same branch as FIX-A1 or independently merged. No ordering dependency.

**Live API deploy risk:** LOW. Adds log output; does not change behavior. PM2 will capture
pino's JSON lines. Disk usage on Lightsail increases; configure pino log rotation if needed.

---

#### FIX-A7: API tests — saves PUT revision branches
**Target files:**
- `api/test/api.test.ts` — add test cases for `revision_required` (PUT with existing slot,
  omitting `revision` field) and `revision_conflict` (PUT with wrong revision number)

**Independent of all other fixes.** The code under test already exists; no production files
are modified.

---

## Component Boundaries: What Each Fix Touches

| Fix | Primary Files Changed | Secondary Files Changed | Layer |
|-----|-----------------------|-------------------------|-------|
| FIX-C1 ensure_parent_dirs | `platform/fs_util.hpp` (NEW) | cheat_store, mod_download, libarchive_extractor, theme_install (remove copy) | Platform |
| FIX-C2 copy_tree | `platform/fs_util.hpp`, `platform/fs_util.cpp` (NEW) | mod_store.cpp, save_service_switch.cpp (remove copy) | Platform |
| FIX-C3 C-style casts | game_list_activity.cpp, save_manager_activity.cpp, save_detail_activity.cpp, mod_browser_activity.cpp | none | App |
| FIX-C4 runAsync wrapper | `app/thomaz_activity.hpp` (NEW), all activity .hpp + .cpp | none | App |
| FIX-C5 TLS warning | `platform/curl_tls.hpp` | none | Platform |
| FIX-C6 cloudBusy atomic | `app/save_detail_activity.hpp` | save_detail_activity.cpp | App |
| FIX-A1 save blob dir | `lib/save-storage.ts`, `config.ts` | `app.ts` (ensureSavesDir call), storage.ts | API |
| FIX-A2 blob URL test | `test/api.test.ts` | none | API Test |
| FIX-A3 token blocklist | `lib/token-blocklist.ts` (NEW), `plugins/auth.ts`, `routes/auth.ts`, `lib/auth-tokens.ts`, `schema.prisma` | migrations/ | API |
| FIX-A4 caption cap | `routes/posts.ts` | none | API |
| FIX-A5 magic bytes | `routes/posts.ts` OR `lib/storage.ts` | none | API |
| FIX-A6 logging | `app.ts` | none | API |
| FIX-A7 saves PUT tests | `test/api.test.ts` | none | API Test |

---

## Dependency and Ordering View

### Shared-File Conflicts (must serialize)

**Group S1 — `source/platform/fs_util.hpp` + `fs_util.cpp`:**
FIX-C1 and FIX-C2 both write to the same new files. Do them together in a single "extract fs_util" commit.

**Group S2 — `source/app/save_detail_activity.hpp`:**
FIX-C4 (runAsync wrapper moves `alive` to base class) and FIX-C6 (cloudBusy atomicity) both
modify this header. Do FIX-C6 first (smaller, isolated), then FIX-C4 removes the `alive` member
and the file ends up consistent.

**Group S3 — `api/src/routes/posts.ts`:**
FIX-A4 (caption cap) and FIX-A5 (magic bytes) both edit the same handler. Do them in one PR.

**Group S4 — `api/src/app.ts`:**
FIX-A1 calls `ensureSavesDir` at startup; FIX-A6 changes the logger flag. Same file, different
lines — safe to do in any order or together. No logical dependency between them.

**Group S5 — `api/test/api.test.ts`:**
FIX-A2 (blob URL test) and FIX-A7 (saves PUT tests) both add cases to the same test file.
FIX-A2 must come after FIX-A1 is deployed (the test asserts 404). FIX-A7 is independent.
They can be separate commits to the test file in any order.

### Independent Fixes (parallelizable across branches)

The following groups have zero shared files and can be developed and reviewed in parallel:

- FIX-C3 (casts) — touches only four activity `.cpp` files, all of which are also touched by
  FIX-C4 (different lines). Safe to parallelize if reviewers coordinate on those four files.
- FIX-C5 (TLS warning) — isolated to `curl_tls.hpp`.
- FIX-A3 (blocklist) — isolated to `plugins/auth.ts`, `routes/auth.ts`, `lib/auth-tokens.ts`,
  `lib/token-blocklist.ts`, `schema.prisma`.
- FIX-A4 + FIX-A5 (posts hardening) — isolated to `routes/posts.ts` / `lib/storage.ts`.
- FIX-A6 (logging) — single line in `app.ts`.
- FIX-A7 (saves PUT tests) — additive to `test/api.test.ts`.

### Logical Ordering for Coarse-Grained Phases

```
Phase 1: Foundation (no live-API risk, no shared files with later phases)
  • FIX-C1 + FIX-C2  (fs_util extraction — C++ only)
  • FIX-C5           (TLS warning — C++ only)
  • FIX-C6           (cloudBusy doc/atomic — C++ only)
  • FIX-A6           (logging — API, trivial, zero risk)
  • FIX-A4 + FIX-A5  (posts hardening — API, low risk, no migration)
  → Deliverable: clean desktop build, all modified APIs backward-compatible

Phase 2: Security — API storage restructure (requires server migration step)
  • FIX-A1           (move saves directory)
  → Deliverable: save blobs no longer reachable via /uploads/; migration script run on Lightsail

Phase 3: Security — token blocklist (requires DB migration)
  • FIX-A3           (RevokedToken model + authenticate decorator + logout handler)
  → Deliverable: prisma migrate deployed; new tokens are revocable

Phase 4: C++ concurrency / pattern hardening (largest diff, touches many activity files)
  • FIX-C3           (C-style casts)
  • FIX-C4           (runAsync base class)
  → Deliverable: clean desktop build, all activities converted

Phase 5: Tests (after corresponding production fixes are merged)
  • FIX-A2           (blob URL security regression test — after Phase 2)
  • FIX-A7           (saves PUT revision tests — independent, can be in Phase 1)
  → Deliverable: full Vitest suite passes; no regression on security fixes
```

FIX-A7 (saves PUT tests) has no production dependency and can move to Phase 1 without risk.

---

## Live-API Deploy Risk Per Change

| Fix | Risk | Reason |
|-----|------|--------|
| FIX-A1 save blob dir | MEDIUM | Requires server-side blob migration before deploy; clients using direct `/uploads/saves/` URLs (none in official client) break | 
| FIX-A3 blocklist | LOW | Pre-deploy tokens (no `jti`) skip blocklist check; safe for existing clients |
| FIX-A4 caption cap | LOW | Only breaks clients sending captions > 500 chars (Switch IME unlikely to produce these) |
| FIX-A5 magic bytes | LOW | Only breaks clients uploading non-JPEG bytes with JPEG MIME type |
| FIX-A6 logging | LOW | Additive; no behavior change; increases disk I/O for log writes |
| FIX-A2, FIX-A7 | NONE | Test-only; not deployed to production |

No API fix in this milestone requires existing 365-day tokens to be invalidated or forces
clients to re-authenticate. The blocklist (FIX-A3) is additive: it gates only tokens that are
explicitly revoked.

---

## Architectural Patterns to Follow

### Pattern: Inline platform-header utilities (C++)

`curl_tls.hpp` is the precedent: a header-only inline function for a POSIX-level platform
concern used across multiple translation units. `fs_util.hpp` should follow this pattern for
`ensure_parent_dirs` (small, used everywhere). `copy_tree` is larger and gets a `.cpp` to
avoid code bloat from inlining a recursive function.

### Pattern: Fastify plugin for cross-cutting API concerns

`plugins/auth.ts` is the precedent for decorating `app` with reusable behavior. The blocklist
check extends the existing `authenticate` decorator rather than adding a separate preHandler.
This keeps the check invisible to route authors — they continue using `preHandler: [app.authenticate]`
with no changes.

### Pattern: `lib/` for pure utility functions in the API

`lib/save-storage.ts`, `lib/auth-tokens.ts`, `lib/refresh-tokens.ts` are all side-effect-free
utility modules. `lib/token-blocklist.ts` follows this pattern. It should export pure functions
that take `PrismaClient` as a parameter, not import a singleton.

---

## Anti-Patterns to Avoid During This Milestone

### Adding business logic to `platform/` instead of `core/`

`ensure_parent_dirs` and `copy_tree` are pure filesystem helpers — no curl, no libnx. They
belong in `platform/` not `core/` because they call POSIX APIs (`mkdir`, `opendir`), but they
contain no business logic. Do not put them in `core/` (which is stdlib-only) and do not add
any business logic to `fs_util`.

### Protecting saves via route-level auth check instead of moving the directory

A `preHandler` auth check on `/uploads/saves/...` would require Fastify to handle that path as
a dynamic route, which conflicts with how `@fastify/static` serves files. The static plugin
handles matching paths directly via the filesystem before any route handler runs. The only
correct fix is removing saves from the static-served directory (FIX-A1).

### Invalidating all existing JWT tokens to add `jti`

Adding `jti` to new tokens and skipping the blocklist check when `jti` is absent is the
correct approach. Do not rotate `JWT_SECRET` as a workaround — that invalidates all 365-day
tokens in the wild and breaks every Switch console that has not re-authenticated.

### Replacing `brls::async` with a global thread pool

The `brls::async` pool exhaustion concern (CONCERNS.md Fragile Areas) is documented but not
in scope for this milestone. Do not introduce a new threading abstraction. The `runAsync`
wrapper (FIX-C4) wraps `brls::async`, not replaces it.

---

## Sources

All findings are derived from direct source inspection of the following files:
- `source/platform/cheat_store.cpp`
- `source/platform/mods/mod_download.cpp`
- `source/platform/mods/libarchive_extractor.cpp`
- `source/platform/mods/mod_store.cpp`
- `source/platform/themes/theme_install.cpp`
- `source/platform/curl_tls.hpp`
- `source/app/save_detail_activity.{cpp,hpp}`
- `source/app/game_list_activity.{cpp,hpp}`
- `source/app/mod_browser_activity.cpp`
- `source/app/theme_browser_activity.cpp`
- `api/src/app.ts`
- `api/src/config.ts`
- `api/src/lib/save-storage.ts`
- `api/src/lib/storage.ts`
- `api/src/lib/refresh-tokens.ts`
- `api/src/routes/saves.ts`
- `api/src/routes/posts.ts`
- `api/src/routes/auth.ts`
- `api/src/plugins/auth.ts`
- `api/prisma/schema.prisma`
- `api/test/api.test.ts`
- `.planning/codebase/CONCERNS.md`
- `.planning/codebase/ARCHITECTURE.md`
- `.planning/codebase/STRUCTURE.md`

---

*Architecture research for: thomaz hardening milestone*
*Researched: 2026-06-04*
