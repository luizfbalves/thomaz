# thomaz — Hardening Milestone

## What This Is

thomaz is a Nintendo Switch homebrew hub (Borealis UI, devkitPro NRO + desktop build) that manages cheats, mods, themes, cloud saves, and sysmodules, backed by a Node.js/Fastify + PostgreSQL cloud API. This milestone is a **quality/hardening pass**: take the issues already surfaced by the codebase audit (`.planning/codebase/CONCERNS.md`) as a backlog and fix them — no new features.

## Core Value

Every issue documented in `CONCERNS.md` is resolved (or explicitly deferred with reason) without regressing existing behavior — verified by host tests and a clean desktop build.

## Requirements

### Validated

<!-- Existing, working capabilities inferred from the codebase map. -->

- ✓ Cheat fetch + apply via switch-cheats-db (`core/cheat_repository`, `platform/cheat_store`) — existing
- ✓ Mod browse/install from GameBanana with archive extraction (`core/mods`, `platform/mods`) — existing
- ✓ Theme browse/apply via Themezer + exelix engine (`core/themes`, `platform/themes`) — existing
- ✓ Cloud save upload/download/sync with optimistic locking (`core/saves`, `platform/saves`, `api/routes/saves`) — existing
- ✓ Sysmodule scan/manage (`core/sysmod`, `platform/sysmod`) — existing
- ✓ JWT auth with refresh-token rotation, rate limiting (`api/plugins/auth`, `api/lib/refresh-tokens`) — existing
- ✓ Clean-architecture split: pure `core/` covered by host doctest suite, `platform/` switch/fake pairs — existing

**Shipped v1.0 (Hardening):**

- ✓ Community feature removed entirely — posts/feed/comments/likes routes, `@fastify/static`+`multipart`, `Post`/`Like`/`Comment` models, client feed code; shared `session_codec`/`auth_store` preserved (RM-01..RM-04) — v1.0
- ✓ Save blobs require auth and owner identity — static serving removed; cross-user 403, owner 200, direct path 404, regression-tested (SEC-01, TEST-01) — v1.0
- ✓ Token revocation on logout — `jti` claim + Postgres `RevokedToken` blocklist; pre-deploy tokens pass unblocked; DB outage fails open (SEC-02) — v1.0
- ✓ TLS fail-closed when CA probe fails + latent on-screen warning banner wired in 14 activities / 5 locales (SEC-03, revised to fail-closed per CONTEXT D-06a) — v1.0
- ✓ `cloudBusy` is `std::atomic<bool>` with documented contract (CONC-01) — v1.0
- ✓ `ThomazActivity` base with `runAsync` auto-capturing the `alive` guard; all 13 activities migrated, `brls::async` count = 0 (CONC-02) — v1.0
- ✓ In-flight curl requests abort on activity destruction across both transports (`mod_download` + `http_client_curl`) (CONC-03) — v1.0
- ✓ `ensure_parent_dirs` + `copy_tree` consolidated into one `platform/fs_util` helper (7 call-sites) (DEBT-01, DEBT-02) — v1.0
- ✓ C-style view casts replaced with null-guarded `dynamic_cast` in flagged activities (DEBT-03) — v1.0
- ✓ API production logging via `envToLogger` map with header/PII redaction (DEBT-04) — v1.0
- ✓ Regression/host tests added: revision branches, save-blob 404, TLS fail-safe, conflict-resolution/`plan_push` (TEST-02, TEST-01, TEST-03, TEST-04) — v1.0

### Active

<!-- v1.0 Hardening shipped. Next milestone TBD via /gsd-new-milestone. -->

_None — v1.0 Hardening complete. Candidates for the next milestone:_
- [ ] PERF-01: avoid double archive traversal per mod extraction (deferred from v1.0)
- [ ] PERF-02: cache last-known `CloudStatus` to skip the upload status prefetch (deferred from v1.0)
- [ ] On-hardware UAT pass: 5 Phase-04 activity-pop/dialog UAF crash-path scenarios + Phase-03 TLS banner render + `save_service_switch.cpp` Switch-toolchain compile (deferred from v1.0 close — see STATE.md)

### Out of Scope

- Re-adding the community feature (posts/feed/comments/likes) — deliberately removed this milestone
- Image-upload magic-byte validation and post caption length cap — target code (`posts.ts`) is being removed, so the vulnerabilities go with it
- New features (cheats/mods/themes/saves functionality) — this is a hardening milestone, not a feature milestone
- Changing the intentional TLS fail-safe behavior — keep fail-safe; only add a visible warning (per user decision)
- Reducing the 365-day JWT lifetime / adding device auto-refresh — keep console-UX trade-off; only add revocation (per user decision)
- Mandatory on-hardware verification gate — hardware testing tracked as a separate manual checklist, not a per-fix blocker (per user decision)
- Performance bottlenecks (double archive traversal, cloud-save status prefetch) — documented in CONCERNS.md but lower priority; revisit only if time remains

## Context

- The codebase was mapped on 2026-06-04; all issues for this milestone come from `.planning/codebase/CONCERNS.md` (Tech Debt, Security, Performance, Fragile Areas, Test Coverage Gaps).
- Architecture is clean-architecture: pure `core/` (host-testable via doctest), `platform/` with `*_switch.cpp`/`*_fake.cpp` pairs, Borealis `*Activity` UI. The API is Fastify + Prisma + Postgres, tested with Vitest.
- Two HIGH-severity security items center on the same root cause: save blobs served statically from `UPLOAD_DIR` without auth.
- Several fragile areas are concurrency patterns (`alive` guard, `cloudBusy`, async pool) that are correct today but easy to break in future edits.
- API runs in production at `api.thomaz.baseup.cc` (Lightsail, PM2, auto-deploy on push to `main` touching `api/**`) — security fixes ship to a live service.

**Post-v1.0 state (2026-06-05):** Community feature fully excised from both API and client. API security hardened (auth-gated save blobs, token revocation, production logging) with a green Vitest regression suite. C++ client refactored onto a `ThomazActivity`/`runAsync` base that makes the `alive` guard unforgettable, with curl cancellation on teardown and a shared `fs_util`; host doctest suite passes (175+ tests). All gates are host-only — a physical-Switch UAT pass for the activity-pop crash paths and TLS banner render remains outstanding (deferred, non-gating).

## Constraints

- **Verification**: Each fix needs a host test (doctest for core, Vitest for API) where feasible, plus a clean desktop build (`-DUSE_SDL2=ON`). Hardware validation is a separate manual checklist, not a per-item gate.
- **Behavior preservation**: Intentional trade-offs (TLS fail-safe, 365-day JWT) keep their behavior; only safety nets are added.
- **Architecture**: New shared logic goes in `core/` (testable); `platform/` stays thin orchestration. No exceptions in core/platform — return-value error handling.
- **Tech stack**: C++20 client (devkitA64 / desktop SDL2); Node ≥20 + TypeScript strict API. No new heavy dependencies just to fix issues.
- **Live API**: API security fixes must not break existing clients (365-day tokens already in the wild).

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Treat CONCERNS.md as the backlog; no new hunting | Audit already surfaced concrete, prioritized issues | ✓ Good — 18/18 reqs shipped from the backlog |
| Remove the entire community feature (posts/feed) as the first phase | No longer wanted; its `@fastify/static`/`multipart` path is the root cause of the save-blob exposure (SEC-01) | ✓ Good — removal closed SEC-01 at the source (RM-01..04) |
| Logout revokes only the current token (not all sessions); tokens without `jti` pass until relogin | Simplest safety net; no break for existing 365-day console tokens | ✓ Good — SEC-02 shipped; pre-deploy tokens unaffected |
| Keep `session_codec` + `auth_store` (under `feed/` dirs) — they are auth, not community | Cloud saves depend on them | ✓ Good — preserved; cloud saves intact |
| Fix all four fronts (security, concurrency, debt, tests) | User wants a thorough hardening pass | ✓ Good — all four delivered |
| Intentional trade-offs get safety nets, not behavior changes | Preserve console UX; avoid regressions on a live service | ✓ Good — 365-day JWT lifetime preserved |
| Verify via host tests + clean desktop build; hardware separate | On-hardware testing is manual; can't gate every fix on it | ⚠️ Revisit — 3 hardware UAT items deferred at close; needs a Switch pass |
| Performance items deferred | Lower risk/impact than security & crash items | — Pending — PERF-01/02 carried to next milestone |
| TLS revised from fail-open+banner to **fail-closed** during Phase 3 (CONTEXT D-06a) | Refusing unauthenticated downloads is safer than warning-and-proceeding; banner kept latent | ✓ Good — TEST-03 asserts both branches |
| `runAsync` base-class wrapper (CONC-02) over per-site `alive` captures | Makes the use-after-free guard impossible to forget; `brls::async` count driven to 0 | ✓ Good — 13 activities migrated |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-05 after v1.0 Hardening milestone*
