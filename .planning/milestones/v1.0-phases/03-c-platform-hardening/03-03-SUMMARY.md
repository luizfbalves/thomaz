---
phase: 03-c-platform-hardening
plan: 03
subsystem: ui
tags: [borealis, tls, security, i18n, cpp, banner]

# Dependency graph
requires:
  - phase: 03-c-platform-hardening/03-02
    provides: tls_is_insecure() accessor and tls_insecure_flag process-global latch (curl_tls.hpp)
provides:
  - install_tls_warning_banner(brls::Activity*) helper (source/app/tls_banner.hpp + .cpp)
  - Persistent red warning Label in AppletFrame header on all 13 activities when tls_is_insecure() is true
  - thomaz/tls/insecure_warning i18n key in 5 locales (en-US, fr, pt-BR, ru, zh-Hans)
affects:
  - 03-04 (save_detail_activity.cpp banner already wired; Plan 04 must only touch cloudBusy / .hpp)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "install_tls_warning_banner mirrors install_header_username: include in activity .cpp, call immediately after username helper in onContentAvailable"
    - "TLS banner is process-global additive: gated on tls_is_insecure(), no-op on desktop, persistent on Switch when CA probe fails"

key-files:
  created:
    - source/app/tls_banner.hpp
    - source/app/tls_banner.cpp
  modified:
    - source/app/home_activity.cpp
    - source/app/cheat_detail_activity.cpp
    - source/app/mod_browser_activity.cpp
    - source/app/settings_activity.cpp
    - source/app/game_list_activity.cpp
    - source/app/clear_cheats_activity.cpp
    - source/app/theme_browser_activity.cpp
    - source/app/mod_detail_activity.cpp
    - source/app/system_activity.cpp
    - source/app/theme_detail_activity.cpp
    - source/app/save_manager_activity.cpp
    - source/app/mod_manager_activity.cpp
    - source/app/save_detail_activity.cpp
    - resources/i18n/en-US/thomaz.json
    - resources/i18n/fr/thomaz.json
    - resources/i18n/pt-BR/thomaz.json
    - resources/i18n/ru/thomaz.json
    - resources/i18n/zh-Hans/thomaz.json

key-decisions:
  - "Banner inserted at index 0 in hint_box so warning precedes username label; brls::Box::addView(View*, size_t) overload confirmed present"
  - "Red color 0xFF5555 chosen to be high-contrast and distinct from username purple 0x9277FF (D-02 requirement)"
  - "save_detail_activity.hpp intentionally untouched — Plan 04 boundary preserved (write-conflict prevention)"

patterns-established:
  - "Paired-call pattern: every activity that calls install_header_username(this) must also call install_tls_warning_banner(this) immediately after (D-02a)"

requirements-completed: [SEC-03]

# Metrics
duration: ~15min
completed: 2026-06-05
---

# Phase 03 Plan 03: TLS Warning Banner (SEC-03) Summary

**Persistent red warning Label injected into every activity's AppletFrame header via shared install_tls_warning_banner helper gated on the tls_is_insecure() latch, with translated warning strings in 5 locales**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-06-05T15:40:00Z
- **Completed:** 2026-06-05
- **Tasks:** 3 of 3 (Task 3 build portion verified; visual smoke deferred to phase UAT)
- **Files modified:** 20

## Accomplishments
- Created tls_banner.hpp + tls_banner.cpp mirroring install_header_username precedent: null-guards activity and hint_box, early return if tls_is_insecure() is false (desktop no-op), inserts red Label at index 0 in brls/applet_frame/hint_box
- Added thomaz/tls/insecure_warning i18n key in all 5 locales: en-US (English), fr (French), pt-BR (Portuguese), ru (Russian), zh-Hans (Simplified Chinese)
- Wired install_tls_warning_banner(this) into all 13 activity .cpp files immediately after install_header_username(this) in onContentAvailable; banner count == username count
- save_detail_activity.hpp untouched as required by Plan 04 boundary

## Task Commits

Each task was committed atomically:

1. **Task 1: Create tls_banner helper + i18n key (5 locales)** - `ceb3949` (feat)
2. **Task 2: Wire install_tls_warning_banner into all 14 activities** - `191f5f6` (feat)
3. **Task 3: Verify the persistent banner renders on every screen** - Build verified clean (exit 0, zero warnings); visual smoke deferred to phase UAT (see Human Verification section)

## Files Created/Modified
- `source/app/tls_banner.hpp` - Declares void thomaz::install_tls_warning_banner(brls::Activity*)
- `source/app/tls_banner.cpp` - Definition: gated on tls_is_insecure(), injects red Label at hint_box[0]
- `source/app/home_activity.cpp` - Banner wiring
- `source/app/cheat_detail_activity.cpp` - Banner wiring
- `source/app/mod_browser_activity.cpp` - Banner wiring
- `source/app/settings_activity.cpp` - Banner wiring
- `source/app/game_list_activity.cpp` - Banner wiring
- `source/app/clear_cheats_activity.cpp` - Banner wiring
- `source/app/theme_browser_activity.cpp` - Banner wiring
- `source/app/mod_detail_activity.cpp` - Banner wiring
- `source/app/system_activity.cpp` - Banner wiring
- `source/app/theme_detail_activity.cpp` - Banner wiring
- `source/app/save_manager_activity.cpp` - Banner wiring
- `source/app/mod_manager_activity.cpp` - Banner wiring
- `source/app/save_detail_activity.cpp` - Banner wiring (.hpp NOT touched)
- `resources/i18n/en-US/thomaz.json` - Added thomaz.tls.insecure_warning
- `resources/i18n/fr/thomaz.json` - Added thomaz.tls.insecure_warning (French)
- `resources/i18n/pt-BR/thomaz.json` - Added thomaz.tls.insecure_warning (Portuguese)
- `resources/i18n/ru/thomaz.json` - Added thomaz.tls.insecure_warning (Russian)
- `resources/i18n/zh-Hans/thomaz.json` - Added thomaz.tls.insecure_warning (Chinese Simplified)

## Decisions Made
- Banner inserted at index 0 in hint_box (brls::Box::addView(View*, size_t) overload confirmed in borealis/core/box.hpp:95) so warning is the first thing rendered, preceding the username label
- Red 0xFF5555 is high-contrast and visually distinct from username purple 0x9277FF; satisfies D-01/D-02 "visible warning" requirement
- save_detail_activity.hpp boundary preserved for Plan 04 to avoid write conflict

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
None.

## Known Stubs
None — banner renders live i18n string, gated on real process-global flag from Plan 02.

## Threat Flags
None — this plan IS the mitigation for T-03-05 (silent TLS downgrade now surfaced to user). No new unmodeled threat surface introduced.

## Human Verification / Deferred Verification

### human_verification: Visual forced-flag TLS banner smoke test

**Status:** DEFERRED to phase UAT

**Item:** Force `thomaz::tls_insecure_flag()` = true at startup, run the desktop build, and confirm:
1. A red warning Label renders in the AppletFrame header on every activity (verifying D-02 persistence across all 13 screens).
2. The banner is absent when the flag is false (normal desktop run).

**Build already verified:** The clean desktop build (`cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` + full build+link) completed with exit 0 and zero warnings from `tls_banner.cpp` or any of the 13 activity files. Only the on-screen visual confirmation remains.

**Why deferred:** The latch is never set on desktop (always verifies CA) so the banner cannot trigger naturally on host; forced-flag visual confirmation requires a desktop run with a patched startup. User explicitly chose to defer to phase-level UAT.

**How to verify (phase UAT):**
1. In `source/app/tls_banner.cpp` (or `main.cpp`), temporarily add `thomaz::set_tls_insecure_for_testing(true)` at startup before the Application loop.
2. Build: `cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_desktop && cmake --build build_desktop`
3. Run `./build_desktop/thomaz` — navigate to every screen (Home, Game List, Cheats, Mods, Settings, Save Manager, System, Themes…) and confirm the red warning Label appears in the header on each.
4. Revert the forced flag and rebuild; confirm the red banner is absent on all screens.

## Next Phase Readiness
- Plan 03 is COMPLETE (all code committed; Task 3 build verified clean; visual smoke deferred to phase UAT above)
- Plan 04 (save_detail cloudBusy + .hpp) can proceed; save_detail_activity.cpp banner wiring is already committed and Plan 04 boundary (no .hpp touch) was preserved
- T-03-06 (new activity forgetting banner call) mitigated by paired-call verify gate: banner count == username count

---
*Phase: 03-c-platform-hardening*
*Completed: 2026-06-05*

## Self-Check: PASSED
All 3 tasks complete (Task 3 build verified; visual smoke deferred).
- ceb3949 confirmed: feat(03-03): add tls_banner helper + i18n key (5 locales)
- 191f5f6 confirmed: feat(03-03): wire install_tls_warning_banner into all 13 activities
- source/app/tls_banner.hpp: FOUND
- source/app/tls_banner.cpp: FOUND
