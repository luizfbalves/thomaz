# thomaz — Switch-Only Simplification

## What This Is

thomaz is a Nintendo Switch homebrew hub (Borealis UI, devkitPro NRO) that manages cheats, mods, themes, cloud saves, and sysmodules, backed by a Node.js/Fastify + PostgreSQL cloud API. The tree historically built for two targets — Switch (devkitPro) and desktop PC (SDL2/GLFW) — from one source. This milestone is a **simplification pass**: remove the desktop build target entirely so the tree targets only the Switch, cutting the platform-abstraction surface that existed solely to run the GUI on PC.

## Current State

**Shipped v1.1 Switch-Only Simplification (2026-06-06).** The desktop (PC/SDL2/GLFW) build target is fully removed: `CMakeLists.txt` has no `PLATFORM_DESKTOP` path, the desktop build/run scripts are gone, the five desktop stub pairs are deleted (their `*_switch` twins are now sole implementations), and every implementation-selection seam unconditionally uses the Switch path. Platform-portability `#else` seams (path strings, `_WIN32`/`localtime_r`) were knowingly retained to keep the host doctest suite green. Proven by **zero change to the shipped `.nro`** — host doctest 209/209 + a clean Switch build (`build_switch/thomaz.nro`, 7.7 MB), redeployed and confirmed on real hardware.

**No active milestone.** Next milestone to be defined via `/gsd-new-milestone`. Carried-forward candidates below.

## Core Value

The desktop build target and its supporting stubs are removed with **zero change to the shipped Switch `.nro`** — proven by a green host doctest suite (`tests/Makefile`) and a clean Switch build (`scripts/build-switch.sh`).

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

**Shipped v1.1 (Switch-Only Simplification):**

- ✓ Five desktop platform stub pairs deleted; `*_switch` impls are sole implementations (SIMPL-01) — v1.1
- ✓ Every `*_fake`-vs-`*_switch` implementation-selection seam collapsed to the Switch path; platform-portability `#else` seams retained by design (Option D) (SIMPL-02) — v1.1
- ✓ No `source/` file references a deleted stub or desktop-only symbol/include (SIMPL-03) — v1.1
- ✓ `CMakeLists.txt` has no `PLATFORM_DESKTOP` path; dual-target comments gone (BUILD-01) — v1.1
- ✓ Desktop helper scripts `build-desktop.sh` / `run-desktop.sh` removed (BUILD-02) — v1.1
- ✓ Clean Switch build via `scripts/build-switch.sh` produces `build_switch/thomaz.nro` (BUILD-03) — v1.1
- ✓ README/docs describe a Switch-only tree with the two-gate verification flow (DOC-01) — v1.1
- ✓ Host doctest 209/209 + clean Switch build confirmed green together as the milestone gate (VERIF-01) — v1.1

### Active

_No active milestone — define the next via `/gsd-new-milestone`._

**Carried-forward candidates (future milestones):**
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

**v1.1 scope note (2026-06-05):** Removing the desktop target also removes PC GUI iteration and the cheap non-devkitPro full-tree compile check. The host doctest suite (`tests/Makefile`) is independent of the desktop build and survives untouched. `saves/fake_cloud_save_client.*` is a doctest test double — NOT a desktop GUI stub — and is kept.

## Context

- The codebase was mapped on 2026-06-04; all issues for this milestone come from `.planning/codebase/CONCERNS.md` (Tech Debt, Security, Performance, Fragile Areas, Test Coverage Gaps).
- Architecture is clean-architecture: pure `core/` (host-testable via doctest), `platform/` with `*_switch.cpp`/`*_fake.cpp` pairs, Borealis `*Activity` UI. The API is Fastify + Prisma + Postgres, tested with Vitest.
- Two HIGH-severity security items center on the same root cause: save blobs served statically from `UPLOAD_DIR` without auth.
- Several fragile areas are concurrency patterns (`alive` guard, `cloudBusy`, async pool) that are correct today but easy to break in future edits.
- API runs in production at `api.thomaz.baseup.cc` (Lightsail, PM2, auto-deploy on push to `main` touching `api/**`) — security fixes ship to a live service.

**Post-v1.0 state (2026-06-05):** Community feature fully excised from both API and client. API security hardened (auth-gated save blobs, token revocation, production logging) with a green Vitest regression suite. C++ client refactored onto a `ThomazActivity`/`runAsync` base that makes the `alive` guard unforgettable, with curl cancellation on teardown and a shared `fs_util`; host doctest suite passes (175+ tests). All gates are host-only — a physical-Switch UAT pass for the activity-pop crash paths and TLS banner render remains outstanding (deferred, non-gating).

## Constraints

- **Verification**: Host doctest suite (`tests/Makefile`, `g++`) is the primary automated gate and must stay green. The compile gate for the UI/platform layer is now the **Switch build** (`scripts/build-switch.sh`, devkitPro Docker) — the desktop build is removed this milestone and is no longer available as a non-devkitPro compile check. Hardware validation is a separate manual checklist, not a per-item gate.
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
| v1.1: sequence removal source→build→docs so the tree is buildable at every phase boundary | Collapsing seams before CMake stops offering the desktop branch avoids an unbuildable mid-phase tree | ✓ Good — no phase left the tree broken |
| v1.1: retain platform-portability `#else` seams (Option D, path strings + `_WIN32`/`localtime_r`), only collapse `*_fake`-vs-`*_switch` stub-selection | The host doctest suite compiles these branches and writes to host-relative paths; removing them breaks host tests writing to absolute Switch paths | ✓ Good — doctest stayed green (209/209) |
| v1.1: keep `saves/fake_cloud_save_client.*` (a doctest test double, not a desktop GUI stub) | The host suite links it; it is not part of the desktop removal surface | ✓ Good — suite unregressed |
| v1.1: two single-target gates (host doctest + Switch build) replace the old non-devkitPro full-tree compile check | Removing the desktop target also removes the cheap PC compile gate; the Switch build becomes the compile gate | ✓ Good — both gates green, hardware-confirmed |

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
*Last updated: 2026-06-06 — shipped v1.1 Switch-Only Simplification milestone*
