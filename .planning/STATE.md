---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: Switch-Only Simplification
status: planning
last_updated: "2026-06-05T22:45:00.000Z"
last_activity: 2026-06-05
progress:
  total_phases: 3
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-06-05)

**Core value:** Remove the desktop (PC/SDL2/GLFW) build target with zero change to the shipped Switch `.nro` — proven by a green host doctest suite (`tests/Makefile`) and a clean Switch build (`scripts/build-switch.sh`)
**Current focus:** v1.1 Switch-Only Simplification roadmapped (Phases 5-7) — ready to plan Phase 5

## Current Position

Phase: 5 — Collapse Source Seams to Switch-Only (not started)
Plan: —
Status: Roadmap created; awaiting Phase 5 planning
Last activity: 2026-06-05 — Milestone v1.1 roadmap created (Phases 5-7)

Roadmap: collapse source seams first (Phase 5), then strip the build system (Phase 6), then docs cleanup + final combined gate (Phase 7). Sequenced so the source layer is single-target before CMake stops offering the desktop branch — the tree is never left unbuildable mid-phase.

## Performance Metrics

**Velocity:**

- Total plans completed: 10
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 3 | - | - |
| 02 | 3 | - | - |
| 03 | 4 | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
| Phase 01-remove-community-feature P02 | 308 | 2 tasks | 24 files |
| Phase 01-remove-community-feature P03 | 12 | 2 tasks | 3 files |
| Phase 02-api-security-regression-tests P01 | 15 | 2 tasks | 4 files |
| Phase 02-api-security-regression-tests P02 | 4 | 2 tasks | 4 files |
| Phase 02-api-security-regression-tests P03 | 5min | 2 tasks | 1 files |
| Phase 03-c-platform-hardening P04 | 5min | 1 tasks | 2 files |
| Phase 04-c-activity-hardening P01 | 3min | 3 tasks | 3 files |
| Phase 04-c-activity-hardening P05 | 3min | 1 tasks | 1 files |
| Phase 04-c-activity-hardening P02 | 20min | 2 tasks | 8 files |
| Phase 04-c-activity-hardening P04 | 25min | 2 tasks | 11 files |
| Phase 04-c-activity-hardening P06 | 10min | 4 tasks | 9 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- v1.1 Roadmap: 3 coarse phases — source-seam collapse first (Phase 5), build-system strip second (Phase 6), docs + final gate third (Phase 7); order enforces a buildable tree at every phase boundary
- v1.1 verification: host doctest suite (`tests/Makefile`) is the in-phase gate for the source layer (independent of the desktop build, survives); the Switch build (`build-switch.sh`, devkitPro Docker) is the post-removal compile gate — no non-devkitPro full-tree compile check remains
- v1.1 keep: `saves/fake_cloud_save_client.*` is a doctest test double, NOT a desktop GUI stub — explicitly retained and still compiled by the suite
- v1.1 removal surface (enumerated): 5 desktop stub pairs (`save_service_fake`, `title_service_fake`, `fake_auth_client`, `themes/firmware_extract_fake`, `sysmod/sysmod_store_fake`); ~32 files carry `__SWITCH__` seams — collapse the `#else`/`#ifndef __SWITCH__` desktop branch in each; `CMakeLists.txt` `PLATFORM_DESKTOP` link (~98-102) + packaging (~130-135) branches; delete `scripts/build-desktop.sh` + `scripts/run-desktop.sh`

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 5: Seams branch two ways — `#ifdef __SWITCH__` (keep the inner Switch code) and `#ifndef __SWITCH__` (drop the inner desktop code). Collapse each correctly; do NOT remove always-Switch code just because it sat behind an `#ifdef __SWITCH__` guard.
- Phase 5: Do NOT delete `saves/fake_cloud_save_client.*` — it is the retained doctest double; `tests/Makefile` compiles it.
- Phase 6: Switch build runs via Docker (`devkitpro/devkita64`); a devkitPro image/Docker must be available to satisfy BUILD-03. If unavailable, the nro-build criterion becomes a manual/deferred gate.
- Phase 7: DOC-01 must sweep `CMakeLists.txt` header comment, README/build docs, and any `AGENTS.md`/`CLAUDE.md` build notes — confirm which docs actually mention the desktop build before editing.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260605-ot7 | Theme-apply screen UX: busy-guard download/apply button against repeated clicks | 2026-06-05 | 4f9205b | [260605-ot7-melhorar-a-ux-da-tela-de-aplicacao-de-te](./quick/260605-ot7-melhorar-a-ux-da-tela-de-aplicacao-de-te/) |
| 260605-qgb | BootActivity boot screen: login/guest entry before HomeActivity when no session | 2026-06-05 | d668063 | [260605-qgb-vamos-adicionar-uma-bootscreen-no-app-us](./quick/260605-qgb-vamos-adicionar-uma-bootscreen-no-app-us/) |
| 260605-rcu | Hybrid title-visibility system: heuristic auto-hides forwarders (save=0 & acct=0) + per-title overrides + global toggle | 2026-06-05 | c816ffc | [260605-rcu-da-pra-ocultar-apps-que-nao-sao-jogos-da](./quick/260605-rcu-da-pra-ocultar-apps-que-nao-sao-jogos-da/) |

## Deferred Items

Items acknowledged and deferred at milestone close on 2026-06-05:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| Performance | PERF-01: avoid double archive traversal | v2 backlog | Roadmap |
| Performance | PERF-02: cache CloudStatus for upload skip | v2 backlog | Roadmap |
| Hardware UAT | Phase 03: TLS-insecure red banner visual render across all activity screens (forced latch) | human_needed | v1.0 close |
| Hardware UAT | Phase 03: save_service_switch.cpp compiles clean under devkitPro Switch toolchain (IN-03 uid_from_hex) | human_needed | v1.0 close |
| Hardware UAT | Phase 04: 5 activity-pop / dialog-button UAF crash-path scenarios (settings, clear_cheats, mod_manager, theme_detail) | testing | v1.0 close |

All deferred UAT/verification items are on-hardware checks the host test suite cannot exercise; all automated gates (Vitest + doctest + clean `-DUSE_SDL2=ON` build) passed. Resume with `/gsd-verify-work 03` / `/gsd-verify-work 04` when a Switch is available.

## Session Continuity

Last session: 2026-06-05T22:45:00Z
Stopped at: v1.1 roadmap created (Phases 5-7); traceability + STATE updated
Resume file: None

## Operator Next Steps

- Plan the first v1.1 phase with /gsd-plan-phase 5
</content>
