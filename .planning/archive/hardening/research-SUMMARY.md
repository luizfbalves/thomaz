# Project Research Summary

**Project:** thomaz — Hardening Milestone
**Domain:** Security / concurrency / tech-debt hardening (Borealis C++20 NRO + Fastify/Prisma/Postgres API)
**Researched:** 2026-06-04
**Confidence:** HIGH

## Executive Summary

thomaz is a production Switch homebrew hub with a live API (`api.thomaz.baseup.cc`). This milestone is a fix-the-known-issues pass driven by `.planning/codebase/CONCERNS.md` — no new features. Research covered thirteen distinct fixes across four fronts: API security (save blob auth, MIME validation, caption cap, token revocation, production logging), C++ concurrency (`atomic cloudBusy`, `runAsync` lifetime-guard wrapper, libcurl cancellation), C++ tech debt (duplicated `ensure_parent_dirs`/`copy_tree`, unsafe C-style view casts), and test-coverage gaps (revision conflict tests, blob security regression, TLS fail-safe branch).

All four researchers converged on the same phase structure: API security fixes first (highest impact, host-testable via Vitest), then isolated C++ platform fixes, then the larger C++ activity refactor, with regression tests co-shipped with the API phase. The one genuinely risky step is the live save-blob directory migration on the Lightsail instance (FIX-A1) — all other fixes are either additive or C++-only with no deployment side-effects. No new production dependencies are required anywhere.

Key risks are narrow: the save-blob directory move requires a server-side `cp uploads/saves/ saves/` before the new code goes live; the Prisma `RevokedToken` migration must precede the token-blocklist deploy; and the `runAsync` base-class wrapper must copy `alive` as a `shared_ptr` value before dispatch (not through `this`), or the lifetime guarantee it provides becomes a use-after-free. Both intentional trade-offs — the 365-day JWT lifetime and the TLS fail-safe behavior — are explicitly preserved.

---

## Key Findings

### Recommended Stack (Techniques)

All fixes stay within the existing stack. The only new dev dependency is `pino-pretty`. One new Prisma model (`RevokedToken`) requires `prisma migrate dev` on the live instance.

**Fix-by-fix techniques:**
- **FIX-A1 save blob access control:** Move `saves/` out of `UPLOAD_DIR` into a separate `SAVES_DIR`. The static plugin never sees save files. `blobKey` in the DB stays path-relative; only the base dir changes.
- **FIX-A3 token blocklist:** `jti` via `randomUUID()` on `jwtSign`; Postgres `RevokedToken` table; post-verify check in `authenticate` decorator. No Redis. Primary-key `findUnique` on `jti` is sub-millisecond.
- **FIX-A4 caption cap:** `z.string().max(500).safeParse()` after the multipart loop. Pattern already exists for `commentBodySchema`.
- **FIX-A5 magic bytes:** Inline 2-byte JPEG SOI probe (`0xFF 0xD8`) after the existing buffer loop. No `sharp` or `file-type`.
- **FIX-A6 production logging:** `envToLogger` map per Fastify official docs. `test: false`, `production: { serializers, redact }`. `pino-pretty` dev-only.
- **FIX-C6 cloudBusy:** Change `bool cloudBusy` → `std::atomic<bool> cloudBusy{false}`. Use `compare_exchange_strong` for the check-then-set to eliminate TOCTOU.
- **FIX-C4 runAsync wrapper:** New `ThomazActivity` base class with `alive` shared_ptr and templated `runAsync(work, then)` that copies `alive` before dispatch. Never capture `this` inside async/sync lambdas.
- **FIX-C8 curl cancel:** `CURLOPT_XFERINFOFUNCTION` progress callback returning 1 when `cancelled->load()`. `CURLOPT_NOPROGRESS` must be `0L`. Destructor sets only the flag; never touches the curl handle.
- **FIX-C1/C2 fs_util:** New `source/platform/fs_util.hpp` + `fs_util.cpp`. `ensure_parent_dirs` inline; `copy_tree` in `.cpp`. Both in one commit (shared files).
- **FIX-C3 view casts:** Null-guarded `dynamic_cast`. Verify `brls::View::cast<T>()` existence in vendored Borealis before using it.
- **FIX-C5 TLS warning:** `brls::Logger::warning(...)` inside `ca_ok == false` branch. Keep `ca_ok` `static const bool`. No `CURLOPT_SSL_VERIFYPEER` lines outside `#ifdef __SWITCH__`.

### Expected Behaviors (Fix Acceptance Criteria)

**Fully host-testable via Vitest:** FIX-A1 (blob URL → 404; post images still 200), FIX-A3 (logout → old token → 401), FIX-A4 (caption > 500 → 400), FIX-A5 (valid JPEG magic → 200; PNG magic → 400), FIX-A6 (test env → no logs), saves PUT revision conflict tests.

**Host-testable via clean desktop build:** FIX-C1/C2, FIX-C3, FIX-C4, FIX-C5, FIX-C6.

**Requires hardware for full behavioral verification:** FIX-C4 (alive guard under real navigation), FIX-C8 (cancellation under real network), FIX-C5 (CA bundle probe on real romfs).

### Architecture: Where Each Fix Lands

**Shared-file conflicts (must serialize):**
- **S1** — `platform/fs_util.hpp` + `fs_util.cpp`: FIX-C1 and FIX-C2 share both new files. Do together in one commit.
- **S2** — `save_detail_activity.hpp`: FIX-C6 (atomic) and FIX-C4 (removes `alive` member). Do FIX-C6 first.
- **S3** — `routes/posts.ts`: FIX-A4 (caption) and FIX-A5 (magic bytes). One PR.
- **S4** — `app.ts`: FIX-A1 (`ensureSavesDir` call) and FIX-A6 (logger). Different lines; safe together.
- **S5** — `test/api.test.ts`: FIX-A2 (depends on FIX-A1 being live) and FIX-A7 (independent).

### Critical Pitfalls

1. **Path traversal in blob download** — Any route constructing a blob path from `req.params` directly is vulnerable. Always use DB `blobKey` via `readSaveBlob(env, blobKey)`.
2. **Magic-byte check consuming the stream before buffering** — Buffer first (`Buffer.concat(chunks)`), then inspect `imageBuffer[0..1]`. Never read `part.file` separately.
3. **Blocklist revokes refresh but not access token** — `authenticate` decorator must call `isRevoked(jti)` after `jwtVerify`. Postgres-backed (not in-memory Set). Include `expiresAt` column for pruning.
4. **`runAsync` wrapper capturing `this` inside async body** — Copy `alive` by value before dispatch: `auto guard = this->alive;`. Desktop ASAN during nav-away is the verification.
5. **libcurl cleanup from wrong thread** — Destructor sets `cancelled->store(true)` only. Progress callback (async thread) returns 1. `curl_easy_cleanup` only inside the async thread.
6. **TLS warning moving SSL opts outside `#ifdef __SWITCH__`** — Verify with `grep -n "VERIFYPEER\|VERIFYHOST"` after the edit; all `0L` assignments must be inside the guard.
7. **`ensure_parent_dirs` edge case on `theme_install.cpp`** — Write a doctest for a representative theme path before migrating the call site.

---

## Implications for Roadmap

### Phase 1: API Security + Regression Tests

**Rationale:** Highest-severity items are API security (save blobs publicly readable, no token revocation). Purely server-side, fully host-testable, largest risk reduction. FIX-A4/A5/A6 share file locality (`posts.ts`, `app.ts`) and zero live-API risk. Regression tests (FIX-A7) have no production dependency and ship here.

**Fixes:** FIX-A1, FIX-A2, FIX-A3, FIX-A4, FIX-A5, FIX-A6, FIX-A7

**Live-API risk:** FIX-A1 is MEDIUM (Lightsail migration required). All others LOW. Migration: `cp -r api/uploads/saves/ api/saves/`; verify reads; delete original. Use `cp` not `mv` so original stays intact until verification.

**Pitfalls:** Path traversal (P1), stream-before-buffer (P2), refresh-only revocation (P3), non-atomic logout transaction (P4), Pino logging Authorization header (P9).

**Serialize:** FIX-A4 + FIX-A5 in one PR; FIX-A2 after FIX-A1 is deployed.

---

### Phase 2: C++ Platform Hardening (fs_util + TLS warning + cloudBusy)

**Rationale:** Smallest, most isolated C++ fixes. FIX-C6 must precede Phase 3 because FIX-C4 (Phase 3) edits the same `save_detail_activity.hpp`. All verifiable by clean desktop build. No live-API risk.

**Fixes:** FIX-C1, FIX-C2, FIX-C5, FIX-C6

**Pitfalls:** `ensure_parent_dirs` theme-path edge case (P11), TLS warning breaking static init or platform guard (P10), `atomic<bool>` without TOCTOU fix (P5).

**Serialize:** FIX-C1 + FIX-C2 in one commit (shared new files).

---

### Phase 3: C++ Activity Hardening (runAsync + cast replacements + curl cancellation)

**Rationale:** FIX-C4 is the largest diff (all activity inheritance chains). Doing it last means Phase 2 has cleaned up `save_detail_activity.hpp`. FIX-C3 shares four activity `.cpp` files with FIX-C4 (different lines); serializing avoids merge conflicts. FIX-C8 depends on the `alive`/`runAsync` ownership model from FIX-C4.

**Fixes:** FIX-C3, FIX-C4, FIX-C8

**Pitfalls:** `runAsync` capturing `this` (P6), `brls::sync` deadlock (P8), libcurl cleanup from wrong thread (P7), `static_cast` as cast replacement (P12).

**Coordinate:** FIX-C3 and FIX-C4 touch the same four activity `.cpp` files. Do FIX-C8 after FIX-C4.

---

### Phase Ordering Rationale

- Security first: FIX-A1 and FIX-A3 are HIGH-severity on a live service; fully host-testable so no hardware gate blocks them.
- FIX-C6 in Phase 2 before FIX-C4 in Phase 3: both touch `save_detail_activity.hpp`; linear edit history avoids conflicts.
- FIX-C4 last: largest file-change surface, depends on Phase 2 state.
- Tests co-shipped with their security fix: FIX-A2 is the regression guard for FIX-A1; FIX-A7 has no dependency and ships in Phase 1 for early coverage.

### Research Flags

No phase requires a dedicated research phase. All techniques verified against official documentation.

Standard patterns (skip research phase): Phase 1 (Fastify, Zod, Pino, `@fastify/jwt` — all official-doc verified), Phase 2 (POSIX stdlib, `std::atomic`, existing codebase patterns), Phase 3 (RAII base class, `dynamic_cast`, libcurl official docs).

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack (techniques) | HIGH | All 8 fix techniques verified against official docs |
| Features (behaviors) | HIGH | Acceptance criteria from direct source inspection + RFC HTTP semantics |
| Architecture (locations) | HIGH | All target files verified by direct source inspection; shared-file conflicts exhaustively mapped |
| Pitfalls | HIGH | All derived from codebase-specific anti-patterns, not generic advice |

**Overall confidence:** HIGH

### Gaps to Address

- **`brls::View::cast<T>()` existence:** ARCHITECTURE.md flags: check `lib/borealis/library/include/borealis/core/view.hpp` before Phase 3. If absent, `dynamic_cast` + null guard is correct either way.
- **Second `copy_tree` location (`save_service_switch.cpp`):** File not found at expected path during research. Verify at Phase 2 implementation start. Does not block Phase 2 — worst case FIX-C2 removes only one copy.
- **`jti` auditing scope for logout:** Current design: `jti` blocklist is for access tokens only; refresh token is already DB-backed via `revokeRefreshToken`. Confirm scope before Phase 1 implementation.
- **Save blob migration rollback plan:** Use `cp -r` (not `mv`) so the original `uploads/saves/` path stays intact until reads from `SAVES_DIR` are verified. Document rollback steps in the Phase 1 plan.

---

## Sources

**Primary (HIGH confidence):** `@fastify/static` README, `@fastify/jwt` README, `@fastify/multipart` examples, `fastify.dev` logging reference, `curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html`, cppreference `std::atomic`, JPEG SOI spec + `sindresorhus/is-jpg`.

**Secondary (MEDIUM confidence):** Auth0 denylist blog, direct source inspection of all target files.

**Tertiary (LOW / needs hardware):** Borealis `brls::async`/`brls::sync` behavior under load — inferred from existing codebase patterns; no official Borealis docs consulted for the `runAsync` wrapper design.

---
*Research completed: 2026-06-04*
*Ready for roadmap: yes*
