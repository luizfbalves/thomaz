---
phase: quick-260605-qgb
plan: "01"
subsystem: app/boot
tags: [bootscreen, navigation, i18n, ux]
dependency_graph:
  requires: []
  provides: [BootActivity, boot.xml, thomaz/boot/login, thomaz/boot/guest]
  affects: [main.cpp, resources/i18n]
tech_stack:
  added: []
  patterns: [pop-then-push navigation, ThomazActivity base class, CONTENT_FROM_XML_RES]
key_files:
  created:
    - source/app/boot_activity.hpp
    - source/app/boot_activity.cpp
    - resources/xml/activity/boot.xml
  modified:
    - source/main.cpp
    - resources/i18n/en-US/thomaz.json
    - resources/i18n/pt-BR/thomaz.json
    - resources/i18n/fr/thomaz.json
    - resources/i18n/ru/thomaz.json
    - resources/i18n/zh-Hans/thomaz.json
    - .gitignore
decisions:
  - "pop-then-push for goHome(): popActivity(NONE, callback) → pushActivity(HomeActivity) so BootActivity does not remain in the navigation stack under HomeActivity"
  - "boot i18n keys added at top level of thomaz.json (no wrapper); Borealis maps filename to namespace, so @i18n/thomaz/boot/login → d['boot']['login']"
  - "boot.xml gitignore exception added (.gitignore had /resources/** blanket rule requiring explicit !exceptions for tracked XML files)"
metrics:
  duration_seconds: 250
  completed_date: "2026-06-05"
  tasks_completed: 3
  files_changed: 9
---

# Quick Task 260605-qgb: Boot Screen Summary

**One-liner:** BootActivity (black background, icon left, login/guest buttons right) inserted before HomeActivity when no saved session exists, wired into main.cpp conditionally on `restoredSession.has_value()`.

## Tasks Completed

| # | Task | Commit | Files |
|---|------|--------|-------|
| 1 | Create BootActivity (header, implementation, XML layout) | fc5c733 | boot_activity.hpp, boot_activity.cpp, boot.xml, .gitignore |
| 2 | Add i18n keys for boot screen in all five locales | bd996d7 | en-US, pt-BR, fr, ru, zh-Hans thomaz.json |
| 3 | Wire BootActivity into main.cpp (conditional on missing session) | d668063 | source/main.cpp |

## What Was Built

**BootActivity** (`source/app/boot_activity.hpp` + `.cpp`):
- Extends `ThomazActivity` (lifetime guards, `runAsync` support)
- Carries all five service pointers: `ITitleService`, `IHttpClient`, `ISaveService`, `IAuthClient`, `ICloudSaveClient`
- `onContentAvailable()`: wires `loginBtn` (push AuthActivity, onAuthed calls `goHome()`), `guestBtn` (calls `goHome()` directly), sets initial focus on `loginBtn`
- `goHome()`: uses pop-then-push pattern — `popActivity(NONE, lambda)` then `pushActivity(new HomeActivity(...))` so BootActivity does not linger under HomeActivity in the stack

**boot.xml** (`resources/xml/activity/boot.xml`):
- `brls:AppletFrame` with `backgroundColor="#000000"`, empty title
- Two-column row layout: left column has `brls:Image` (icon.png, 180×180, centered), right column has two `AnimatedBox` buttons
- loginBtn: `tile_cheats` background, `entranceDelay="80"`, label `@i18n/thomaz/boot/login`
- guestBtn: `surface_1` background with `line` border, `entranceDelay="180"`, label `@i18n/thomaz/boot/guest`

**i18n** (five locales, top-level `"boot"` key in each `thomaz.json`):
- en-US: "Log in" / "Continue without login"
- pt-BR: "Entrar" / "Continuar sem login"
- fr: "Se connecter" / "Continuer sans connexion"
- ru: "Войти" / "Продолжить без входа"
- zh-Hans: "登录" / "不登录继续"

**main.cpp** conditional push:
- `restoredSession.has_value()` → push HomeActivity (unchanged behavior for returning users)
- `!restoredSession.has_value()` → push BootActivity (new first-run / logged-out flow)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] boot.xml gitignore exception missing**
- **Found during:** Task 1 commit preparation
- **Issue:** `.gitignore` has a blanket `/resources/**` rule. Every tracked XML file requires an explicit `!/resources/xml/activity/<file>.xml` exception. `boot.xml` was silently ignored and would not be committed.
- **Fix:** Added `!/resources/xml/activity/boot.xml` to `.gitignore` alongside the existing XML exception lines.
- **Files modified:** `.gitignore`
- **Commit:** fc5c733

**2. [Observation] Plan verification script assumes wrong JSON structure**
- **Found during:** Task 2 (pre-edit inspection)
- **Issue:** The plan's verification script checks `d['thomaz']['boot']`, implying a top-level `"thomaz"` wrapper in `thomaz.json`. The actual files have no such wrapper — `"auth"`, `"home"`, etc. are all top-level keys. Borealis i18n uses the *filename* as the namespace, not a wrapper key.
- **Fix:** Added `"boot"` as a top-level sibling of `"auth"` (and `"saves"`, `"tls"` for partial locales). The Borealis path `@i18n/thomaz/boot/login` correctly resolves to `d['boot']['login']` in `thomaz.json`.
- **Adapted verification:** Custom verification confirmed all five files parse as valid JSON and contain `boot.login` + `boot.guest` at the correct level.
- **No files changed** beyond what the task required; the plan's verification command would fail if run as-is but the runtime behavior is correct.

## Verification Results

- Desktop build (`cmake -DUSE_SDL2=ON` + `cmake --build --target thomaz`): **PASSED** — no errors, no warnings
- `boot_activity.cpp` compiled: **PASSED** (confirmed in build output)
- `main.cpp` compiled: **PASSED**
- JSON validity + key presence (all 5 locales): **PASSED**
- Hardware run (Switch): out of scope per constraints

## Known Stubs

None. BootActivity is fully wired: loginBtn → AuthActivity, guestBtn → HomeActivity, and main.cpp conditional is in place.

## Threat Flags

No new threat surface beyond what the plan's threat model covers. BootActivity introduces no new network endpoints, no new auth paths beyond the existing AuthActivity flow, and no new file access patterns.

## Self-Check: PASSED

- [x] source/app/boot_activity.hpp exists
- [x] source/app/boot_activity.cpp exists
- [x] resources/xml/activity/boot.xml exists
- [x] source/main.cpp modified (conditional push)
- [x] All 5 i18n files contain boot.login + boot.guest
- [x] Commit fc5c733 exists (Task 1)
- [x] Commit bd996d7 exists (Task 2)
- [x] Commit d668063 exists (Task 3)
- [x] Desktop build: thomaz target built successfully with no errors
