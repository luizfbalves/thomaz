# Roadmap: thomaz

## Milestones

- ✅ **v1.0 Hardening** — Phases 1-4 (shipped 2026-06-05)
- 🟡 **v0.5.0 Theme Extraction** — Phases 1-2 shipped 2026-06-05; Phases 3-4 open
- 🟢 **v1.1 Switch-Only Simplification** — Phases 5-7 (active, started 2026-06-05)

## Phases

<details>
<summary>✅ v1.0 Hardening (Phases 1-4) — SHIPPED 2026-06-05</summary>

Resolved every issue surfaced by the codebase audit (`CONCERNS.md`) without adding features — community-feature removal, live-API security hardening with regression tests, isolated C++ platform fixes, and the C++ activity refactor. Verified by host tests (Vitest + doctest) and a clean desktop build (`-DUSE_SDL2=ON`). Full detail: [milestones/v1.0-ROADMAP.md](milestones/v1.0-ROADMAP.md).

- [x] Phase 1: Remove Community Feature (3/3 plans) — completed 2026-06-04
- [x] Phase 2: API Security + Regression Tests (3/3 plans) — completed 2026-06-05
- [x] Phase 3: C++ Platform Hardening (4/4 plans) — completed 2026-06-05
- [x] Phase 4: C++ Activity Hardening (6/6 plans) — completed 2026-06-05

**Deferred at close (hardware-only, non-gating):**

- Phase 03 TLS-insecure banner visual render (requires forced latch on real hardware)
- Phase 03 `save_service_switch.cpp` Switch-toolchain compile check (devkitPro only)
- Phase 04 5 UAT crash-path scenarios — activity-pop / dialog-button UAF probes (require a physical Switch)

</details>

<details>
<summary>🟡 v0.5.0 Theme Extraction (Phases 1-2) — SHIPPED 2026-06-05 (partial)</summary>

On-device extraction of the firmware home-menu base `.szs` layouts into `/themes/systemData/`, keyless to the user (ported exelix BIS+SPL+hactool, GPLv2). Released as app version 0.5.0. Verified by host doctests and a clean desktop build; on-hardware run deferred. Full detail: [milestones/v0.5.0-ROADMAP.md](milestones/v0.5.0-ROADMAP.md).

- [x] Phase 1: Privileged Extraction Spike (5/5 plans) — completed 2026-06-05
- [x] Phase 2: Full Extraction Engine (4/4 plans) — completed 2026-06-05

**Carried forward (planned, not built):**

- [ ] Phase 3: Theme UI Integration (0/3 plans) — INTEG-01..05
- [ ] Phase 4: Forwarder, optional (0/1 plans) — TAKEOVER-03

</details>

### 🟢 v1.1 Switch-Only Simplification (Phases 5-7) — ACTIVE

Remove the desktop (PC/SDL2/GLFW) build target entirely so the source tree targets only Nintendo Switch, cutting the platform-abstraction surface that existed solely to run the GUI on PC — with **zero change to the shipped `.nro`**. Sequenced for safety: collapse the source selection seams first (so `*_switch` impls are sole before the build stops offering a desktop branch), then strip the build system, then clean docs and run the final combined gate. Verified by a green host doctest suite (`tests/Makefile`) and a clean Switch build (`scripts/build-switch.sh`).

- [ ] **Phase 5: Collapse Source Seams to Switch-Only** — Delete the 5 desktop stub pairs and collapse every `__SWITCH__` selection seam so the `*_switch` impls are the sole implementations
- [x] **Phase 6: Strip Desktop from Build System** — Remove the `PLATFORM_DESKTOP` path from CMake, delete the desktop helper scripts, and prove the Switch build still produces `thomaz.nro` (completed 2026-06-06)
- [x] **Phase 7: Docs Cleanup & Final Verification Gate** — Update comments/docs to describe a Switch-only tree and re-confirm both verification gates (host doctest + Switch build) are green (completed 2026-06-06)

## Phase Details

### Phase 5: Collapse Source Seams to Switch-Only

**Goal**: The `source/` tree compiles and reasons as a single Switch target — every desktop platform stub is gone and every implementation-selection seam unconditionally uses the Switch path, with the host doctest suite still green.
**Depends on**: Phase 4 (v1.0 — last shipped phase; this milestone continues numbering)
**Requirements**: SIMPL-01, SIMPL-02, SIMPL-03
**Success Criteria** (what must be TRUE):

  1. The 5 desktop stub pairs are deleted — `find source/platform -name '*fake*'` returns only `saves/fake_cloud_save_client.{cpp,hpp}` (the retained doctest double; the glob is `*fake*`, not `*_fake*` — the retained file has no underscore prefix); `save_service_fake`, `title_service_fake`, `fake_auth_client`, `themes/firmware_extract_fake`, and `sysmod/sysmod_store_fake` no longer exist.
  2. No `*_fake`-vs-`*_switch` **implementation-selection** seam survives — the factory/include seams that chose a deleted desktop stub over its `*_switch` counterpart (all in `main.cpp` and `home_activity.cpp`) are gone, leaving each `*_switch` impl the sole implementation behind its interface. **Platform-portability seams are explicitly RETAINED** (scope decision Option D, 2026-06-05): `#ifdef __SWITCH__ … #else … #endif` blocks that return a platform path string (Switch absolute vs host-relative) or a host-compilable fallback impl, plus `_WIN32`/`localtime_r` seams, are NOT desktop *stub-selection* and stay — they exist to keep the retained host doctest suite green (removing the host path branches makes host tests write to absolute Switch paths like `/themes` and fail). A residual `grep -rn 'ifndef __SWITCH__\|#else' source/` therefore still lists these portability seams; that is expected, not a violation.
  3. No `source/` file references a deleted stub or a desktop-only symbol/include — `grep -rnE 'PLATFORM_DESKTOP|SDL2|GLFW|_fake' source/` returns nothing except the retained `fake_cloud_save_client` references.
  4. `tests/Makefile` still builds and `make -C tests test` passes unchanged, confirming core + platform-neutral logic is unregressed and `saves/fake_cloud_save_client.*` is still compiled by the suite.

**Plans**: 4 plans
Plans:

- [x] 05-01-PLAN.md — Collapse main.cpp and home_activity.cpp factory/include seams (consumer edits before stub deletion)
- [x] 05-02-PLAN.md — Delete 9 desktop stub files from source/platform/
- [x] 05-03-PLAN.md — (Option D rescope) Keep platform-portability #else seams; clean 3 stale desktop-fake comments (SIMPL-03). The "22 #else" were portability seams, retained.
- [x] 05-04-PLAN.md — Ran all 4 success-criterion checks + host doctest gate (208/208 passing)

### Phase 6: Strip Desktop from Build System

**Goal**: The build system offers only the Switch toolchain — CMake has no `PLATFORM_DESKTOP` path, the desktop helper scripts are gone, and a clean Switch build still produces `thomaz.nro`.
**Depends on**: Phase 5 (source seams must be Switch-only before the build stops offering a desktop branch, so the tree is never left unbuildable mid-phase)
**Requirements**: BUILD-01, BUILD-02, BUILD-03
**Success Criteria** (what must be TRUE):

  1. No `PLATFORM_DESKTOP` token remains in `CMakeLists.txt` — `grep -n 'PLATFORM_DESKTOP' CMakeLists.txt` returns nothing; the `elseif (PLATFORM_DESKTOP)` link branch and the `PLATFORM_DESKTOP` packaging branch are removed, and dual-target comments are gone.
  2. The desktop helper scripts are removed — `scripts/build-desktop.sh` and `scripts/run-desktop.sh` no longer exist in the repo.
  3. A clean Switch build via `scripts/build-switch.sh` (devkitPro Docker) succeeds end-to-end and produces `build_switch/thomaz.nro`.

**Plans**: 2 plans
Plans:

- [x] 06-01-PLAN.md — Strip the two `PLATFORM_DESKTOP` CMake branches + dual-target comments and delete `scripts/build-desktop.sh` / `scripts/run-desktop.sh` (BUILD-01, BUILD-02)
- [x] 06-02-PLAN.md — Clean Switch build via `scripts/build-switch.sh` (devkitPro Docker) proves the stripped tree produces `build_switch/thomaz.nro` (BUILD-03)

### Phase 7: Docs Cleanup & Final Verification Gate

**Goal**: Repository docs and code comments describe a Switch-only tree with the host-doctest + Switch-build verification flow, and both single-target gates are confirmed green together.
**Depends on**: Phase 6
**Requirements**: DOC-01, VERIF-01
**Success Criteria** (what must be TRUE):

  1. No doc or comment instructs or implies a desktop PC build — the `CMakeLists.txt` header comment, README/build docs, and any `AGENTS.md`/`CLAUDE.md` build notes describe a Switch-only tree and the host-doctest + `build-switch.sh` verification flow; a grep for desktop-build instructions (`build-desktop`, `PLATFORM_DESKTOP`, "desktop PC") in tracked docs returns nothing stale.
  2. The host doctest suite passes — `make -C tests test` is green and still compiles the retained `saves/fake_cloud_save_client.*` test double.
  3. The Switch build is clean — `scripts/build-switch.sh` produces `build_switch/thomaz.nro` after all removals, with the two single-target gates (host doctest + Switch build) standing as the milestone's verification flow.

**Plans**: 2 plans
Plans:

- [x] 07-01-PLAN.md — Clean README living docs to Switch-only (drop Desktop (PC) section, point at build-switch.sh, document the two-gate verification flow); grep gate green (DOC-01)
- [x] 07-02-PLAN.md — Run both single-target gates together — host doctest (make -C tests test) + Switch build (scripts/build-switch.sh → build_switch/thomaz.nro) — confirm green (VERIF-01)

## Progress

| Phase | Milestone | Plans Complete | Status   | Completed  |
|-------|-----------|----------------|----------|------------|
| 1. Remove Community Feature        | v1.0 | 3/3 | Complete | 2026-06-04 |
| 2. API Security + Regression Tests | v1.0 | 3/3 | Complete | 2026-06-05 |
| 3. C++ Platform Hardening          | v1.0 | 4/4 | Complete | 2026-06-05 |
| 4. C++ Activity Hardening          | v1.0 | 6/6 | Complete | 2026-06-05 |
| 1. Privileged Extraction Spike     | v0.5.0 | 5/5 | Complete | 2026-06-05 |
| 2. Full Extraction Engine          | v0.5.0 | 4/4 | Complete | 2026-06-05 |
| 3. Theme UI Integration            | v0.5.0 | 0/3 | Carried forward | - |
| 4. Forwarder (Optional)            | v0.5.0 | 0/1 | Carried forward | - |
| 5. Collapse Source Seams to Switch-Only | v1.1 | 0/4 | Not started | - |
| 6. Strip Desktop from Build System      | v1.1 | 2/2 | Complete   | 2026-06-06 |
| 7. Docs Cleanup & Final Verification Gate | v1.1 | 2/2 | Complete   | 2026-06-06 |
