# Phase 4: C++ Activity Hardening - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-05
**Phase:** 4-c-activity-hardening
**Areas discussed:** runAsync migration breadth, CONC-02/CONC-03 unification, curl cancellation scope & UX, TEST-04 coverage depth

---

## runAsync migration breadth (CONC-02)

| Option | Description | Selected |
|--------|-------------|----------|
| All 13 call-sites | Migrate every direct brls::async usage across all activities to ThomazActivity::runAsync (matches Phase 3 D-04, guard impossible to forget app-wide) | ✓ |
| Only the 3 CONC-02-named | Migrate just save_detail, mod_browser, theme_browser (requirement's literal set) | |

**User's choice:** All 13 call-sites
**Notes:** Consistent with the Phase 3 D-04 "all call-sites" precedent; larger diff accepted for complete hardening. → CONTEXT D-01.

---

## CONC-02 + CONC-03 unification

| Option | Description | Selected |
|--------|-------------|----------|
| Unify in base class | ThomazActivity owns both alive guard and cancelled flag; runAsync wires both; curl XFERINFOFUNCTION checks the base-class flag | ✓ |
| Keep cancellation separate | runAsync handles only alive; cancelled flag wired per network call site independently | |

**User's choice:** Unify in base class
**Notes:** One coherent ownership model; CONC-03 builds on CONC-02 per roadmap sequencing. → CONTEXT D-02.

---

## Curl cancellation scope & UX (CONC-03)

| Option | Description | Selected |
|--------|-------------|----------|
| All network ops, silent drop | Wire cancelled-check into mods + cloud saves + themes; abort silently on teardown; happy-path unaffected | ✓ |
| Only mod_download | Add the check only where XFERINFOFUNCTION already exists | |
| All ops + visible 'cancelled' state | Same broad scope but surface a cancellation toast/state | |

**User's choice:** All network ops, silent drop
**Notes:** No screen exists at teardown, so silent drop is correct; broadest coverage. → CONTEXT D-03 / D-04.

---

## TEST-04 coverage depth

| Option | Description | Selected |
|--------|-------------|----------|
| Both | Conflict/plan_push branch doctest AND runAsync dropped-callback unit test | ✓ |
| Conflict/plan_push branch only | Cover just the named cloud-save conflict path | |

**User's choice:** Both
**Notes:** Regression-guards existing conflict logic and the new concurrency machinery. → CONTEXT D-05.

---

## Claude's Discretion

- Exact `ThomazActivity` API / `runAsync(worker, onSync)` signature and how `alive` + `cancelled` are captured and exposed to the curl layer.
- Mechanics of bridging the base-class `cancelled` flag into each platform-layer curl call site.
- `dynamic_cast` null-handling form (log message, early-return placement) — safe-no-op contract fixed, form open.
- Ordering of DEBT-03 cast edits vs CONC-02 edits within the four shared activity files.

## Deferred Ideas

None — discussion stayed within phase scope.
