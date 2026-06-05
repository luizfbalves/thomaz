# Requirements: thomaz — Hardening Milestone

**Defined:** 2026-06-04
**Core Value:** Every issue documented in CONCERNS.md is resolved (or explicitly deferred with reason) without regressing existing behavior — verified by host tests and a clean desktop build.

## v1 Requirements

Each requirement is a fix from `.planning/codebase/CONCERNS.md` (acceptance criteria in `.planning/research/FEATURES.md`), plus the community-feature removal decided during Phase 1 discussion. Intentional trade-offs (365-day JWT, TLS fail-safe) keep their behavior — safety nets only.

### Community Feature Removal

The posts/feed/comments/likes community feature is being removed entirely. Its image-upload path (`@fastify/static` + `@fastify/multipart`) is the root cause of the SEC-01 save-blob exposure, so removal resolves it at the source. Auth/session infrastructure that happens to live under `feed/` directories (`core/feed/session_codec`, `platform/feed/auth_store`) is shared with cloud saves and MUST be preserved.

- [x] **RM-01**: Community API endpoints are removed — `routes/posts.ts`, `routes/feed.ts`, the community parts of `routes/users.ts` (`/users/:username`, feed pages), and the `@fastify/multipart` plugin. `auth.ts`, `saves.ts`, and account-only endpoints remain. *(scope decision 2026-06-04)*
- [x] **RM-02**: `@fastify/static` serving is removed and the `Post`/`Like`/`Comment` Prisma models are dropped (with migration); no save blob resides in any publicly served path. `User`/`RefreshToken`/`SaveSlot` remain. *(resolves SEC-01 root cause)*
- [x] **RM-03**: Client community-feed code is removed — `core/feed/feed_json`, `core/feed/feed_types`, `platform/feed/http_feed_client`, `fake_feed_client`, `feed_client.hpp` (IFeedClient) — while `core/feed/session_codec` and `platform/feed/auth_store` (auth/session, used by cloud saves) are preserved. *(scope decision 2026-06-04)*
- [x] **RM-04**: After removal, the API test suite and a clean desktop build (`-DUSE_SDL2=ON`) pass — auth, cloud saves, and the saves revision logic are intact (no dead references to removed feed/post code). *(regression guard)*

### Security

- [x] **SEC-01**: Save blobs are not downloadable without authentication and not by a non-owner — achieved by removing static file serving (RM-02); cross-user access via the API returns 403, owner access returns 200. Verified by TEST-01. *(AC-1, HIGH — fixed in removal phase, verified in security phase)*
- [x] **SEC-02**: A revoked (logged-out) access token can no longer authenticate — `POST /auth/logout` blocklists the bearer token via a `jti` claim + DB-backed `RevokedToken`; that token then returns 401, while other valid tokens still return 200. Logout revokes only the current token; pre-deploy tokens without `jti` keep working until relogin. *(AC-5, HIGH)*
- [ ] **SEC-03**: When the CA bundle probe fails on Switch (`ca_ok == false`), a persistent on-screen warning is shown; the fail-safe networking behavior is unchanged. *(AC-4)*

### Concurrency

- [ ] **CONC-01**: `cloudBusy` is `std::atomic<bool>` with a documented threading contract; the concurrent-operation guard behaves as before. *(AC-6A)*
- [ ] **CONC-02**: A shared `runAsync` wrapper on an activity base class auto-captures the `alive` guard; existing async call sites (`save_detail`, `mod_browser`, `theme_browser`) migrate to it so the use-after-free guard can't be forgotten. *(AC-6B)*
- [ ] **CONC-03**: In-flight curl requests are aborted on activity destruction via a shared `cancelled` flag checked in a `CURLOPT_XFERINFOFUNCTION` callback; happy-path requests are unaffected. *(AC-6C)*

### Tech Debt

- [x] **DEBT-01**: `ensure_parent_dirs` exists in exactly one shared `source/platform/fs_util` helper; the duplicated copies are removed (behavioral equivalence with the `theme_install.cpp` variant verified first). *(CONCERNS Tech Debt)*
- [x] **DEBT-02**: `copy_tree` exists in exactly one shared platform utility; the duplicate copy is removed (second copy location confirmed first). *(CONCERNS Tech Debt)*
- [ ] **DEBT-03**: C-style view casts in the flagged activities are replaced with null-guarded `dynamic_cast` / `brls::View::cast<T>()`; a mistyped/wrong-type view ID fails safely instead of crashing later. *(CONCERNS Tech Debt)*
- [x] **DEBT-04**: API production logging is enabled via an `envToLogger` map — logger on for production/development, off for test; existing test suite stays clean and no secrets/PII (Authorization headers, emails) are logged. *(AC-L)*

### Testing

- [x] **TEST-01**: A regression test asserts no save-blob URL is publicly accessible (guards SEC-01 / RM-02 — no static route exposes saves). *(AC-1.1 test, HIGH)*
- [x] **TEST-02**: API tests cover the saves `PUT` revision path — `revision_required` (400), `revision_conflict` (409), new-slot create (200), matching-revision update (200). *(AC-T, HIGH)*
- [ ] **TEST-03**: A host test covers the TLS fail-safe branch (`ca_ok == false`) so a regression that silently disables verification fails CI. *(AC-4 test)*
- [ ] **TEST-04**: The cloud-save upload conflict-resolution path is covered (host doctest for the conflict/`plan_push` branch and/or the `runAsync`-dropped-callback semantics). *(CONCERNS Test Gaps)*

## v2 Requirements

Deferred — documented in CONCERNS.md but lower priority than security/crash items.

### Performance

- **PERF-01**: Avoid traversing the archive twice per mod extraction (`libarchive_extractor.cpp`) — indeterminate progress or cached entry list.
- **PERF-02**: Cache last-known `CloudStatus` to skip the status prefetch on cloud-save upload when recent.

## Out of Scope

Explicitly excluded — see PROJECT.md and FEATURES.md anti-features.

| Feature | Reason |
|---------|--------|
| VAL-01: image-upload magic-byte validation | Target was `posts.ts`; the posts feature is being removed (RM-01) — vulnerability removed with the code |
| VAL-02: post caption length cap | Same — `posts.ts` removed (RM-01); no caption field remains |
| Reduce JWT lifetime below 365 days | Breaks existing console sessions; console UX constraint (intentional trade-off) |
| Device auto-refresh flow | New feature, not a hardening fix |
| Change TLS fail-safe to a hard failure | Would brick the app on a packaging defect (intentional trade-off) |
| Mandatory per-fix hardware test gate | Hardware testing tracked as a separate manual checklist |
| Redis for token blocklist | New heavy infrastructure dependency; Postgres denylist is sufficient |
| Re-adding any community feature (posts/feed) | Deliberately removed this milestone (RM-01..RM-03) |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| RM-01 | Phase 1 | Complete |
| RM-02 | Phase 1 | Complete |
| RM-03 | Phase 1 | Complete |
| RM-04 | Phase 1 | Complete |
| SEC-01 | Phase 2 | Complete |
| SEC-02 | Phase 2 | Complete |
| DEBT-04 | Phase 2 | Complete |
| TEST-01 | Phase 2 | Complete |
| TEST-02 | Phase 2 | Complete |
| SEC-03 | Phase 3 | Pending |
| CONC-01 | Phase 3 | Pending |
| DEBT-01 | Phase 3 | Complete |
| DEBT-02 | Phase 3 | Complete |
| TEST-03 | Phase 3 | Pending |
| CONC-02 | Phase 4 | Pending |
| CONC-03 | Phase 4 | Pending |
| DEBT-03 | Phase 4 | Pending |
| TEST-04 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 18 total
- Mapped to phases: 18 (Phase 1: 4, Phase 2: 5, Phase 3: 5, Phase 4: 4)
- Unmapped: 0 ✓

---
*Requirements defined: 2026-06-04*
*Last updated: 2026-06-04 — added community-feature removal (RM-01..RM-04); VAL-01/VAL-02 moved to Out of Scope; traceability updated for 4-phase roadmap*
