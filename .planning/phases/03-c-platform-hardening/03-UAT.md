---
status: testing
phase: 03-c-platform-hardening
source: [03-VERIFICATION.md]
started: 2026-06-05T16:06:44Z
updated: 2026-06-05T16:06:44Z
---

## Current Test

number: 1
name: TLS warning banner renders on every screen when the insecure latch is set
expected: |
  A high-contrast red warning Label appears in the AppletFrame header on every
  screen while thomaz::tls_insecure_flag() is true. No banner appears on any
  screen when the flag is false (normal desktop run).
awaiting: user response

## Tests

### 1. TLS warning banner visual rendering (forced-flag smoke)
expected: |
  Force `thomaz::tls_insecure_flag()=true` at startup (e.g. add the line in
  main.cpp or at the top of tls_banner.cpp's install function before the
  Application loop), build with
  `cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_desktop && cmake --build build_desktop`,
  run the desktop binary, and navigate to all 13 screens (Home, Game List,
  Cheats, Mods, Settings, Save Manager, System, Themes, Mod Detail, Cheat
  Detail, Theme Detail, Clear Cheats, Mod Manager). A high-contrast red warning
  Label must appear in the AppletFrame header on every screen while the flag is
  true. Then revert the forced flag and confirm NO banner appears on any screen
  during a normal desktop run.
result: [pending]
why_human: |
  The tls_insecure latch is never set on desktop (the host always verifies the
  CA bundle), so the banner cannot trigger naturally on the host. Visual
  rendering of a live brls::Label requires running the app. The code wiring is
  verified correct; only on-screen visual confirmation remains. Deferred by
  explicit user decision at the 03-03 checkpoint.

## Summary

total: 1
passed: 0
issues: 0
pending: 1
skipped: 0
blocked: 0

## Gaps
