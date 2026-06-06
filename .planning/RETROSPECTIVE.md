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

## Milestone: v1.1 — Switch-Only Simplification

**Shipped:** 2026-06-06
**Phases:** 3 | **Plans:** 8 | **Tasks:** 7

### What Was Built
- Deleted the 5 desktop platform stub pairs and collapsed every `*_fake`-vs-`*_switch` implementation-selection seam in `main.cpp`/`home_activity.cpp` so the `*_switch` impls are sole (SIMPL-01..03)
- Stripped both `PLATFORM_DESKTOP` branches (link + packaging) and dual-target comments from `CMakeLists.txt`; deleted `build-desktop.sh`/`run-desktop.sh` (BUILD-01/02)
- Clean Switch build from the stripped tree produces `build_switch/thomaz.nro` (7.7 MB), launched on real hardware via nxlink (BUILD-03)
- README rewritten Switch-only; documented the two single-target verification gates (DOC-01); both gates confirmed green together — host doctest 209/209 + Switch build (VERIF-01)

### What Worked
- **Dependency-ordered removal (source → build → docs)** kept the tree buildable at every phase boundary — no phase left it broken
- **Scope correction mid-flight (Option D)** — recognizing that `#else` portability seams (path strings, `_WIN32`) are NOT desktop stub-selection prevented over-deletion that would have broken the host doctest suite
- **Host doctest as a target-independent gate** survived the desktop removal untouched and caught that the retained `fake_cloud_save_client` double still compiles
- **Native devkitPro fallback** when Docker was unavailable — built `.nro` via the MSYS2 login shell, recorded both build paths in project memory for reuse

### What Was Inefficient
- **REQUIREMENTS.md checkboxes never ticked** for BUILD-*/DOC-01/VERIF-01 despite the work being done — the SUMMARYs were authoritative but the traceability display lagged (same drift pattern flagged in v1.0)
- **Environment friction on Windows** — msys2 cmake not inheriting env vars, login shell starting in `$HOME`, missing host g++ and Switch portlibs each cost a debugging cycle before the build path was stable
- **Quick tasks accumulated without SUMMARY/status markers** — 5 orthogonal UI quick-tasks surfaced as "open" at milestone close, needing acknowledgment rather than being cleanly closed inline

### Patterns Established
- **Collapse selection seams before removing the build branch** — make the kept implementation sole at the source layer first, so the build can drop the alternative without an unbuildable window
- **Distinguish stub-selection seams from portability seams** — only the former are removed in a single-target simplification; the latter keep host tests compilable
- **Record machine-specific build recipes in project memory** (native devkitPro build, host doctest flags) so environment setup isn't re-derived each session

### Key Lessons
1. A "remove a target" milestone is really a "make the kept path sole" milestone — sequence so the tree never goes unbuildable
2. Not every `#else` is the thing you're deleting — classify seams (stub-selection vs portability) before bulk-collapsing
3. Tick requirement checkboxes at plan completion, not at milestone close — the drift repeated from v1.0
4. Close or mark quick tasks (SUMMARY/status) as they finish, so milestone-close audits stay clean

### Cost Observations
- Model mix: profile `balanced` (mode: yolo) — not separately instrumented
- Sessions: spanned a context compaction; build/debug iteration dominated wall-clock over planning
- Notable: verification-only phase (07-02) changed 0 source files — pure gate confirmation, cheap

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v1.0 | — | 4 | First milestone — established GSD phase/plan/verify cadence and host-testable-seam discipline |
| v1.1 | — | 3 | Single-target simplification — dependency-ordered removal kept the tree buildable; native devkitPro build path established |

### Cumulative Quality

| Milestone | Tests | Coverage | Zero-Dep Additions |
|-----------|-------|----------|-------------------|
| v1.0 | 175+ doctest (C++) + 14 Vitest (API) | not measured | No new heavy deps (Postgres denylist over Redis) |
| v1.1 | 209 doctest (C++) | not measured | None — removal-only milestone |

### Top Lessons (Verified Across Milestones)

1. **Roadmap/requirements display drift is recurring** — both v1.0 and v1.1 left checkboxes/progress rows stale after plans completed; the plan/summary count is authoritative but the human-readable display must be kept in sync at plan completion
2. **Host-testable seams pay off repeatedly** — pulling decision logic out of Borealis/curl (v1.0) made the host doctest suite a target-independent gate that survived the v1.1 desktop removal untouched
