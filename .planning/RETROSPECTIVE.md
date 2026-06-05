# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v1.0 — Hardening

**Shipped:** 2026-06-05
**Phases:** 4 | **Plans:** 16 | **Tasks:** 32 | **Commits:** ~124 (since 2026-06-04)

### What Was Built
- Community feature fully removed from API + client (routes, `@fastify/static`/`multipart`, `Post`/`Like`/`Comment` models, 8 client feed files); shared `session_codec`/`auth_store` preserved
- Live-API security hardening: auth-gated save blobs (SEC-01/TEST-01), `jti`+Postgres token revocation on logout (SEC-02), production logging with PII redaction (DEBT-04)
- C++ platform consolidation: single `fs_util` (`ensure_parent_dirs`/`copy_tree`, 7 call-sites), fail-closed TLS seam with host doctest (TEST-03), `std::atomic<bool>` cloudBusy (CONC-01)
- C++ activity refactor: `ThomazActivity`/`runAsync` base auto-capturing `alive` across all 13 activities (`brls::async` count → 0), curl cancellation on teardown across both transports (CONC-03), null-guarded `dynamic_cast` (DEBT-03), conflict-resolution doctest (TEST-04)

### What Worked
- **CONCERNS.md as a frozen backlog** — no scope-hunting; 18/18 requirements traced cleanly to 4 phases in dependency order
- **Dependency-ordered phasing** — removing the community feature first deleted the SEC-01 root cause, shrinking later phases
- **Cross-phase serialization constraints (S1/S2)** were called out in the roadmap up front (e.g. CONC-01 atomicizing `cloudBusy` before CONC-02 removed the `alive` member from the same header), avoiding merge churn
- **Host-testable seams** — extracting pure `tls_policy(bool)` and `run_if_alive` let safety-critical branches be doctested without a Switch

### What Was Inefficient
- **Roadmap status drift** — Phase 2's checkbox/progress-table row stayed stale (`[ ]` / "In Progress 1/3") after its plans completed; the authoritative plan/summary count was correct but the display lagged
- **Late code-review fixes** — a cluster of WR-0x fix commits (truncated-download guard, conflict-retry cap, brls::sync deferral guards) landed after the main implementation, suggesting some edge cases could have surfaced earlier in planning
- **Hardware UAT left entirely to the end** — 3 items deferred at close because no Switch was available during the milestone

### Patterns Established
- **`ThomazActivity` base + `runAsync(worker, onSync)`** is now the canonical async pattern for activities — the `alive` guard is captured by the base, not per-call-site
- **Pure-core seam extraction for host testing** — pull the decision logic (TLS policy, alive-drop, conflict classification) out of Borealis/curl so doctest can cover it
- **Cooperative curl cancellation** via `shared_ptr<atomic<bool>>` + `CURLOPT_XFERINFOFUNCTION` threaded from the activity's `cancelled` flag

### Key Lessons
1. Removing a feature can be the cheapest security fix — deleting the community `@fastify/static` path closed a HIGH-severity exposure with zero new code
2. Make unforgettable-by-construction guards (base-class `runAsync`) instead of relying on every call-site remembering the `alive` capture
3. When a class of fixes can only be validated on hardware, schedule a hardware UAT pass as an explicit milestone item rather than discovering the gap at close
4. Keep ROADMAP.md checkboxes/progress table in sync with plan completion — trust the plan/summary count, but the human-readable display should match

### Cost Observations
- Model mix: profile `balanced` (mode: yolo) — not separately instrumented this milestone
- Notable: ~124 commits over ~2 days; 36 feat + 22 fix commits indicates a meaningful post-implementation fix tail worth front-loading next time

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | — | 4 | First milestone — established GSD phase/plan/verify cadence and host-testable-seam discipline |

### Cumulative Quality

| Milestone | Tests | Coverage | Zero-Dep Additions |
|-----------|-------|----------|-------------------|
| v1.0 | 175+ doctest (C++) + 14 Vitest (API) | not measured | No new heavy deps (Postgres denylist over Redis) |

### Top Lessons (Verified Across Milestones)

1. _(pending second milestone to cross-validate)_
