# Phase 2: Full Extraction Engine - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-05
**Phase:** 2-full-extraction-engine
**Areas discussed:** Target set, Failure policy, Re-extraction/overwrite, Output validation

---

## Target set

| Option | Description | Selected |
|--------|-------------|----------|
| All `/lyt/*.szs` | Extract everything in each title's `/lyt/`; covers the 8 canonical + future targets; marginal cost | ✓ |
| Only the 8 canonical | Strict filter; leaner but any new theme target needs code changes; `common` currently missing | |

**User's choice:** All `/lyt/*.szs` (recommended)
**Notes:** Guarantees `common` and the canonical set are never missed; future-proof for uncommon theme targets.

---

## Failure policy

| Option | Description | Selected |
|--------|-------------|----------|
| Best-effort + report | Extract what succeeds, collect+report failures; systemic (key/BIS) failures still hard-abort | ✓ |
| All-or-nothing | Any single failure aborts the whole run | |

**User's choice:** Best-effort + report (recommended)
**Notes:** Missing part/title → warn and continue; key-derivation / BIS-mount failure → abort (continuing is pointless).

---

## Re-extraction / overwrite

| Option | Description | Selected |
|--------|-------------|----------|
| Overwrite each file in place | Idempotent; correct after firmware update; no stale files, no dangerous partial state | ✓ |
| Skip existing | Faster but leaves stale szs after a firmware update | |
| Wipe then re-extract | Cleanest, but a mid-run failure leaves no base at all | |

**User's choice:** Overwrite each file in place (recommended)
**Notes:** —

---

## Output validation

| Option | Description | Selected |
|--------|-------------|----------|
| Validate Yaz0+SARC | Confirm each szs decompresses (Yaz0) and opens as valid SARC before accepting | ✓ |
| Non-empty file only | Accept any byte count > 0; a corrupt szs would pass and only fail at apply | |

**User's choice:** Validate Yaz0+SARC (recommended)
**Notes:** Same validation built host-side this session; reuse linked `lib/switchthemes` Yaz0/SARC; keep mapping/path logic host-testable.

---

## Claude's Discretion

- Loop structure (mount BIS once vs per-title), NCA resolution mechanics, pipeline placement of validation.
- Whether "all `/lyt/*.szs`" is a directory glob or an enumerated set.

## Deferred Ideas

- Firmware-version recording (`ver.cfg`) + re-extract UX → Phase 3.
- "Extrair layouts" UI action, progress, messaging → Phase 3.
- qlaunch IPS install from extraction/system action → already at apply time; Phase 3 nicety.
