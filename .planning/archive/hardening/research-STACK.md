# Stack Research — Hardening Milestone

**Project:** thomaz (Switch homebrew hub)
**Domain:** Hardening — security, concurrency, debt, test coverage fixes
**Researched:** 2026-06-04
**Confidence:** HIGH (Node/Fastify items verified via official docs and Context7; C++ items verified via cppreference and libcurl official docs)

---

## Fix-by-Fix Recommendations

Each section maps directly to a CONCERNS.md item and gives the recommended technique, what to avoid, and confidence.

---

### 1. Save Blobs — Serve Private Files Behind Auth

**CONCERNS.md:** "Save blobs are publicly accessible via static file serving" (HIGH severity)

**Root cause:** `@fastify/static` serves the entire `UPLOAD_DIR` at `/uploads/`, including `uploads/saves/<userId>/<titleId>.bin`. Post images are intentionally public; save blobs are not.

**Recommended approach: `allowedPath` filter on the existing `@fastify/static` registration**

`@fastify/static` 8.x exposes an `allowedPath(pathName, root, request)` callback. Return `false` to deny; the plugin serves a 404. This requires zero new dependencies and surgically blocks the `saves/` subtree while leaving image paths untouched.

```typescript
await app.register(fastifyStatic, {
  root: join(process.cwd(), env.UPLOAD_DIR),
  prefix: '/uploads/',
  decorateReply: false,
  allowedPath: (pathName) => !pathName.startsWith('/saves/'),
});
```

For the authenticated download path (when a client explicitly fetches save data), the existing `/saves/:titleId?includeData=1` route already reads the blob via `readSaveBlob` and returns it as base64 — no static-serving needed. The `allowedPath` block is purely a defence-in-depth backstop.

**What NOT to do:**
- Do not add a second `@fastify/static` registration scoped to only images — two registrations on overlapping roots fight each other and require careful ordering.
- Do not move the saves directory outside `UPLOAD_DIR` right now; it would require a data migration on the live Lightsail instance and is riskier than a filter.
- Do not use a wildcard `GET /uploads/saves/*` preHandler route to reject requests — `@fastify/static` routes bypass Fastify's normal route-matching and preHandlers would not fire.

**Confidence:** HIGH — `allowedPath` is documented in `@fastify/static` README (verified via WebFetch of the GitHub repo). The existing route already handles the authenticated read path.

---

### 2. Caption Max-Length — Zod Cap on Multipart String Fields

**CONCERNS.md:** "`caption` field in posts has no max-length schema guard" (Tech Debt / Security)

**Recommended approach: in-loop length guard + existing Zod schema**

The current handler reads multipart fields via `request.parts()` and assigns `caption = String(part.value)` with no bound. The correct fix is a two-layer cap:

**Layer 1 — Parsing-level cap** (prevents reading oversized data into memory at all):

```typescript
await app.register(multipart, {
  limits: {
    fileSize: 16 * 1024 * 1024,
    fieldSize: 2048,   // bytes; no field value should exceed this
  },
});
```

`fieldSize` is a `@fastify/busboy` passthrough option. It truncates at the byte limit silently; combined with the Zod check below it becomes a hard reject.

**Layer 2 — Zod validation after parsing**:

```typescript
const captionSchema = z.string().max(500);
// inside the for-await loop:
if (part.fieldname === 'caption') {
  const r = captionSchema.safeParse(String(part.value));
  if (!r.success) return reply.status(400).send(actionError('caption_too_long'));
  caption = r.data;
}
```

The existing `commentBodySchema` already uses `.max(2000)` — use the same pattern for parity.

**What NOT to do:**
- Do not rely on `fieldSize` alone (it truncates silently without rejecting the request).
- Do not use `attachFieldsToBody: true` just for this fix — it changes the parsing contract for the entire multipart handler and would require refactoring the parts-iteration loop.

**Confidence:** HIGH — `@fastify/multipart` option passthrough to busboy is in the package README; Zod `.max()` is core Zod API.

---

### 3. Image Upload — Validate by Magic Bytes, Not Content-Type

**CONCERNS.md:** "MIME type spoofing for image uploads" (Security)

**Recommended approach: zero-dependency inline header probe**

The project currently accepts only `image/jpeg`. A JPEG file always begins with the 3-byte Start-Of-Image marker `FF D8 FF`. A 3-byte buffer slice check is sufficient and adds no dependency:

```typescript
function isJpegBuffer(buf: Buffer): boolean {
  return buf.length >= 3 &&
    buf[0] === 0xff && buf[1] === 0xd8 && buf[2] === 0xff;
}
```

Replace the `imageMime !== 'image/jpeg'` check in `posts.ts`:

```typescript
if (!imageBuffer || !isJpegBuffer(imageBuffer)) {
  return reply.status(400).send(actionError('invalid_image_type'));
}
```

**Alternative — `sharp` (already likely available or small install):**

`sharp` is a native image-processing library. Calling `await sharp(imageBuffer).metadata()` rejects with an error on invalid/non-image input and returns `{ format: 'jpeg' }` for a real JPEG. This gives stronger validation (libvips parses the full header, not just 3 bytes) but adds a native dependency.

Since the project currently has no image processing dependency and the use case is write-once validation for a single format (JPEG), the inline probe is the right minimal choice. Only add `sharp` if the API ever needs image resizing or additional format support.

**What NOT to do:**
- Do not trust `part.mimetype` — it is the `Content-Type` as declared by the client, trivially spoofed.
- Do not add `file-type` (sindresorhus) for this fix — it is ESM-only at v20+, supports 100+ formats, and the overhead is disproportionate when the format is already fixed to JPEG.

**Confidence:** HIGH — JPEG SOI marker `FF D8 FF` is the JPEG specification definition; confirmed in multiple sources. The inline probe is the pattern used by `is-jpg` (sindresorhus, v3) and described in official file-signature references.

---

### 4. Production Logging — Fastify Pino Configuration

**CONCERNS.md:** "Production logging disabled — `logger: false` unconditionally" (Tech Debt)

**Recommended approach: `envToLogger` map, official Fastify pattern**

This is the exact pattern from the Fastify official documentation:

```typescript
// api/src/app.ts
const envToLogger: Record<string, unknown> = {
  development: {
    transport: {
      target: 'pino-pretty',
      options: { translateTime: 'HH:MM:ss Z', ignore: 'pid,hostname' },
    },
  },
  production: {
    serializers: {
      req(req) { return { method: req.method, url: req.url }; },
      res(res) { return { statusCode: res.statusCode }; },
    },
    redact: ['req.headers.authorization'],
  },
  test: false,
};

const app = Fastify({ logger: envToLogger[env.NODE_ENV] ?? true });
```

- In `test`: logger is `false` — no log noise in Vitest output (preserves existing test behaviour).
- In `development`: pretty-printed, human-readable.
- In `production`: structured JSON, redacts Authorization headers, captures method/URL/status code.

**Install `pino-pretty` as a dev dependency only** (it is never needed in production JSON mode):

```bash
npm install -D pino-pretty
```

**What NOT to do:**
- Do not log the full request body in production serializers — it may contain passwords or save data.
- Do not set `level: 'debug'` in production — info is the correct default; debug floods PM2 logs on Lightsail.
- Do not remove the `test: false` branch — Vitest output becomes unreadable.

**Confidence:** HIGH — verified against the Fastify official logging reference (`fastify.dev/docs/latest/Reference/Logging/`).

---

### 5. JWT Token Revocation / Blocklist on Logout

**CONCERNS.md:** "JWT access token lifetime is 365 days — implement token revocation (blocklist) for logout/compromise" (Security)

**Context:** The 365-day lifetime is intentional (console UX, no auto-refresh on device). The fix is: when a user calls `POST /auth/logout` or an admin needs to revoke a token, the access token's `jti` is stored in a Postgres-backed denylist, and every subsequent authenticated request checks the denylist.

**Recommended approach: `jti` denylist via `@fastify/jwt` `trusted` option + Prisma `RevokedToken` model**

**Step 1 — Add `jti` when signing access tokens**

`@fastify/jwt` supports adding arbitrary claims to `jwtSign`. Add `jti` using `crypto.randomUUID()`:

```typescript
// lib/auth-tokens.ts
import { randomUUID } from 'node:crypto';

const token = await reply.jwtSign(
  { sub: user.id, username: user.username, jti: randomUUID() },
  { expiresIn: env.JWT_ACCESS_EXPIRES },
);
```

**Step 2 — Prisma schema: add `RevokedToken` model**

```prisma
model RevokedToken {
  jti       String   @id          // UUID
  userId    String
  expiresAt DateTime             // copied from token exp; used for cleanup
  revokedAt DateTime @default(now())

  @@index([expiresAt])           // for periodic cleanup query
}
```

**Step 3 — Wire `trusted` option in `registerAuth`**

The `@fastify/jwt` `trusted` callback fires after signature verification, before the route handler. Return `false` to reject:

```typescript
await app.register(import('@fastify/jwt'), {
  secret: env.JWT_SECRET,
  trusted: async (_request, decodedToken) => {
    const jti = (decodedToken as { jti?: string }).jti;
    if (!jti) return true;                      // tokens without jti: pass through (backward compat)
    const revoked = await app.prisma.revokedToken.findUnique({ where: { jti } });
    return revoked === null;
  },
});
```

**Step 4 — Revoke on logout**

```typescript
// POST /auth/logout
await app.prisma.revokedToken.create({
  data: { jti, userId, expiresAt: tokenExpiry },
});
await revokeRefreshToken(app.prisma, parsed.data.refreshToken);
```

**Step 5 — Periodic cleanup (optional but recommended)**

Tokens expire after 365 days. The denylist grows at `O(1 entry per logout)`. A scheduled cleanup (or startup sweep) that deletes entries where `expiresAt < now()` keeps the table bounded:

```typescript
await app.prisma.revokedToken.deleteMany({
  where: { expiresAt: { lt: new Date() } },
});
```

**Backward compatibility:** Tokens issued before this change have no `jti` claim. The `trusted` callback must pass them through (`if (!jti) return true`). This is the correct trade-off: pre-existing long-lived tokens cannot be revoked until they expire naturally, but all tokens issued after the deployment are revocable immediately.

**Performance:** Each authenticated request incurs one Prisma `findUnique` on a `@id` (primary key) lookup — sub-millisecond on Postgres with a hot connection pool. The `@@index([expiresAt])` speeds up the cleanup query. No Redis/caching layer needed at the current traffic scale.

**What NOT to do:**
- Do not attempt to revoke existing tokens issued without `jti` — impossible without breaking all active console sessions.
- Do not use a separate `tokenVersion` counter on the `User` model ("revoke all") — the CONCERNS.md requirement is logout/compromise revocation, not forced rotation. Keep it scoped.
- Do not store the full JWT in the denylist — only the `jti` UUID. Storing the full token leaks the secret at rest in the DB.
- Do not add Redis just for this — the Postgres lookup is fast enough and keeps infrastructure minimal (aligns with project constraint "no new heavy dependencies").

**Confidence:** HIGH — `trusted` option is documented in the `@fastify/jwt` README (verified via WebFetch of GitHub repo). JTI denylist pattern is a standard security recommendation (Auth0, OWASP).

---

### 6. `cloudBusy` — Make Threading Contract Safe

**CONCERNS.md:** "`cloudBusy` threading contract — plain `bool` member accessed from async workers" (Fragile)

**Current state:** `cloudBusy` is a plain `bool` member of `SaveDetailActivity`. All mutations go through `brls::sync` closures (which post back to the Borealis main thread), so there is no data race today. The risk is a future refactor that removes the `brls::sync` wrapper.

**Recommended approach: `std::atomic<bool>` + comment**

Replace the plain `bool` with `std::atomic<bool>`:

```cpp
// save_detail_activity.hpp
std::atomic<bool> cloudBusy{false};
```

Usage remains identical: `cloudBusy.load()` and `cloudBusy.store(true/false)` (or `= true/false` with the implicit conversion). No memory-ordering arguments needed for a UI flag — the default `memory_order_seq_cst` is correct and the perf cost is negligible on a UI thread.

Add a comment documenting the invariant:

```cpp
// cloudBusy guards against concurrent upload/download.
// Reads and writes MUST occur on the Borealis main thread (inside brls::sync).
// std::atomic<bool> makes any accidental off-thread access race-free.
std::atomic<bool> cloudBusy{false};
```

**What NOT to do:**
- Do not introduce a `std::mutex` for this flag — it is overkill for a single boolean and could deadlock if brls::sync ever serialises with brls::async.
- Do not use `volatile bool` — `volatile` does not provide memory ordering guarantees; it is not a substitute for `std::atomic`.

**Confidence:** HIGH — `std::atomic<bool>` semantics and the default `seq_cst` ordering are from the C++20 standard (cppreference.com). The existing `alive` member in the same file is already `std::atomic<bool>` — this is consistent with the existing codebase style.

---

### 7. `alive` Guard — `runAsync` Base-Class Wrapper

**CONCERNS.md:** "`alive` shared_ptr pattern easy to omit in new async blocks" (Fragile)

**Current state:** Every activity declares `std::shared_ptr<std::atomic<bool>> alive` and every `brls::async` block must manually capture it and check `if (!alive->load()) return`. Forgetting this in one new lambda is a silent use-after-free.

**Recommended approach: `runAsync` helper method on a shared activity base**

Add a base class (or mixin) that wraps `brls::async` + `brls::sync` and auto-inserts the alive guard:

```cpp
// source/app/activity_base.hpp
#pragma once
#include <borealis.hpp>
#include <atomic>
#include <functional>
#include <memory>

namespace thomaz {

class ActivityBase : public brls::Activity {
public:
    ActivityBase()
        : alive(std::make_shared<std::atomic<bool>>(true)) {}

    ~ActivityBase() override { *alive = false; }

protected:
    // Run `work` on the async pool, then `then` on the main thread —
    // but only if this activity is still alive when `then` fires.
    // `then` receives the result of `work`.
    template <typename R>
    void runAsync(
        std::function<R()> work,
        std::function<void(R)> then)
    {
        auto guard = this->alive;
        brls::async([guard, work = std::move(work), then = std::move(then)]() {
            R result = work();
            brls::sync([guard, result = std::move(result), then = std::move(then)]() {
                if (!guard->load()) return;
                then(std::move(result));
            });
        });
    }

    // Convenience overload: no return value.
    void runAsync(
        std::function<void()> work,
        std::function<void()> then)
    {
        auto guard = this->alive;
        brls::async([guard, work = std::move(work), then = std::move(then)]() {
            work();
            brls::sync([guard, then = std::move(then)]() {
                if (!guard->load()) return;
                then();
            });
        });
    }

    std::shared_ptr<std::atomic<bool>> alive;
};

} // namespace thomaz
```

Activities that migrate to `ActivityBase` inherit `alive` and `runAsync`, and can delete their own `alive` declarations and manual guard checks.

**Migration strategy:** Migrate activities one at a time. Start with `SaveDetailActivity` (most complex async logic). Existing activities that keep their own `alive` member continue to work.

**What NOT to do:**
- Do not use `std::stop_token` (C++20 `jthread`) for this — Borealis' thread pool does not integrate with `std::stop_source`; the `alive` shared_ptr is the correct idiom for this UI framework.
- Do not template the base class with CRTP just to avoid `std::function` overhead — the UI dispatch cost dwarfs any virtual-dispatch difference, and `std::function` keeps the API simple and testable.

**Confidence:** MEDIUM-HIGH — the pattern is a clean application of standard C++ RAII and `std::function`; no exotic APIs. The specific Borealis API (`brls::async`, `brls::sync`) is verified by reading the existing codebase. No official Borealis docs were consulted for the template wrappers — the wrapping technique is standard C++.

---

### 8. In-Flight libcurl Cancellation via `CURLOPT_XFERINFOFUNCTION`

**CONCERNS.md:** "`brls::async` pool exhaustion — no cancellation mechanism for in-flight curl requests on activity destruction" (Fragile)

**Recommended approach: `std::atomic<bool> cancelled` flag + `CURLOPT_XFERINFOFUNCTION` abort**

libcurl's only supported way to abort a transfer mid-flight is to return a non-zero value from a `CURLOPT_XFERINFOFUNCTION` progress callback. The callback fires frequently during transfer.

**Requirements (from libcurl official docs):**
1. `CURLOPT_NOPROGRESS` must be set to `0` (it defaults to `1`, disabling the callback).
2. The progress callback must return non-zero to abort (`CURLE_ABORTED_BY_CALLBACK` is returned to the caller).

**Implementation sketch:**

```cpp
// In http_client_curl.cpp (or a cancel-aware wrapper)
struct CurlRequest {
    CURL* handle;
    std::atomic<bool> cancelled{false};
};

// Progress callback — fires every ~100ms during transfer
static int xferinfo(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* req = static_cast<CurlRequest*>(userdata);
    return req->cancelled.load() ? 1 : 0;   // non-zero = abort
}

// Attach to handle:
curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, xferinfo);
curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &req);
```

**Integration with the activity alive pattern:**

The activity should hold a reference to the in-flight `CurlRequest` and set `cancelled = true` in its destructor (after `*alive = false`). The existing `alive` guard already prevents the sync callback from touching freed UI state; the `cancelled` flag additionally aborts the network transfer so the async pool thread frees up sooner.

This is a separate flag from `alive` because `alive` is shared across all lambdas (including UI-only ones) and its lifetime is managed by `shared_ptr` across threads. The `cancelled` flag is per-request and is owned by the request object.

**What NOT to do:**
- Do not call `curl_easy_cleanup` from the activity destructor on a handle that is still in use on a pool thread — libcurl operations are not thread-safe on the same handle.
- Do not use `curl_multi_*` just to add cancellation — the existing code uses the easy interface; switching to multi would be a significant refactor.
- Do not set `CURLOPT_TIMEOUT` as the sole cancellation mechanism — a short timeout that causes false failures in poor network conditions is worse than pool saturation on a Switch.

**Confidence:** HIGH — `CURLOPT_XFERINFOFUNCTION` return value behaviour and `CURLOPT_NOPROGRESS` requirement verified on the official libcurl documentation page (`curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html`).

---

## Installation Changes

```bash
# API — only pino-pretty is a new dependency; everything else uses existing packages
npm install -D pino-pretty       # dev only; not needed in production JSON mode

# No new prod dependencies required for any of the above fixes
```

The `RevokedToken` Prisma model requires a migration:

```bash
cd api && npx prisma migrate dev --name add_revoked_tokens
```

---

## Summary: What NOT to Add

| Avoid | Why | Instead |
|-------|-----|---------|
| `file-type` (npm) | ESM-only v20+, 100-format overhead, overkill for single-format JPEG check | Inline 3-byte SOI probe in pure TypeScript |
| `sharp` (for validation only) | Native binary dep, significant install overhead | Inline probe; add sharp later only if resizing is needed |
| Redis | Adds infra complexity for a one-row PK lookup | Postgres `findUnique` on `jti` is fast enough at current scale |
| `std::mutex` on `cloudBusy` | Overkill; potential deadlock with brls::sync | `std::atomic<bool>` |
| `curl_multi_*` | Full interface change just to cancel | `CURLOPT_XFERINFOFUNCTION` on easy interface |
| `volatile bool` | No memory-order guarantees; not a substitute for atomic | `std::atomic<bool>` |

---

## Version Compatibility

| Package | Version in use | Notes |
|---------|---------------|-------|
| `@fastify/static` | 8.1 | `allowedPath` option available since v6; no upgrade needed |
| `@fastify/multipart` | 9.0 | `fieldSize` limit is a busboy passthrough; no upgrade needed |
| `@fastify/jwt` | 9.1 | `trusted` callback available since v7; no upgrade needed |
| `zod` | 3.25 | `.max()` is core API; no upgrade needed |
| `pino-pretty` | latest | dev-only; does not affect production build |
| `std::atomic<bool>` | C++20 | Already used for `alive` in the codebase |
| `CURLOPT_XFERINFOFUNCTION` | libcurl ≥7.32.0 | Available in all Switch portlib versions |

---

## Sources

- Fastify official logging docs (`fastify.dev/docs/latest/Reference/Logging/`) — `envToLogger` pattern, serializers, redact — HIGH confidence
- `@fastify/static` GitHub README (verified via WebFetch) — `allowedPath` filter, `serve: false` — HIGH confidence
- `@fastify/jwt` GitHub README (verified via WebFetch) — `trusted` callback signature — HIGH confidence
- libcurl official docs `curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html` — return value to abort, `CURLOPT_NOPROGRESS` requirement — HIGH confidence
- `@fastify/multipart` GitHub `examples/example-with-zod.ts` (verified via WebFetch) — `fieldSize`, field preprocessing — HIGH confidence
- JPEG SOI marker definition `filesignature.org/jpg` + `github.com/sindresorhus/is-jpg` — `FF D8 FF` first 3 bytes — HIGH confidence
- Auth0 denylist blog `auth0.com/blog/denylist-json-web-token-api-keys/` — JTI denylist fields, cleanup strategy — MEDIUM confidence
- cppreference.com `std::atomic` — C++20 `std::atomic<bool>` semantics — HIGH confidence

---

*Stack research for: thomaz hardening milestone*
*Researched: 2026-06-04*
