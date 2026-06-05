# Roadmap: thomaz

## Milestones

- ✅ **v1.0 Hardening** — Phases 1-4 (shipped 2026-06-05)
- 🟡 **v0.5.0 Theme Extraction** — Phases 1-2 shipped 2026-06-05; Phases 3-4 open

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
