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
- ✓ Community feed + posts with image upload (`core/feed`, `platform/feed`, `api/routes/{feed,posts}`) — existing
- ✓ JWT auth with refresh-token rotation, rate limiting (`api/plugins/auth`, `api/lib/refresh-tokens`) — existing
- ✓ Clean-architecture split: pure `core/` covered by host doctest suite, `platform/` switch/fake pairs — existing

### Active

<!-- This milestone: fix everything mapped in CONCERNS.md, across four fronts. -->

**Security**
- [ ] Save blobs no longer publicly downloadable — require auth / move out of static root (HIGH)
- [ ] Post `caption` length-capped via schema before DB write
- [ ] Image uploads validated by magic bytes, not trusted `Content-Type`
- [ ] Visible on-screen warning when TLS verification is unavailable (CA bundle probe fails) — behavior unchanged, safety net only
- [ ] Token revocation / blocklist on logout/compromise — JWT lifetime unchanged, safety net only

**Concurrency / crashes**
- [ ] `cloudBusy` threading contract made safe/explicit (document invariant or `std::atomic<bool>`)
- [ ] `alive` lifetime-guard pattern made hard to omit (shared `runAsync` wrapper on activity base)
- [ ] `brls::async` in-flight request cancellation on activity destruction (pool-exhaustion guard)

**Tech debt / duplication**
- [ ] `ensure_parent_dirs` extracted to one shared `platform/fs_util` helper (4 copies removed)
- [ ] `copy_tree` factored into a single shared platform utility (2 copies removed)
- [ ] C-style view casts replaced with `brls::View::cast<T>()` / null-guarded `dynamic_cast`
- [ ] Production logging enabled in the API (`logger` not unconditionally false)

**Test coverage**
- [ ] API test for saves `PUT` revision-required / revision-conflict branches
- [ ] Test asserting save-blob URL is not publicly accessible (security regression guard)
- [ ] Test for the TLS fail-safe branch (`ca_ok == false`)
- [ ] Coverage for the cloud-save upload conflict-resolution path

### Out of Scope

- New features (cheats/mods/themes/saves/feed functionality) — this is a hardening milestone, not a feature milestone
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

## Constraints

- **Verification**: Each fix needs a host test (doctest for core, Vitest for API) where feasible, plus a clean desktop build (`-DUSE_SDL2=ON`). Hardware validation is a separate manual checklist, not a per-item gate.
- **Behavior preservation**: Intentional trade-offs (TLS fail-safe, 365-day JWT) keep their behavior; only safety nets are added.
- **Architecture**: New shared logic goes in `core/` (testable); `platform/` stays thin orchestration. No exceptions in core/platform — return-value error handling.
- **Tech stack**: C++20 client (devkitA64 / desktop SDL2); Node ≥20 + TypeScript strict API. No new heavy dependencies just to fix issues.
- **Live API**: API security fixes must not break existing clients (365-day tokens already in the wild).

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Treat CONCERNS.md as the backlog; no new hunting | Audit already surfaced concrete, prioritized issues | — Pending |
| Fix all four fronts (security, concurrency, debt, tests) | User wants a thorough hardening pass | — Pending |
| Intentional trade-offs get safety nets, not behavior changes | Preserve console UX; avoid regressions on a live service | — Pending |
| Verify via host tests + clean desktop build; hardware separate | On-hardware testing is manual; can't gate every fix on it | — Pending |
| Performance items deferred | Lower risk/impact than security & crash items | — Pending |

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
*Last updated: 2026-06-04 after initialization*
