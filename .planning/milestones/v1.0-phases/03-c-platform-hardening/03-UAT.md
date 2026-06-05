---
status: testing
phase: 03-c-platform-hardening
source: [03-VERIFICATION.md]
started: 2026-06-05T16:06:44Z
updated: 2026-06-05T18:10:00Z
note: |
  Re-verified after the CR-01 fail-open→fail-closed reversal (CONTEXT D-06a).
  Phase goal is verified 5/5. Both remaining items below are NON-GATING
  (gating: false in 03-VERIFICATION.md) — they do not block phase completion
  or Phase 4. They are optional integration checks.
---

## Current Test

[testing complete — phase goal verified 5/5; remaining items are optional/non-gating]

## Tests

### 1. TLS warning banner visual rendering (forced-latch smoke) — NON-GATING
expected: |
  Force the latch (`thomaz::tls_insecure_flag().store(true);` after Application::init(),
  or temporarily remove the `if (!thomaz::tls_is_insecure()) return;` guard in tls_banner.cpp),
  build `cmake -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -S . -B build_smoke && cmake --build build_smoke`,
  run and navigate all 13 screens. A high-contrast red (0xFF5555) warning Label from the
  `thomaz/tls/insecure_warning` key renders in the AppletFrame header on every screen while the
  latch is forced; no banner when the latch is false.
result: deferred
gating: false
reason: |
  Under the revised fail-closed design (D-06a), the tls_insecure latch is never set
  automatically on any platform — the banner is latent infrastructure for a future
  TlsMode::InsecureAllowed caller. SEC-03's intent is now met by refusing the insecure
  transfer, not by warning-and-continuing. Optional render check only.

### 2. Switch-toolchain build for IN-03 refactor — NON-GATING
expected: |
  Build the Switch NRO target (devkitPro aarch64). save_service_switch.cpp (uid_from_hex
  with SCNx64/PRIx64 inside #ifdef __SWITCH__) compiles with zero errors/warnings, and
  uid_from_hex round-trips an AccountUid (format then parse returns the original).
result: deferred
gating: false
reason: |
  save_service_switch.cpp is #ifdef __SWITCH__-gated and not compiled by the host g++ suite.
  The IN-03 refactor is behavior-preserving but needs a devkitPro toolchain to exercise.
  Tracked with the existing hardware checklist for this milestone.

## Summary

total: 2
passed: 0
issues: 0
pending: 0
skipped: 0
blocked: 0
deferred_non_gating: 2

## Gaps

[none — phase goal verified 5/5; both open items are non-gating optional checks]
