# Roadmap: thomaz

## Milestones

- ✅ **v1.0 Hardening** — Phases 1-4 (shipped 2026-06-05)
- 🟡 **v0.5.0 Theme Extraction** — Phases 1-2 shipped 2026-06-05; Phases 3-4 open
- ✅ **v1.1 Switch-Only Simplification** — Phases 5-7 (shipped 2026-06-06)

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

<details>
<summary>✅ v1.1 Switch-Only Simplification (Phases 5-7) — SHIPPED 2026-06-06</summary>

Removed the desktop (PC/SDL2/GLFW) build target entirely so the source tree targets only Nintendo Switch, cutting the platform-abstraction surface that existed solely to run the GUI on PC — with **zero change to the shipped `.nro`**. Sequenced for safety: collapsed the source selection seams first, then stripped the build system, then cleaned docs and ran the final combined gate. Verified by a green host doctest suite (209/209) and a clean Switch build (`build_switch/thomaz.nro`, 7.7 MB), redeployed and confirmed on real hardware. Full detail: [milestones/v1.1-ROADMAP.md](milestones/v1.1-ROADMAP.md).

- [x] Phase 5: Collapse Source Seams to Switch-Only (4/4 plans) — completed 2026-06-06
- [x] Phase 6: Strip Desktop from Build System (2/2 plans) — completed 2026-06-06
- [x] Phase 7: Docs Cleanup & Final Verification Gate (2/2 plans) — completed 2026-06-06

</details>

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
| 5. Collapse Source Seams to Switch-Only | v1.1 | 4/4 | Complete   | 2026-06-06 |
| 6. Strip Desktop from Build System      | v1.1 | 2/2 | Complete   | 2026-06-06 |
| 7. Docs Cleanup & Final Verification Gate | v1.1 | 2/2 | Complete   | 2026-06-06 |
