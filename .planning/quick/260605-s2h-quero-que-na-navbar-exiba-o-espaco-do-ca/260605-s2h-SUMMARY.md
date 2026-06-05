---
quick_id: 260605-s2h
phase: quick
plan: 260605-s2h
subsystem: app/ui
tags: [navbar, storage, wifi, system-status, borealis, i18n, doctest]
dependency_graph:
  requires: []
  provides: [install_system_status, format_bytes, used_ratio, query_system_status]
  affects: [all 13 activity screens]
tech_stack:
  added:
    - source/core/storage_format.hpp/.cpp (pure decimal SI formatter + ratio helper)
    - source/platform/system_status.hpp/.cpp (Switch ns/nifm query + desktop stub)
    - tests/test_storage_format.cpp (doctest coverage)
  patterns:
    - "#ifdef __SWITCH__ / #else stub pattern (mirrors title_service_switch.cpp)"
    - "Borealis Box widget injection into hint_box (mirrors install_header_username)"
key_files:
  created:
    - source/core/storage_format.hpp
    - source/core/storage_format.cpp
    - source/platform/system_status.hpp
    - source/platform/system_status.cpp
    - tests/test_storage_format.cpp
  modified:
    - source/app/app_header.hpp
    - source/app/app_header.cpp
    - source/app/cheat_detail_activity.cpp
    - source/app/clear_cheats_activity.cpp
    - source/app/game_list_activity.cpp
    - source/app/home_activity.cpp
    - source/app/mod_browser_activity.cpp
    - source/app/mod_detail_activity.cpp
    - source/app/mod_manager_activity.cpp
    - source/app/save_detail_activity.cpp
    - source/app/save_manager_activity.cpp
    - source/app/settings_activity.cpp
    - source/app/system_activity.cpp
    - source/app/theme_browser_activity.cpp
    - source/app/theme_detail_activity.cpp
    - resources/i18n/pt-BR/thomaz.json
    - resources/i18n/en-US/thomaz.json
    - resources/i18n/fr/thomaz.json
    - resources/i18n/ru/thomaz.json
    - resources/i18n/zh-Hans/thomaz.json
decisions:
  - "Decimal SI units (KB=1e3, MB=1e6, GB=1e9) for format_bytes — matches CONTEXT.md spec"
  - "used_ratio: free>total returns 1.0f (all used), free==total returns 0.0f (nothing used)"
  - "nifm init/exit per-call as specified in CONTEXT.md — fast IPC, no global state"
  - "install_system_status(this) added to home_activity ONLY in onContentAvailable, NOT refreshHeaderUsername"
  - "WiFi indicator: 3 Box bars, increasing height, FLEX_END alignment, first wifi_strength bars lit"
metrics:
  completed_date: "2026-06-05"
  tasks_completed: 3
  tasks_total: 3
  files_created: 5
  files_modified: 20
---

# Quick Task 260605-s2h: Navbar System Status Indicators

**One-liner:** SD/NAND progress bars and WiFi 3-bar signal indicator injected into
hint_box of all 13 screens, with pure helper coverage via doctest and a Switch-only
ns+nifm platform query behind an `#ifdef __SWITCH__` stub.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Pure helpers + platform query + desktop test | 5f80f60 | storage_format.hpp/.cpp, system_status.hpp/.cpp, test_storage_format.cpp |
| 2 | install_system_status widget in app_header | 504d746 | app_header.hpp, app_header.cpp |
| 3 | Wire 13 activities + add i18n keys to 5 locales | 857f6da | 13 activity .cpp files, 5 locale .json files |

## What Was Built

### Task 1 — Pure helpers + desktop-testable platform stub

`source/core/storage_format.hpp/.cpp`
- `format_bytes(uint64_t bytes) -> string`: decimal SI (KB=1e3, MB=1e6, GB=1e9),
  one decimal place for KB/MB/GB, no decimal for bytes.
- `used_ratio(uint64_t total, uint64_t free) -> float`: clamped to [0.0f, 1.0f];
  total==0 guard returns 0.0f; free>total returns 1.0f (all used).

`source/platform/system_status.hpp/.cpp`
- `StorageInfo { total_bytes, free_bytes }` and `SystemStatus { sd, nand, wifi_connected, wifi_strength }`
- `query_system_status()`: Switch block uses `nsGetTotalSpaceSize`/`nsGetFreeSpaceSize`
  (NcmStorageId_SdCard + NcmStorageId_BuiltInUser) and nifm init/exit per-call.
  `#else` stub returns zeroed `SystemStatus{}`.

`tests/test_storage_format.cpp`: 13 doctest assertions covering all edge cases.

### Task 2 — Borealis widget

`install_system_status(brls::Activity*)` in app_header.cpp:
- Resolves `brls/applet_frame/hint_box` via `dynamic_cast<brls::Box*>(getView(...))`.
- For SD and NAND: a column of [Label + progress track + fill bar], margin-right 10px.
  Fill width = `kTrackW (80px) * used_ratio(...)`.
- WiFi: 3 Box bars (4px wide, heights 5/8/11px), FLEX_END aligned, first `wifi_strength`
  bars lit with accent_bright (`nvgRGB(0x92, 0x77, 0xFF)`), rest dim.
- No `<switch.h>` in this file — all IPC lives in system_status.cpp.

### Task 3 — Wiring and i18n

All 13 activity `onContentAvailable()` bodies now call `install_system_status(this)`
immediately before `install_header_username(this)`. `home_activity.cpp` has exactly
one call (in `onContentAvailable`); `refreshHeaderUsername()` is unchanged.

Five locale files updated with `"status": { "sd", "console", "wifi" }` keys.
All files validated with `python3 json.load` — no parse errors.

## Test Results

```
[doctest] test cases: 2 | 2 passed | 0 failed
[doctest] assertions: 13 | 13 passed | 0 failed
```

Full suite: 202 passed, 6 pre-existing failures (filesystem permission errors in
test_active_theme_store, test_cfw_paths, test_gamebanana_overrides, test_theme_paths;
and a stale cache hit in test_api_base_url). These failures exist on the `main` branch
before this task and are unrelated to storage_format or system_status changes.

## Compile Coverage (honest reporting)

**Compiled and tested on desktop:** `tests/run` — storage_format.cpp, system_status.cpp
(#else stub compiled successfully as part of the test binary).

**NOT compiled here:** The `#ifdef __SWITCH__` block in system_status.cpp (uses
`<switch.h>`, `nsGetTotalSpaceSize`, `nsGetFreeSpaceSize`, `nifmInitialize`,
`nifmGetInternetConnectionStatus`, `nifmExit` — requires devkitPro/libnx).
Also not compiled: app_header.cpp, all 13 activity files (require Borealis).
**User must compile these on Switch toolchain.**

## Deviations from Plan

None — plan executed exactly as written.

## Known Stubs

None. When running on desktop (no Switch), `query_system_status()` returns
`SystemStatus{}` (all zeros), causing bars to render empty and WiFi bars unlit.
This is the intended desktop/stub behavior per CONTEXT.md decision.

## Threat Flags

No new security-relevant surface introduced beyond what the plan's threat model
already covers (T-s2h-01 through T-s2h-04):
- `used_ratio`: clamped to [0.0f, 1.0f], total==0 guard present.
- `wifi_strength`: `std::min<u32>(strength, 3u)` clamp applied.
- All ns/nifm IPC calls wrapped in `R_SUCCEEDED()` guards; failure leaves fields at 0.
