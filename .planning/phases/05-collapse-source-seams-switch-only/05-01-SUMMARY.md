---
phase: 05-collapse-source-seams-switch-only
plan: 01
subsystem: app-entry / platform-factory
tags: [switch-only, seam-collapse, preprocessor, refactor, SIMPL-02, SIMPL-03]
requires:
  - source/platform/title_service_switch.hpp (NsTitleService — pre-existing)
  - source/platform/save_service_switch.hpp (NsSaveService — pre-existing)
  - source/platform/sysmod/sysmod_store.hpp (SysmoduleStore — pre-existing)
provides:
  - "source/main.cpp: unconditional Switch service factories (no desktop branch)"
  - "source/app/home_activity.cpp: unconditional Switch sysmod store factory (no desktop branch)"
affects:
  - Plan 05-02 (stub deletion) — consumer references now cleared, so deleting the *_fake stubs leaves no dangling #include
tech-stack:
  added: []
  patterns:
    - "Collapse #ifdef __SWITCH__ / #else seams by keeping the Switch branch unconditional and deleting the desktop #else branch"
key-files:
  created: []
  modified:
    - source/main.cpp
    - source/app/home_activity.cpp
decisions:
  - "Left the no-#else #ifdef __SWITCH__ titleService->exit() guard intact (Category B, not a SIMPL-02 target)"
  - "Did NOT delete or edit the *_fake stub files or interface comments — that is Plan 05-02 / Step 5 scope; this plan only clears consumer references"
metrics:
  duration: ~5min
  completed: 2026-06-05
  tasks: 2
  files: 2
---

# Phase 5 Plan 01: Collapse Source Seams (Consumer Factories) Summary

Removed the three `#ifdef __SWITCH__ / #else` factory and include selection seams in `main.cpp` and `home_activity.cpp`, leaving only the Switch path unconditional so consumer files no longer reference any desktop stub header before those stubs are deleted in Plan 02.

## What Was Built

- **`source/main.cpp`** — three seams collapsed:
  1. Include seam (was lines 7-14): removed the `#ifdef __SWITCH__ / #else / #endif` wrapper and the two fake includes; `<switch.h>`, `title_service_switch.hpp`, and `save_service_switch.hpp` are now included unconditionally.
  2. Title-service factory (was lines 70-79): `NsTitleService` + `init()` is now unconditional; the `FakeTitleService` `#else` branch was removed.
  3. Save-service factory (was lines 82-86): `NsSaveService` is now unconditional; the `FakeSaveService` `#else` branch was removed.
  - The no-`#else` `#ifdef __SWITCH__ titleService->exit(); #endif` guard (now ~line 118) was intentionally left intact — it is Category B per RESEARCH.md Section 2 and not a SIMPL-02 target.

- **`source/app/home_activity.cpp`** — two seams collapsed:
  1. Include seam (was lines 14-16): deleted the 3-line `#ifndef __SWITCH__` / `#include "platform/sysmod/sysmod_store_fake.hpp"` / `#endif` block. `sysmod_store.hpp` remains included unconditionally.
  2. Sysmod store factory (was lines 94-98): `auto store = std::make_shared<SysmoduleStore>();` is now unconditional; the `FakeSysmoduleStore` `#else` branch was removed.

## Verification

All acceptance criteria for both tasks passed:

- `main.cpp`: zero `FakeTitleService`/`FakeSaveService`/`*_fake` references; exactly one `#ifdef __SWITCH__` remains (the `exit()` guard); zero `#else`; `NsTitleService`+`NsSaveService` factories present; 2 unconditional switch includes.
- `home_activity.cpp`: zero `FakeSysmoduleStore`/`sysmod_store_fake` references; zero `#ifndef __SWITCH__`; `SysmoduleStore` factory present.
- Both consumer files (`main.cpp`, `home_activity.cpp`) now contain **zero** references to any of the three fake classes/headers — the plan's core objective.

The only remaining `*_fake` / `Fake*` matches across `source/` are inside the stub files themselves (`save_service_fake.*`, `title_service_fake.*`, `fake_auth_client.*`, `sysmod_store_fake.*`) and the stale interface comments in `sysmod_store.hpp` / `auth_client.hpp`. These are intentionally out of scope for this plan — RESEARCH.md Section 6 sequences stub deletion (Step 3) and comment cleanup (Step 5) into Plan 02. No dangling `#include` exists at any point because the consumer references were cleared first (Risk 1 mitigation).

Host doctest suite is unaffected: `tests/Makefile` compiles neither `main.cpp` nor `home_activity.cpp` (confirmed), so no test changes were needed (RESEARCH.md Section 5).

## Deviations from Plan

None — plan executed exactly as written.

## Authentication Gates

None.

## Known Stubs

None introduced by this plan. (The pre-existing `*_fake` stub files remain on disk by design until Plan 02 deletes them; they are no longer referenced by any consumer.)

## Threat Model Compliance

- T-05-01 (main.cpp seam collapse): mitigated — acceptance grep confirmed exact presence of `NsTitleService`/`NsSaveService` factories and absence of fake references.
- T-05-02 (Category B guard): mitigated — the no-`#else` `titleService->exit()` guard was left untouched; grep confirms exactly one `#ifdef __SWITCH__` remains in main.cpp.
- T-05-03 (fake_cloud_save_client): not touched — this plan edited only main.cpp and home_activity.cpp.

No new security-relevant surface introduced (edit-only, removes consumer-side desktop selection).

## Self-Check: PASSED

- FOUND: .planning/phases/05-collapse-source-seams-switch-only/05-01-SUMMARY.md
- FOUND commit 1b29fce (main.cpp seam collapse)
- FOUND commit 6952b1c (home_activity.cpp seam collapse)
