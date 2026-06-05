# Phase 3: Theme UI Integration - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-05
**Phase:** 3-theme-ui-integration
**Areas discussed:** Action placement, Status & re-extract UX, Progress & result UI, Firmware version role

---

## Action placement

| Option | Description | Selected |
|--------|-------------|----------|
| A) Settings activity | Row alongside other system/setup actions (sysmodules-like); out of the theme flow | |
| B) Theme browser screen | Status row/card at top of the theme list + action; in-context where users theme the console | ✓ |
| C) Dedicated "Base layouts" screen | Its own managed page with richer per-title status | |

**User's choice:** B (theme browser screen), keeping the existing `base_missing` dialog as the reactive shortcut.
**Notes:** Accepted recommendation. Browser action = persistent/discoverable entry; base_missing dialog = just-in-time entry. Both invoke the same extraction path. → D-01 / D-01a.

---

## Status & re-extract UX

| Option | Description | Selected |
|--------|-------------|----------|
| Headline all-or-nothing status | Driven by `base_present_for()`; per-title detail only after a failed run | ✓ |
| Per-title resting status | Show each title's state in the resting view | |
| Manual re-extract + confirm dialog | Overwrite-warning confirm before re-running (low risk: in-place overwrite, best-effort) | ✓ |
| Silent re-extract | Fire immediately, no confirm | |

**User's choice:** Headline status + manual "Reextrair" button gated by a lightweight overwrite-confirm dialog.
**Notes:** Accepted recommendation. → D-02 / D-03.

---

## Progress & result UI

| Option | Description | Selected |
|--------|-------------|----------|
| Blocking spinner dialog (no fake progress) | Non-dismissable "Extraindo…" over the existing `brls::async`; engine has no progress callbacks | ✓ |
| Granular per-title progress | Would require an engine-interface change (callback) | |
| Toast only (current) | Ephemeral notify; too short-lived to read a failure reason | |
| Results dialog | States success, or NAMES the systemic reason, or shows partial "X escritos, Y falharam" + failed parts | ✓ |

**User's choice:** Blocking spinner during the run + a results dialog on completion (replaces the current toast).
**Notes:** Accepted recommendation. Results dialog is what makes INTEG-05's "name the reason" durable. → D-04 / D-05.

---

## Firmware version role

| Option | Description | Selected |
|--------|-------------|----------|
| Store + show | Persist fw version at extraction; show it in the status line | ✓ |
| Store only | Internal state, not displayed | |
| Passive mismatch hint | Display-only advisory when current fw ≠ recorded fw; no forced modal | ✓ |
| Proactive auto-prompt | Auto-detect update and modally nudge re-extraction | (deferred — future req) |

**User's choice:** Store + show the recorded fw version; passive display-only mismatch hint; re-extract stays manual.
**Notes:** Accepted recommendation. Proactive auto-detect-and-prompt is explicitly a deferred/future requirement and out of Phase 3 scope. → D-06 / D-07.

---

## Claude's Discretion

- Persistence format/location of the recorded fw-version marker (⚠ avoid colliding with the firmware's own `ver.cfg`; use a thomaz-namespaced marker).
- Whether resting "extracted" status reads live off `base_present_for()`, off the persisted marker, or both.
- Exact Borealis widgets and mount point in `theme_browser_activity`.
- Whether to add a progress callback to the Phase 2 engine for truthful per-title progress — flagged as a cross-phase trade-off requiring user opt-in, not assumed.

## Deferred Ideas

- Proactive firmware-update detection + auto-prompt to re-extract (future requirement).
- Forwarder / Application-mode launcher (Phase 4 / TAKEOVER-03).
- Surfacing/installing the qlaunch IPS patch from the extraction action (separate nicety; IPS already wired at apply time).
- Per-title progress UI (needs engine-interface change).
