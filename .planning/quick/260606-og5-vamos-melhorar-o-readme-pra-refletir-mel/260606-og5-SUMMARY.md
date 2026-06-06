---
phase: quick-260606-og5
plan: 01
subsystem: docs
tags: [readme, documentation, project-description]
requires: []
provides:
  - "Accurate multi-feature README (cheats, mods, temas, saves) for thomaz"
affects:
  - README.md
tech-stack:
  added: []
  patterns: []
key-files:
  created: []
  modified:
    - README.md
decisions:
  - "Described api/ as 'auth + cloud saves' (community feed removed in v1.0); did not advertise the residual source/*/feed dirs since they hold auth/session code, not a user feature"
  - "Merged the two planned tasks into a single coherent rewrite of README.md (same file, tightly coupled feature-list vs structure/roadmap sections), committed atomically"
  - "Did NOT advertise the sysmodules card — it is visibility=gone in home.xml and unreachable from the menu"
metrics:
  duration: ~6min
  completed: 2026-06-06
---

# Quick Task 260606-og5: README Multi-Feature Rewrite Summary

Rewrote README.md so it accurately presents thomaz as a Switch-only, multi-feature homebrew hub (cheats + mods + temas + cloud saves) instead of the cheats-only manager the old README described — every claim grounded in the verified codebase.

## What Changed

- **Headline** reframed from "Gerenciador de trapaças (cheats)" to a hub framing covering cheats, mods, temas e backups/saves na nuvem, with a touch/mouse/controller UI. The em-desenvolvimento caveat was preserved.
- **Funcionalidades** restructured into one block per active home card: Trapaças (cheats), Temas (Themezer + on-device firmware extraction via exelix engine + hactool, no user prod.keys), Mods (GameBanana browse/install/extract via libarchive), Saves (backup + cloud sync with optimistic locking, login required), Configurações (idioma / auto-update / atualizar base). Plus a cross-cutting block: conta/login boot screen, title-visibility filter, bilingual UI, dark violet theme.
- **Language claim** corrected to "Totalmente bilíngue PT-BR / EN, com traduções parciais em outros idiomas (fr, ru, zh)" — matches the verified `resources/i18n/` tree (pt-BR + en-US full; fr/ru/zh partial).
- **Como usar** kept the accurate cheats walkthrough and the "Onde os cheats são gravados" path/fallback subsection, scoped it clearly to cheats, and added short factual entry-point lines for Mods, Temas, and Saves.
- **Compilar** section confirmed accurate against `scripts/build-switch.sh` + `CMakeLists.txt` — no drift; renamed the backend subsection from "feed / auth" to "auth + saves na nuvem".
- **Estrutura** updated to the real core/platform/app subdirs (mods, themes, saves) and the api/ description changed from "auth, feed, stubs de saves" to "auth + saves na nuvem" (community feed removed in v1.0).
- **Roadmap** refreshed to mark shipped v1.0/v1.1 work done (mods, temas, saves, hardening, Switch-only) and keep only genuinely-deferred items (hardware UAT, lazy icons, embedded cacert TLS, PERF-01/02).
- **Créditos** added Themezer, exelix11/SwitchThemeInjector, and GameBanana alongside the existing credits.
- **Licença / Requisitos / Instalação / Capturas / Aviso** kept accurate (Aviso lightly extended to mention mods).

## Ground-Truth Verification

| Claim | Source verified |
|-------|-----------------|
| 5 active home cards (Temas hero, Cheats, Mods, Saves, Configurações); sysmodules hidden | `resources/xml/activity/home.xml` — `systemCard` has `visibility="gone"` |
| Mods = GameBanana + libarchive extraction | `source/core/mods/gamebanana_*`, `source/platform/mods/` |
| Temas = Themezer + on-device firmware extraction | `source/core/themes/themezer_*`, `source/platform/themes/` |
| Saves = cloud sync, login required | `source/core/saves/save_sync*`, `source/platform/saves/` |
| Build command, output path, two-gate verification | `scripts/build-switch.sh` (outputs `build_switch/thomaz.nro`; Docker `devkitpro/devkita64` default, `DEVKITPRO=` native) |
| Partial fr/ru/zh locales | `resources/i18n/{fr,ru,zh-Hans}` have only `thomaz.json` (+ demo/brls), not the full mods/themes/system set |

## Deviations from Plan

**1. [Process - Consolidation] Both plan tasks executed as one atomic commit**
- The plan split the rewrite into Task 1 (headline/Funcionalidades/Como usar) and Task 2 (build/structure/roadmap/credits), each committing README.md. Because both edit the same file and the verifier requires internal consistency between them, I rewrote the whole README in one pass and made a single atomic commit covering both tasks' scope. Both verification gates pass.
- **Commit:** 30f0330

No content was invented beyond the codebase / PROJECT.md / MILESTONES.md. No auto-fix (Rule 1-3) or architectural (Rule 4) issues arose — this was a docs-only task.

## Verification Results

- Task 1 gate: `grep Mods && Temas && Saves|nuvem && GameBanana && Themezer` → PASS
- Task 2 gate: `grep Themezer && GameBanana && build_switch/thomaz.nro && build-switch.sh && ! "stubs de saves"` → PASS
- Sysmodules/feed negative check: no `sysmodule`, `systemCard`, `stubs de saves`, or community-feed mentions remain → clean
- Commit contains only README.md, no file deletions

## Known Stubs

None.

## Self-Check: PASSED

- README.md exists and modified (71 insertions, 30 deletions)
- Commit 30f0330 exists on main
