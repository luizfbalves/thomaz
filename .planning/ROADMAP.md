# Roadmap: thomaz — Hardening Milestone

## Overview

This milestone resolves every issue surfaced by the codebase audit without adding new features. Three phases deliver the fixes in risk order: API security (live service, highest impact) first, then isolated C++ platform fixes, then the larger C++ activity refactor. Every fix is verifiable by host tests (Vitest) or a clean desktop build (`-DUSE_SDL2=ON`); on-hardware validation is a separate manual checklist.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (X.Y): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: API Security + Regression Tests** - Harden the live API against the two HIGH-severity security issues and co-ship regression tests
- [ ] **Phase 2: C++ Platform Hardening** - Fix isolated C++ platform-layer issues (fs_util extraction, TLS warning, cloudBusy atomicity) and their host tests
- [ ] **Phase 3: C++ Activity Hardening** - Refactor all activities to the runAsync base-class pattern, replace unsafe casts, add curl cancellation, and cover the conflict path

## Phase Details

### Phase 1: API Security + Regression Tests
**Goal**: The live API no longer exposes save blobs publicly, tokens can be revoked on logout, uploads are validated, production logging is enabled, and regression tests guard these fixes
**Depends on**: Nothing (first phase)
**Requirements**: SEC-01, SEC-02, VAL-01, VAL-02, DEBT-04, TEST-01, TEST-02
**Success Criteria** (what must be TRUE):
  1. `GET /uploads/saves/<userId>/<titleId>.bin` returns 404 (blob no longer in static-served directory); owner access via the API route returns 200; cross-user access returns 403
  2. A token used after `POST /auth/logout` returns 401; other valid tokens for the same user still return 200; pre-deploy tokens without a `jti` claim are unaffected
  3. A post upload with a non-JPEG magic-byte payload (valid JPEG MIME header, wrong bytes) returns 400; a caption exceeding 500 chars returns 400; valid requests still succeed
  4. API running in `production` environment emits pino JSON request logs; `test` environment stays silent (existing Vitest suite passes with no spurious output)
  5. Vitest suite passes including: blob-URL-not-reachable test (TEST-01), saves PUT revision-required (400) and revision-conflict (409) tests (TEST-02)
**Plans**: TBD

**Planning flags:**
- **jti scope (SEC-02):** Confirm before Phase 1 implementation — `jti` blocklist covers access tokens only; refresh tokens are already DB-backed via `revokeRefreshToken`. Existing 365-day tokens without `jti` must skip the blocklist check (not be rejected).
- **Save blob migration rollback (SEC-01):** Use `cp -r` (not `mv`) during the Lightsail migration so `uploads/saves/` stays intact until reads from `SAVES_DIR` are verified. Document rollback steps in the Phase 1 plan.
- **S3/S4 serialize constraints:** FIX-A4 (caption cap) and FIX-A5 (magic bytes) share `routes/posts.ts` — one PR. FIX-A1 (`ensureSavesDir` call) and FIX-A6 (logger) share `app.ts` different lines — safe in any order. TEST-01 (blob URL test) depends on FIX-A1 being deployed first.

### Phase 2: C++ Platform Hardening
**Goal**: Duplicated filesystem helpers are consolidated into a shared `fs_util` platform utility, the TLS fail-safe shows a visible on-screen warning, and `cloudBusy` is `std::atomic<bool>` — all verified by a clean desktop build
**Depends on**: Phase 1
**Requirements**: SEC-03, CONC-01, DEBT-01, DEBT-02, TEST-03
**Success Criteria** (what must be TRUE):
  1. `source/platform/fs_util.hpp` and `fs_util.cpp` exist; `ensure_parent_dirs` and `copy_tree` are defined there; the four previously duplicated `ensure_parent_dirs` copies and both `copy_tree` copies are removed from their original call sites
  2. `save_detail_activity.hpp` declares `cloudBusy` as `std::atomic<bool>{false}`; all read/write sites use `load()`/`store()` or `compare_exchange_strong`
  3. When the CA bundle probe fails (`ca_ok == false`), a visible warning is emitted via `brls::Logger::warning` (or `brls::Application::notify` from the first curl call site); no `CURLOPT_SSL_VERIFYPEER` lines appear outside `#ifdef __SWITCH__`
  4. A doctest covering the TLS fail-safe branch (`ca_ok == false`) passes in the host test suite (TEST-03)
  5. Desktop build with `-DUSE_SDL2=ON` compiles clean with zero errors and zero new warnings
**Plans**: TBD

**Planning flags:**
- **Second `copy_tree` location (DEBT-02):** Research could not locate `save_service_switch.cpp` at the expected path. Verify at Phase 2 implementation start. If absent, DEBT-02 removes only the `mod_store.cpp` copy — still satisfies the requirement.
- **`ensure_parent_dirs` edge case (DEBT-01):** `theme_install.cpp` uses a character-by-character loop variant. Write a doctest for a representative theme path (e.g., `romfs:/themes/a/b/c`) before removing the local copy, confirming the canonical substring-at-slash form is equivalent.
- **S1 serialize constraint:** FIX-C1 and FIX-C2 share `fs_util.hpp`/`.cpp` — do them in a single commit.
- **S2 constraint (cross-phase):** CONC-01 (atomicize `cloudBusy` in `save_detail_activity.hpp`) must land before Phase 3's CONC-02, which removes the `alive` member from the same header.

### Phase 3: C++ Activity Hardening
**Goal**: All activities inherit the `ThomazActivity` base class with its `runAsync` wrapper (making the `alive` guard impossible to forget), unsafe C-style view casts are null-guarded, in-flight curl requests cancel on activity destruction, and the conflict-resolution path is covered by a host test
**Depends on**: Phase 2
**Requirements**: CONC-02, CONC-03, DEBT-03, TEST-04
**Success Criteria** (what must be TRUE):
  1. `source/app/thomaz_activity.hpp` defines a `ThomazActivity` base class with a protected `runAsync(worker, onSync)` method that auto-captures `alive` by value before dispatch; all activities that previously used `brls::async` directly now call `this->runAsync(...)` instead
  2. The four flagged activities (`game_list`, `save_manager`, `save_detail`, `mod_browser`) use null-guarded `dynamic_cast` (or `brls::View::cast<T>()` if available) in place of every C-style `(T*)this->getView(...)` cast; a null result is handled safely (log + return)
  3. In-flight curl requests abort when their activity is destroyed — the shared `cancelled` flag is set in the destructor, and the `CURLOPT_XFERINFOFUNCTION` callback returns 1 when it fires; happy-path requests complete normally
  4. A host doctest covers the cloud-save conflict-resolution / `plan_push` branch (TEST-04), and the `runAsync`-dropped-callback semantics under simulated nav-away are exercised where feasible without hardware
  5. Desktop build with `-DUSE_SDL2=ON` compiles clean with zero errors and zero new warnings after all activity inheritance changes
**Plans**: TBD

**Planning flags:**
- **`brls::View::cast<T>()` existence (DEBT-03):** Check `lib/borealis/library/include/borealis/core/view.hpp` before Phase 3 implementation. If the method is absent, `dynamic_cast` + null guard is the correct replacement either way.
- **S2 constraint:** CONC-02 removes the `alive` member from `save_detail_activity.hpp` — this requires Phase 2's CONC-01 to have already atomicized `cloudBusy` in that same header.
- **CONC-03 after CONC-02:** The curl cancellation flag (CONC-03) relies on the ownership model established by the `runAsync` wrapper (CONC-02). Do CONC-02 first.
- **FIX-C3 / FIX-C4 shared files:** DEBT-03 (casts) and CONC-02 (runAsync) both touch the four activity `.cpp` files at different lines. Serialize in one PR or coordinate carefully to avoid merge conflicts.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. API Security + Regression Tests | 0/? | Not started | - |
| 2. C++ Platform Hardening | 0/? | Not started | - |
| 3. C++ Activity Hardening | 0/? | Not started | - |
