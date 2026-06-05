# Phase 3: C++ Platform Hardening - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-05
**Phase:** 3-C++ Platform Hardening
**Areas discussed:** TLS warning surface, TLS banner placement, fs_util consolidation scope, TEST-03 test seam

---

## TLS fail-safe warning surface (SEC-03)

| Option | Description | Selected |
|--------|-------------|----------|
| Persistent on-screen banner | `notify` recurring OR fixed header/label while insecure state lasts; literal "persistent on-screen" | ✓ |
| Single toast + Logger::warning | `notify()` once + log; on-screen but transient | |
| Logger::warning only | Log only, not on screen; cheapest but does NOT satisfy SEC-03 "on-screen" | |

**User's choice:** Persistent on-screen banner
**Notes:** User wanted maximum visibility; rejected log-only and transient toast.

---

## TLS banner placement

| Option | Description | Selected |
|--------|-------------|----------|
| Global at top of app | Fixed banner in main `brls::Application` shell, visible on all screens while insecure | ✓ |
| One-time modal dialog at boot | `brls::Dialog` dismissed at startup; "persistent" = blocks until acknowledged | |
| Banner only on network screens | Fixed warning atop HTTPS activities (saves/mods/themes); contextual | |

**User's choice:** Global at top of app
**Notes:** Maximum visibility across all screens; accepts touching root UI setup.

---

## fs_util consolidation scope (DEBT-01/DEBT-02)

| Option | Description | Selected |
|--------|-------------|----------|
| Consolidate ALL call-sites | All 4 `ensure_parent_dirs` + 3 `copy_tree` copies → `fs_util`; strict "exactly one" | ✓ |
| Only roadmap-flagged | Minimal pair per requirement; smaller diff | |

**User's choice:** Consolidate ALL call-sites
**Notes:** Larger diff accepted for complete hardening.

---

## TEST-03 test seam

| Option | Description | Selected |
|--------|-------------|----------|
| Extract pure policy function | `tls_policy(bool ca_present)` host-compilable + testable; `apply_curl_tls` consumes it | ✓ |
| Planner decides seam | Defer structure to planner/researcher from existing doctest harness | |

**User's choice:** Extract pure policy function
**Notes:** Clean seam tests real decision logic, not a mock; fixes the `#ifdef __SWITCH__` host-testability gap.

---

## Claude's Discretion

- Exact `fs_util` namespace/signatures and header layout.
- How the global banner view wires into the `brls::Application` shell.
- `cloudBusy` atomic mechanics (`load`/`store` vs `compare_exchange_strong`) — preserve current semantics.

## Deferred Ideas

None — discussion stayed within phase scope.
