# Feature Research — Hardening Milestone: Correct Behaviors

**Domain:** Security, concurrency, and quality hardening for a Switch homebrew hub (Borealis C++ client + Fastify/Prisma/Postgres API)
**Researched:** 2026-06-04
**Confidence:** HIGH — all behaviors derived directly from the codebase (routes, headers, activity code), well-established HTTP semantics, and C++ memory-model standards. No speculative claims.

---

## Context

This is not a feature-addition milestone. "Features" here means the six fix classes surfaced by `.planning/codebase/CONCERNS.md`. Each section below answers: what does "done right" look like, stated as testable acceptance criteria, with testability mode and trade-off flags noted.

**Intentional trade-offs that MUST NOT change** (per PROJECT.md Out of Scope):
- TLS fail-safe behavior: `ca_ok == false` path keeps networking alive (no change).
- JWT access token lifetime: 365-day default stays (console UX constraint).

---

## Fix Class 1: Access Control for Private User Blobs (Save Files)

**CONCERNS.md ref:** "Save blobs are publicly accessible via static file serving" (Security, HIGH)
**Root cause:** `@fastify/static` serves the entire `UPLOAD_DIR` at `/uploads/`. Save blobs live at `uploads/saves/<userId>/<titleId>.bin` — a predictable path that any caller can guess.

### Correct Behavior

**Architecture decision (must pick one):**

Option A — Route-level auth (preferred, zero new dependencies):
- Remove the `saves/` subdirectory from the static-served tree. The static plugin remains for post images (`uploads/<uuid>.jpg`) only.
- Add a dedicated `GET /saves/:titleId/blob` route protected by `app.authenticate`, which reads the blob with `readSaveBlob` and streams it back. The existing `GET /saves/:titleId?includeData=1` endpoint already does this correctly via base64; a raw binary endpoint is an alternative but the existing pattern is sufficient.

Option B — Move blobs outside `UPLOAD_DIR`:
- Store save blobs in a directory that is never registered with `@fastify/static` (e.g., `./save-blobs/` separate from `./uploads/`).
- Post images stay in `./uploads/` and remain static-served.

**Either option must satisfy all of the following acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-1.1 | Unauthenticated `GET /uploads/saves/<anyUserId>/<anyTitleId>.bin` | **404** (path not served) or **403** (blocked by middleware). Must not return 200 with file content. |
| AC-1.2 | Authenticated request by the **wrong user** attempting to access another user's save blob via any API route | **403** Forbidden — not 404. The owning userId is embedded in the blobKey; cross-user access is categorically denied, not treated as "not found." |
| AC-1.3 | Authenticated request by the **correct owner** to retrieve save data | **200** with data (existing `?includeData=1` path already correct; blob fetch path must mirror this). |
| AC-1.4 | Unauthenticated request to any `/saves/*` API route | **401** Unauthorized (existing `app.authenticate` preHandler already enforces this; must not regress). |
| AC-1.5 | Post images remain publicly accessible at `GET /uploads/<uuid>.jpg` | **200** — post images are intentionally public (community feed). Do not remove static serving for the image prefix. |

**Status code semantics:**
- 401 = no valid JWT present at all.
- 403 = valid JWT, but the authenticated user is not the resource owner.
- 404 is acceptable for AC-1.1 only because the URL prefix simply no longer exists in the routing table — it is not a deliberate information-hiding choice (the blobKey structure is not secret once auth is required to reach it).

**Testability:**
- AC-1.1, AC-1.2, AC-1.3, AC-1.4: **Host-testable via Vitest** using `app.inject()`. No hardware required.
- AC-1.5: **Host-testable via Vitest**.
- A regression-guard test must be added (CONCERNS.md: "No test for save blob public URL accessibility").

---

## Fix Class 2: Upload Validation — Magic Bytes, not Content-Type

**CONCERNS.md ref:** "MIME type spoofing for image uploads" (Security)
**Root cause:** `posts.ts` line 40/60–62 trusts `part.mimetype` (the `Content-Type` from the multipart part, which is client-supplied) to decide if the upload is a JPEG.

### Correct Behavior

A valid JPEG file begins with the two-byte SOI (Start Of Image) marker: `0xFF 0xD8`. A valid PNG begins with `\x89PNG\r\n\x1a\n` (8 bytes). Any other prefix means the file is not what the client claims.

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-2.1 | Upload with `Content-Type: image/jpeg` but file content that does NOT start with `0xFF 0xD8` | **400** `invalid_image_type` — rejected before `saveJpeg` writes anything to disk. |
| AC-2.2 | Upload with `Content-Type: image/png` (or any non-JPEG type) AND content starting with `0xFF 0xD8` | **400** `invalid_image_type` — current code already rejects non-JPEG MIME strings; this behavior must be preserved (only JPEG accepted). |
| AC-2.3 | Upload with `Content-Type: image/jpeg` AND content starting with `0xFF 0xD8` AND size ≤ 5 MB | **200** `{ ok: true, post: {...} }` — accepted as before. |
| AC-2.4 | Upload with `Content-Type: image/jpeg` AND valid JPEG magic AND size > 5 MB | **413** `image_too_large` — existing size limit must not regress. |
| AC-2.5 | Upload with `Content-Type: image/jpeg` AND valid JPEG magic AND size = 0 | **400** `missing_image` (existing `empty_image` check, must not regress). |

**Implementation note:** The magic check requires the `imageBuffer` to be fully buffered before the check (already the case — chunks are collected before `saveJpeg` is called). No streaming changes needed. A 2-byte prefix slice suffices: `imageBuffer.subarray(0, 2)` compared to `Buffer.from([0xff, 0xd8])`. No new library dependency required — the existing buffer is available at line 44 before the MIME check at line 60.

**Testability:** All AC-2.x criteria are **host-testable via Vitest** using crafted multipart payloads with `app.inject()`. No hardware required.

---

## Fix Class 3: Input Length Cap — `caption` Field

**CONCERNS.md ref:** "`caption` field in posts has no max-length schema guard" (Tech Debt)
**Root cause:** `posts.ts` lines 48–49 assigns `caption = value` from the raw multipart field without validation. The Prisma `caption` column is `String` (PostgreSQL `text`, unbounded).

### Correct Behavior

**Limit rationale:** 500 characters is the community standard for a "social caption" (Instagram uses 2,200 but that is for a dedicated long-form use case; Switch game screenshots are a micro-social use case closer to Twitter/X's 280). 500 is the CONCERNS.md suggested value and is appropriate.

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-3.1 | `caption` field absent from the multipart body | **200** — caption defaults to `""` (existing behavior; empty string is valid). |
| AC-3.2 | `caption` field present, length ≤ 500 characters (Unicode code points) | **200** — post created with the provided caption. |
| AC-3.3 | `caption` field present, length = 501 characters | **400** `invalid_body` — rejected before DB write. |
| AC-3.4 | `caption` field present, length = 0 (empty string) | **200** — empty string is valid (same as absent). |
| AC-3.5 | Validation failure must not leave a dangling image file on disk | The `saveJpeg` call happens inside the `try` block after caption parsing; if caption validation is added before `saveJpeg`, no cleanup is needed. If reordering is required, ensure no partial state. |

**Implementation note:** Add a `captionSchema = z.string().max(500).default("")` and validate the collected `caption` string before calling `saveJpeg`. The multipart loop must complete first (file must be consumed even on field errors, to drain the stream). Validate after the loop.

**Testability:** All AC-3.x criteria are **host-testable via Vitest**. No hardware required.

---

## Fix Class 4: TLS Fail-Safe UX Warning

**CONCERNS.md ref:** "TLS verification silently disabled on CA bundle failure" (Security)
**Root cause:** `curl_tls.hpp` lines 31–35: when `ca_ok == false`, all HTTPS proceeds with `SSL_VERIFYPEER=0` / `SSL_VERIFYHOST=0` and no user-visible signal is emitted.

**INTENTIONAL TRADE-OFF — DO NOT CHANGE:** The fail-safe behavior itself (keeping networking alive rather than bricking the app when the CA bundle is missing) is an explicit PROJECT.md Out of Scope item. The fix is a warning overlay only.

### Correct Behavior

A visible, persistent on-screen indicator must appear when `ca_ok == false`. It must:
1. Be displayed before any network operation occurs (i.e., at startup/`onContentAvailable` time if the flag is already false, not deferred to first network call).
2. Communicate the security implication without being a blocking dialog (which would be frustrating if triggered by a build defect).
3. Use language a Switch homebrew user can understand.

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-4.1 | `ca_ok == true` (normal packaging) | No warning displayed. |
| AC-4.2 | `ca_ok == false` (CA bundle probe fails) | A persistent on-screen label or banner is visible, reading something equivalent to: "Warning: Certificate verification unavailable. Your connection may not be secure." |
| AC-4.3 | The TLS fail-safe behavior when `ca_ok == false` | **UNCHANGED** — `SSL_VERIFYPEER=0` / `SSL_VERIFYHOST=0` remain; no exception thrown; networking continues. |
| AC-4.4 | The warning does not appear in desktop builds | Desktop `#else` branch always uses system CA; warning is `#ifdef __SWITCH__` only. |
| AC-4.5 | `ca_ok` state is accessible to the activity layer without calling `apply_curl_tls` on a live handle | Expose a `bool thomaz::tls_verified()` free function (or similar) that returns `ca_ok`; activities query it on `onContentAvailable`. |

**Implementation note:** `ca_ok` is a `static const bool` initialized once via an IIFE in `apply_curl_tls`. A thin accessor `bool thomaz::tls_verified()` that returns that same value (or a module-level flag set during init) can be called from any activity without requiring a CURL handle.

**Testability:**
- AC-4.2, AC-4.3, AC-4.5: **Host-testable via doctest** — the `ca_ok == false` branch can be exercised by providing a fake/absent CA path in a fake impl. CONCERNS.md explicitly flags this: "No test covers the TLS fail-safe branch."
- AC-4.1, AC-4.4: **Host-testable** for the non-Switch path; Switch `ca_ok == true` path requires **hardware verification** (romfs mount must succeed).

---

## Fix Class 5: Token Revocation / Blocklist on Logout and Compromise

**CONCERNS.md ref:** "JWT access token lifetime is 365 days by default" / "Token stored in session without expiry check on client" (Security)
**Root cause:** Access tokens are JWTs with a 365-day lifetime. If a token is stolen, `revokeRefreshToken` in `auth.ts` only removes the refresh token from the DB — the bearer access token itself remains cryptographically valid until expiry. No blocklist exists.

**INTENTIONAL TRADE-OFF — DO NOT CHANGE:** The 365-day JWT lifetime is an explicit PROJECT.md Out of Scope item. The fix adds revocation as a safety net, not a lifetime reduction.

### Correct Behavior

On **logout** (`POST /auth/logout`):
- The refresh token is deleted (already implemented in `revokeRefreshToken`).
- The access token (JWT) that was used in the logout request must be added to an in-memory or DB-backed blocklist so it cannot authenticate further requests within its remaining validity window.

On **compromise** (admin revoke, future use):
- The same blocklist mechanism must support adding arbitrary JWTs by `jti` (JWT ID) claim or by userId (revoke-all for a user).

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-5.1 | `POST /auth/logout` with a valid refresh token AND a valid access token in `Authorization` header | **200** `{ ok: true }` — refresh token deleted AND access token added to blocklist. |
| AC-5.2 | After logout, a request to any authenticated endpoint using the now-revoked access token | **401** `unauthorized` — even though the JWT is not yet expired by its `exp` claim. |
| AC-5.3 | After logout, a request using a *different* valid access token (different user or different login session) | **200** — revocation is scoped to the specific revoked token, not the user or all tokens. |
| AC-5.4 | `POST /auth/logout` called without an `Authorization` header (refresh-only logout) | **200** `{ ok: true }` — refresh token is still deleted; no access token to blocklist (graceful degradation). |
| AC-5.5 | `POST /auth/refresh` called with a revoked refresh token | **401** `invalid_refresh_token` (already implemented; must not regress). |
| AC-5.6 | The blocklist does not require a running Redis or new infrastructure dependency | Acceptable: DB-backed `RevokedToken` table with `(jti, expiresAt)` columns, or in-memory Map for the process lifetime. DB-backed is preferred for multi-process / restart resilience on the Lightsail PM2 deployment. |
| AC-5.7 | Blocklist entries are pruned after the token's original `exp` time passes | Expired entries need not be checked (a JWT that has expired by its `exp` claim is already rejected by `@fastify/jwt`); cleanup can be lazy (on insert, on a cron, or accept accumulation over 365 days — documented). |
| AC-5.8 | Existing 365-day access tokens already issued to console clients continue to work until logout or explicit revocation | **200** — no retroactive revocation of currently-valid tokens. |

**Implementation note:** Add a `jti` claim to all newly issued JWTs (`reply.jwtSign({ sub, username, jti: randomUUID() }, ...)`). The `authenticate` decorator must check the blocklist after successful signature verification. The logout route must extract the `jti` from the bearer token (if present) before returning. The `POST /auth/logout` request body already carries the refresh token; the access token is in the `Authorization` header.

**Testability:** All AC-5.x criteria are **host-testable via Vitest**. No hardware required. The existing test scaffold (`app.inject()`) already exercises auth flows.

---

## Fix Class 6: Concurrency Correctness — `cloudBusy`, `alive`, and Async Pool

**CONCERNS.md ref:** Three fragile areas — "`cloudBusy` flag with non-atomic access", "`alive` shared_ptr pattern as lifetime guard", "`brls::async` pool exhaustion on simultaneous operations."
**Root cause:** See `.planning/codebase/CONCERNS.md` Fragile Areas section.

### Correct Behavior

This fix class defines observable safety invariants rather than functional behavior changes. The behavior as seen by the user must not change; only the implementation contract must be made hard to violate.

#### 6A: `cloudBusy` Threading Contract

**Current state:** `cloudBusy` is a plain `bool` member. All reads/writes happen inside `brls::sync` closures (UI thread callbacks), which is safe today. But the invariant is undocumented and structurally fragile — removing a `brls::sync` wrapper in a future edit would silently introduce a data race.

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-6A.1 | `cloudBusy` is declared `std::atomic<bool>` in `save_detail_activity.hpp` | No UB if any future call site reads/writes it off-thread without a `brls::sync` wrapper. |
| AC-6A.2 | All existing reads/writes of `cloudBusy` in `save_detail_activity.cpp` continue to compile with the `std::atomic<bool>` type | `load()` / `store()` / direct assignment (which calls `store`) must be used consistently. |
| AC-6A.3 | Behavior is unchanged: the guard still prevents concurrent `doUpload`/`doDownload` calls | If `cloudBusy.load() == true`, the second call returns immediately (existing `if (this->cloudBusy) return;` gates, now using `.load()`). |

**Testability:** AC-6A.1 and AC-6A.2 are **host-verifiable via a clean desktop build** (`-DUSE_SDL2=ON`). Behavioral correctness (AC-6A.3) requires **hardware verification** (concurrent button presses), but the structural fix is verified by compilation.

#### 6B: `alive` Guard — `runAsync` Wrapper

**Current state:** Every `brls::async` block in every activity must manually capture `alive` and check `if (!alive->load()) return;` inside the `brls::sync` callback. Forgetting this in any new block silently introduces a use-after-free.

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-6B.1 | A `runAsync(std::function<void()> work, std::function<void()> onDone)` method (or equivalent CRTP wrapper) is added to an activity base class | The wrapper auto-captures `alive` and wraps `onDone` in `brls::sync([alive, onDone]{ if (alive->load()) onDone(); })`. |
| AC-6B.2 | Existing `brls::async` call sites in `save_detail_activity.cpp`, `mod_browser_activity.cpp`, and `theme_browser_activity.cpp` are migrated to use `runAsync` | The manual `alive` capture-and-check pattern is removed from call sites. |
| AC-6B.3 | The `alive` member remains in the activity (`shared_ptr<atomic<bool>>`); `runAsync` captures it by value in the lambda | The shared ownership model is preserved — the `alive` flag outlives the activity destruction as before. |
| AC-6B.4 | If an activity is destroyed while a `runAsync` work task is in flight, the `onDone` callback is silently dropped (not called) | No crash, no assertion failure. This is the existing `alive` guard semantics; the wrapper must replicate them. |

**Testability:** AC-6B.1 and AC-6B.2 are **host-verifiable via a clean desktop build**. AC-6B.4 can be **tested in a host doctest** by constructing a fake activity, queuing a work task, destroying the activity before the task completes (using a fake async executor), and asserting the callback is not invoked. Hardware testing is required to verify no regression in the real Borealis thread scheduler.

#### 6C: `brls::async` In-Flight Request Cancellation

**Current state:** When an activity is destroyed (user navigates away), in-flight `brls::async` tasks (e.g., `c->getStatus(...)`, `c->pull(...)`) run to completion even though their `brls::sync` callbacks are safely dropped by the `alive` check. On slow networks this can saturate the fixed-size async pool.

**Acceptance criteria:**

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-6C.1 | Each activity that issues curl requests adds a `std::atomic<bool> cancelled` flag, initialized `false`, set `true` in the destructor | The flag is distinct from `alive` (which guards the UI callback); `cancelled` guards the network I/O itself. |
| AC-6C.2 | Long-running curl operations (specifically `ICloudSaveClient` and `IHttpClient` implementations) check `cancelled` via a `CURLOPT_PROGRESSFUNCTION` or `CURLOPT_XFERINFOFUNCTION` callback that returns `1` (abort) when `cancelled.load() == true` | The curl request is aborted mid-transfer rather than completing to drain the pool slot. |
| AC-6C.3 | The `cancelled` flag is shared (e.g., `shared_ptr<atomic<bool>>`) between the activity and the lambda, so the lambda can read it safely after the activity is destroyed | Same ownership pattern as `alive`. |
| AC-6C.4 | A normal (non-cancelled) request is not affected: `CURLOPT_XFERINFOFUNCTION` returning `0` allows curl to proceed | No regression in happy-path network calls. |

**Testability:**
- AC-6C.1: **Host-verifiable via clean desktop build**.
- AC-6C.2, AC-6C.4: **Host-testable via doctest** using a fake `IHttpClient` that checks the cancellation flag during a simulated slow transfer. Full network-layer cancellation requires **hardware verification** on slow real networks.
- AC-6C.3: **Host-verifiable via clean desktop build** (shared_ptr type enforcement).

---

## Ancillary Acceptance Criteria (Supporting Fixes)

These are not fix classes in themselves but must hold alongside the six above.

### API Logging

**CONCERNS.md ref:** "Production logging disabled" (Tech Debt)

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-L.1 | `NODE_ENV=production` → Fastify logger enabled with pino defaults or configured serializers | Request lines and error traces appear in PM2 log output. |
| AC-L.2 | `NODE_ENV=test` → Fastify logger disabled | Test output is not polluted by request logs. Existing test suite must pass. |
| AC-L.3 | `NODE_ENV=development` → logger enabled (developer convenience) | Developer can see request traces locally. |

**Testability:** AC-L.2 is **host-testable via Vitest** (test suite must remain clean). AC-L.1 and AC-L.3 require **manual inspection of PM2 / terminal output**.

### Saves `PUT` Revision Conflict Tests

**CONCERNS.md ref:** "No API integration tests for the saves `PUT` conflict / revision path" (Test Coverage Gaps, HIGH)

| # | Criterion | Expected result |
|---|-----------|-----------------|
| AC-T.1 | `PUT /saves/:titleId` with an existing slot and `revision` field absent from body | **400** `revision_required` |
| AC-T.2 | `PUT /saves/:titleId` with an existing slot and `revision` not matching `existing.revision` | **409** `revision_conflict` |
| AC-T.3 | `PUT /saves/:titleId` creating a new slot (no existing) with `revision=0` | **200** `{ ok: true, slot: { revision: 1 } }` |
| AC-T.4 | `PUT /saves/:titleId` with `revision` matching `existing.revision` | **200** `{ ok: true, slot: { revision: existing.revision + 1 } }` |

**Testability:** All AC-T.x criteria are **host-testable via Vitest**. These are pure API integration tests.

---

## Feature Dependencies

```
AC-1 (blob access control)
    └──enables──> AC-T blob security regression test (AC-1.1 test guards the fix)

AC-5 (token revocation)
    └──requires──> jti claim added to JWT issuance
                       └──required by──> blocklist lookup in authenticate decorator

AC-6B (runAsync wrapper)
    └──requires──> activity base class modification
                       └──used by──> AC-6C (cancellation flag wired through same runAsync)

AC-6A (atomic cloudBusy) ──independent──> all other fix classes
AC-2 (magic bytes) ──independent──> all other fix classes
AC-3 (caption length) ──independent──> all other fix classes
AC-4 (TLS warning) ──independent──> all other fix classes
```

---

## Fix Class Prioritization

| Fix Class | Security Impact | Break Risk | Host-Testable | Priority |
|-----------|----------------|------------|---------------|----------|
| AC-1: Save blob access control | HIGH — data exfiltration | LOW — adds auth gate, backward-compat for API clients | Yes (Vitest) | P1 |
| AC-5: Token revocation | HIGH — stolen token window | LOW — additive; existing tokens unaffected | Yes (Vitest) | P1 |
| AC-2: Upload magic bytes | MEDIUM — content injection | LOW — additive validation | Yes (Vitest) | P1 |
| AC-3: Caption length cap | LOW-MEDIUM — DB abuse | LOW — additive Zod check | Yes (Vitest) | P2 |
| AC-T.1–4: Revision conflict tests | LOW (guards regression) | None | Yes (Vitest) | P2 |
| AC-4: TLS warning UI | LOW (warning only) | LOW — display-only change | Partial (doctest + hardware) | P2 |
| AC-6A: atomic cloudBusy | LOW (structural) | LOW | Build verification | P2 |
| AC-6B: runAsync wrapper | LOW (structural) | LOW-MEDIUM — touches all activities | Build + doctest | P3 |
| AC-6C: curl cancellation | LOW (pool exhaustion) | MEDIUM — curl callback contract | Partial (doctest) | P3 |
| AC-L: API logging | LOW (observability) | LOW | Vitest (test=silent) | P2 |

---

## Anti-Features

Do not implement any of the following; they are explicitly out of scope or would conflict with stated constraints.

| Anti-Feature | Why Avoid | Constraint |
|--------------|-----------|------------|
| Reduce JWT lifetime below 365d | Breaks existing console sessions; console UX constraint | PROJECT.md Out of Scope |
| Add device auto-refresh flow | New feature, not a hardening fix | PROJECT.md Out of Scope |
| Change TLS fail-safe to a hard failure | Would brick the app on packaging errors | PROJECT.md Out of Scope |
| Add mandatory hardware test gate per fix | Blocks CI on hardware availability | PROJECT.md Out of Scope |
| Fix double archive traversal / save status prefetch | Performance; lower priority than security/crash items | PROJECT.md deferred |
| Add Redis for token blocklist | New heavy infrastructure dependency | PROJECT.md constraint: no new heavy deps |

---

## Sources

All findings are derived from direct code inspection. No external sources needed for correctness definitions (HTTP semantics are RFC 7235 / 9110 standard; C++ memory model is ISO C++20).

- `api/src/app.ts` — static file serving root
- `api/src/lib/save-storage.ts` — blob path structure
- `api/src/routes/saves.ts` — existing auth and revision logic
- `api/src/routes/posts.ts` — MIME check and caption handling
- `api/src/routes/auth.ts` — logout, refresh, revoke
- `api/src/lib/refresh-tokens.ts` — refresh token lifecycle
- `api/src/config.ts` — JWT lifetime configuration
- `source/platform/curl_tls.hpp` — TLS fail-safe implementation
- `source/app/save_detail_activity.hpp/.cpp` — cloudBusy, alive pattern
- `source/app/mod_browser_activity.hpp` — alive pattern in another activity
- `.planning/codebase/CONCERNS.md` — issue inventory and severity
- `.planning/PROJECT.md` — out-of-scope and intentional trade-off declarations

---

*Feature research for: thomaz hardening milestone — fix-class correct behaviors*
*Researched: 2026-06-04*
