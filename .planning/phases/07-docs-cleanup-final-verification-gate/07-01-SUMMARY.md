---
phase: 07-docs-cleanup-final-verification-gate
plan: 01
subsystem: docs
tags: [readme, docs, switch-only, build-docs]
requires:
  - phase: 06-strip-desktop-build-system
    provides: desktop-free CMakeLists + deleted desktop scripts
provides:
  - Switch-only README build/verification docs (DOC-01)
affects: [07-02, milestone-v1.1-completion]
tech-stack:
  added: []
  patterns: []
key-files:
  created: []
  modified:
    - README.md
key-decisions:
  - "Living-doc grep gate scoped to exclude docs/superpowers/ and .planning/ (both historical/working archives)"
patterns-established: []
requirements-completed: [DOC-01]
duration: ~3min
completed: 2026-06-05
---

# Phase 7 — Plan 01: README Switch-Only Docs Cleanup

**README now describes a Switch-only tree built via `scripts/build-switch.sh` and verified by the two single-target gates — every stale desktop-build instruction is gone.**

## Performance
- **Duration:** ~3 min
- **Completed:** 2026-06-05
- **Tasks:** 2
- **Files modified:** 1 (README.md)

## Accomplishments
- Removed the entire "### Desktop (PC)" section (`build-desktop.sh` + `build_desktop/thomaz` + FakeTitleService note).
- De-desktopped the Status blurb: "(Switch e desktop)" → "na Switch".
- Rewrote the Switch build subsection to recommend `scripts/build-switch.sh` (Docker `devkitpro/devkita64` default; native when `DEVKITPRO` set), output `build_switch/thomaz.nro`.
- Added a from-source deploy note (copy to `/switch/thomaz.nro` or push via `nxlink`).
- Replaced the old host-tests blurb with the **two single-target verification gates** (host doctest `make -C tests test` + Switch build `scripts/build-switch.sh`), in pt-BR.
- Verified `CMakeLists.txt` header is already Switch-only (Plan 06-01) — unchanged.

## Verification
- README-scoped desktop grep gate: clean (exit 1).
- Canonical living-doc gate `git grep -nE 'build-desktop|run-desktop|PLATFORM_DESKTOP|build_desktop|-DUSE_SDL2|-DPLATFORM_DESKTOP|desktop PC' -- '*.md' 'CMakeLists.txt' ':!docs/superpowers/' ':!.planning/'`: **empty** (exit 1).
- Required strings present: `scripts/build-switch.sh`, `make -C tests test`, `build_switch/thomaz.nro`, `nxlink`.
- `docs/superpowers/**` historical archives intentionally untouched (D-02).

## Files Modified
- `README.md` — "🛠️ Compilar do código-fonte" section rewritten Switch-only; Status blurb de-desktopped.

## Notes
- Line "não há mais smoke run de desktop" documents the *removal* of the desktop smoke — accurate, and outside the canonical DOC-01 gate terms.
- DOC-01 satisfied; Phase 7 success criterion #1 met.
