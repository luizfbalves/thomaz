# Requirements: thomaz — Hardening Milestone

**Defined:** 2026-06-04
**Core Value:** Every issue documented in CONCERNS.md is resolved (or explicitly deferred with reason) without regressing existing behavior — verified by host tests and a clean desktop build.

## v1 Requirements

Each requirement is a fix from `.planning/codebase/CONCERNS.md`, with acceptance criteria derived from `.planning/research/FEATURES.md`. Intentional trade-offs (365-day JWT, TLS fail-safe) keep their behavior — safety nets only.

### Security

- [ ] **SEC-01**: Save blobs are not downloadable without authentication, and not by a non-owner — unauthenticated `GET /uploads/saves/...` no longer returns file content (404/403), cross-user access returns 403, owner access still returns 200, post images stay public (200). *(AC-1, HIGH)*
- [ ] **SEC-02**: A revoked (logged-out) access token can no longer authenticate — `POST /auth/logout` blocklists the bearer token via a `jti` claim + DB-backed `RevokedToken`; a request with that token then returns 401, while other valid tokens still return 200. Existing 365-day tokens keep working until logout. *(AC-5, HIGH)*
- [ ] **SEC-03**: When the CA bundle probe fails on Switch (`ca_ok == false`), a persistent on-screen warning is shown to the user; the fail-safe networking behavior is unchanged. *(AC-4)*

### Validation

- [ ] **VAL-01**: Image uploads are validated by magic bytes (JPEG SOI `0xFF 0xD8`), not the client-supplied `Content-Type` — spoofed content returns 400 before anything is written; valid JPEG ≤5 MB still returns 200; size/empty limits unchanged. *(AC-2)*
- [ ] **VAL-02**: Post `caption` is length-capped (`z.string().max(500)`) before the DB write — 501 chars returns 400; absent/empty/≤500 returns 200. *(AC-3)*

### Concurrency

- [ ] **CONC-01**: `cloudBusy` is `std::atomic<bool>` with a documented threading contract; the concurrent-operation guard behaves as before. *(AC-6A)*
- [ ] **CONC-02**: A shared `runAsync` wrapper on an activity base class auto-captures the `alive` guard; existing async call sites (`save_detail`, `mod_browser`, `theme_browser`) migrate to it so the use-after-free guard can't be forgotten. *(AC-6B)*
- [ ] **CONC-03**: In-flight curl requests are aborted on activity destruction via a shared `cancelled` flag checked in a `CURLOPT_XFERINFOFUNCTION` callback; happy-path requests are unaffected. *(AC-6C)*

### Tech Debt

- [ ] **DEBT-01**: `ensure_parent_dirs` exists in exactly one shared `source/platform/fs_util` helper; the four duplicated copies are removed (behavioral equivalence with the `theme_install.cpp` variant verified first). *(CONCERNS Tech Debt)*
- [ ] **DEBT-02**: `copy_tree` exists in exactly one shared platform utility; the duplicate copy is removed (second copy location confirmed first). *(CONCERNS Tech Debt)*
- [ ] **DEBT-03**: C-style view casts in the four flagged activities are replaced with null-guarded `dynamic_cast` / `brls::View::cast<T>()`; a mistyped/wrong-type view ID fails safely instead of crashing later. *(CONCERNS Tech Debt)*
- [ ] **DEBT-04**: API production logging is enabled via an `envToLogger` map — logger on for production/development, off for test; existing test suite stays clean and no secrets/PII are logged. *(AC-L)*

### Testing

- [ ] **TEST-01**: A regression test asserts the save-blob URL is not publicly accessible (guards SEC-01). *(AC-1.1 test, HIGH)*
- [ ] **TEST-02**: API tests cover the saves `PUT` revision path — `revision_required` (400), `revision_conflict` (409), new-slot create (200), matching-revision update (200). *(AC-T, HIGH)*
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
| Reduce JWT lifetime below 365 days | Breaks existing console sessions; console UX constraint (intentional trade-off) |
| Device auto-refresh flow | New feature, not a hardening fix |
| Change TLS fail-safe to a hard failure | Would brick the app on a packaging defect (intentional trade-off) |
| Mandatory per-fix hardware test gate | Hardware testing tracked as a separate manual checklist |
| Redis for token blocklist | New heavy infrastructure dependency; Postgres denylist is sufficient |
| New product features (cheats/mods/themes/saves/feed) | This is a hardening milestone, not a feature milestone |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SEC-01 | Phase 1 | Pending |
| SEC-02 | Phase 1 | Pending |
| VAL-01 | Phase 1 | Pending |
| VAL-02 | Phase 1 | Pending |
| DEBT-04 | Phase 1 | Pending |
| TEST-01 | Phase 1 | Pending |
| TEST-02 | Phase 1 | Pending |
| SEC-03 | Phase 2 | Pending |
| CONC-01 | Phase 2 | Pending |
| DEBT-01 | Phase 2 | Pending |
| DEBT-02 | Phase 2 | Pending |
| TEST-03 | Phase 2 | Pending |
| CONC-02 | Phase 3 | Pending |
| CONC-03 | Phase 3 | Pending |
| DEBT-03 | Phase 3 | Pending |
| TEST-04 | Phase 3 | Pending |

**Coverage:**
- v1 requirements: 16 total
- Mapped to phases: 16
- Unmapped: 0 ✓

---
*Requirements defined: 2026-06-04*
*Last updated: 2026-06-04 — traceability populated after roadmap creation*
