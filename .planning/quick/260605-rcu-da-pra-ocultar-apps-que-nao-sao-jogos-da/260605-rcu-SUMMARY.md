---
phase: quick-260605-rcu
plan: "01"
subsystem: games-visibility
tags: [title-filter, visibility, i18n, persistence]
dependency_graph:
  requires: []
  provides: [title-visibility-system]
  affects: [game_list_activity, title_service_fake, title_service_switch]
tech_stack:
  added: [core/title_filter, platform/title_visibility_store]
  patterns: [pure-core-logic, platform-io-abstraction, rebuild-list-pattern]
key_files:
  created:
    - source/core/title_filter.hpp
    - source/core/title_filter.cpp
    - source/platform/title_visibility_store.hpp
    - source/platform/title_visibility_store.cpp
    - tests/test_title_filter.cpp
  modified:
    - source/platform/title.hpp
    - source/platform/title_service_switch.cpp
    - source/platform/title_service_fake.cpp
    - source/app/game_list_activity.hpp
    - source/app/game_list_activity.cpp
    - resources/i18n/pt-BR/thomaz.json
    - resources/i18n/en-US/thomaz.json
    - resources/i18n/fr/thomaz.json
    - resources/i18n/ru/thomaz.json
    - resources/i18n/zh-Hans/thomaz.json
    - tests/Makefile
decisions:
  - "classify() returns NonGame only when save_data_size==0 AND startup_user_account==0 (conservative heuristic — prefers false-positives over missed games)"
  - "effectively_hidden() checks force_shown first (priority), then force_hidden, then heuristic"
  - "toggle_title cycles: force_shown→force_hidden→force_shown on repeat; default→force_shown (if hidden by heuristic) or force_hidden (if visible)"
  - "T-rcu-01 (Tampering): parser validates each id with is_valid_hex_id(); invalid lines silently ignored"
  - "Replaced single-shot populate() with rebuildList() + allTitles_ member; UI rebuilt on every toggle"
  - "Sphaira entry added to FakeTitleService with save=0, acct=0 so desktop demonstrates the feature"
metrics:
  duration: "~35 minutes"
  completed: "2026-06-05"
  tasks: 3
  files_changed: 12
---

# Phase quick-260605-rcu Plan 01: Ocultar apps não-jogo da lista — Summary

**One-liner:** Hybrid title-visibility system — heuristic auto-hides forwarders (save=0 & acct=0), with per-title force overrides and a global toggle, persisted to title_visibility.txt.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Campo startup_user_account + title_filter + TitleVisibilityStore | f75ef99 | title.hpp, title_service_switch.cpp, title_service_fake.cpp, core/title_filter.{hpp,cpp}, platform/title_visibility_store.{hpp,cpp}, tests/test_title_filter.cpp, tests/Makefile |
| 2 | GameListActivity — rebuildList(), botão X, botão Y, badge Oculto | 15c238b | game_list_activity.{hpp,cpp} |
| 3 | i18n — 5 locales com chaves de visibilidade em thomaz/games/ | c816ffc | 5 × thomaz.json |

## Verification Results

### Task 1 — tests/Makefile + doctest
```
[doctest] test cases: 9 | 9 passed | 0 failed
[doctest] Status: SUCCESS!
```
Full suite: 206 tests, 0 failed.

### Task 2 — cmake --build
```
[100%] Built target thomaz   (no errors, no new warnings)
```

### Task 3 — JSON validation + rebuild
```
OK resources/i18n/pt-BR/thomaz.json
OK resources/i18n/en-US/thomaz.json
OK resources/i18n/fr/thomaz.json
OK resources/i18n/ru/thomaz.json
OK resources/i18n/zh-Hans/thomaz.json
[100%] Built target thomaz
```

## Architecture

```
InstalledTitle.startup_user_account  ←  NsTitleService (Switch) / FakeTitleService (desktop)
        │
        ▼
core::classify(t)                    →  TitleKind::{Game, NonGame}
        │
core::effectively_hidden(t, fh, fs)  →  bool  (pure, no libnx, testable)
        │                                 [force_shown > force_hidden > heuristic]
        ▼
TitleVisibilityStore                 →  load()/save() via read/write_text_file
  force_hidden_, force_shown_               thomaz-cache/title_visibility.txt (desktop)
  show_hidden_                              /switch/thomaz/config/title_visibility.txt (Switch)
        │
        ▼
GameListActivity::rebuildList()      →  filters allTitles_, badges, Y-button per row
  BUTTON_X on gameListFrame          →  toggle_show_hidden() + save() + rebuildList()
  BUTTON_Y per row                   →  toggle_title() + save() + rebuildList()
```

## Deviations from Plan

None — plan executed exactly as written.

The `is_valid_hex_id()` helper in `title_visibility_store.cpp` was added per T-rcu-01 (threat model: Tampering — validate ids before inserting). This is a mitigation mandated by the threat register, not a deviation.

## Known Stubs

None — the Sphaira entry in FakeTitleService (save=0, acct=0) is the real test vector for the heuristic on desktop; it is not a UI stub. Data flows end-to-end from listInstalled() through effectively_hidden() to the list filter.

## Threat Flags

No new trust-boundary surface introduced beyond what was already in the plan's threat model. T-rcu-01 mitigation was implemented (id validation in parser).

## Self-Check: PASSED

| Item | Status |
|------|--------|
| source/core/title_filter.hpp | FOUND |
| source/core/title_filter.cpp | FOUND |
| source/platform/title_visibility_store.hpp | FOUND |
| source/platform/title_visibility_store.cpp | FOUND |
| tests/test_title_filter.cpp | FOUND |
| Commit f75ef99 (Task 1) | FOUND |
| Commit 15c238b (Task 2) | FOUND |
| Commit c816ffc (Task 3) | FOUND |
