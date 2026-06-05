# Phase 4: C++ Activity Hardening - Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Resolve the C++ activity-layer robustness issues from CONCERNS.md without adding features:
1. **CONC-02** — introduce a `ThomazActivity` base class with a `runAsync(worker, onSync)` wrapper that auto-captures the `alive` guard, so the use-after-free guard cannot be forgotten.
2. **CONC-03** — abort in-flight curl requests on activity destruction via a shared `cancelled` flag checked in a `CURLOPT_XFERINFOFUNCTION` callback; happy-path transfers unaffected.
3. **DEBT-03** — replace C-style view casts in the flagged activities (`game_list`, `save_manager`, `save_detail`, `mod_browser`) with null-guarded `dynamic_cast`; a wrong-type/mistyped view id fails safely (log + return) instead of crashing later.
4. **TEST-04** — host doctest coverage for the cloud-save conflict-resolution / `plan_push` branch AND the `runAsync` dropped-callback (not-alive) semantics.

Verified by a clean desktop build (`-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON`, zero new warnings) and the host doctest suite under `tests/`.

**Not in scope:** further API/server work (Phases 1-2, done); the Phase 3 platform fixes (fs_util, TLS, cloudBusy — done). No new user-facing features — this is an internal robustness refactor with no visible surface.
</domain>

<decisions>
## Implementation Decisions

### runAsync base class & migration breadth (CONC-02)
- **D-01:** Migrate **ALL** direct `brls::async` activity call-sites to `ThomazActivity::runAsync`, not just the three named in CONC-02 (`save_detail`, `mod_browser`, `theme_browser`). `brls::async` currently appears in **13** activity files (`save_detail_activity.hpp` + `.cpp`, `save_manager_activity.cpp`, `mod_detail_activity.cpp`, `clear_cheats_activity.cpp`, `mod_manager_activity.cpp`, `auth_activity.cpp`, `theme_browser_activity.cpp`, `mod_browser_activity.cpp`, `game_list_activity.cpp`, `settings_activity.cpp`, `theme_detail_activity.cpp`, `cheat_detail_activity.cpp`). Migrating all makes the alive-guard genuinely impossible to forget app-wide. Mirrors the Phase 3 D-04 "all call-sites" precedent (larger diff accepted for complete hardening).
- **D-01a (S2, satisfied):** `runAsync` lives on a new `source/app/thomaz_activity.hpp` `ThomazActivity` base class (inherits `brls::Activity`). It owns the `alive` guard, so CONC-02 **removes the per-activity `alive` member** (e.g. `save_detail_activity.hpp:60`). Phase 3's CONC-01 already atomicized `cloudBusy` in that same header, so editing it is safe now.

### CONC-02 + CONC-03 unification
- **D-02:** **Unify both lifetime mechanisms in the base class.** `ThomazActivity` owns BOTH the `alive` guard (CONC-02) and the `cancelled` flag (CONC-03). `runAsync` wires both; the curl `CURLOPT_XFERINFOFUNCTION` callback checks the base-class `cancelled` flag and returns 1 to abort. One coherent ownership model — CONC-03 builds directly on the CONC-02 wrapper, matching the roadmap's "implement CONC-02 first" sequencing. (Implies the base class exposes the cancelled flag to network call paths — planner/researcher to determine the cleanest handoff from activity → platform curl layer.)

### Curl cancellation scope & UX (CONC-03)
- **D-03:** Wire the `cancelled`-flag check into **all activity-owned network transfers** (mods, cloud saves, themes), not just `mod_download.cpp` (which already has an `xferInfo` `XFERINFOFUNCTION` at `mod_download.cpp:22`/`:65`). Other transfer paths must gain an equivalent progress-callback abort hook.
- **D-04:** On teardown abort, **drop silently** — no error UI or toast. The owning activity is being destroyed, so there is no screen to show a message on. Happy-path (non-cancelled) transfers MUST be unaffected (callback returns 0 normally).

### Test coverage (TEST-04)
- **D-05:** Cover **both** branches: (a) a host doctest for the cloud-save conflict-resolution / `plan_push` branch, AND (b) a host unit test for `runAsync`'s dropped-callback (not-alive) semantics — i.e. the `onSync` continuation is skipped when the guard is false. Regression-guards both the existing conflict logic and the new concurrency machinery.

### Claude's Discretion
- Exact `ThomazActivity` API shape, header layout, and the `runAsync(worker, onSync)` signature/ownership details (how `alive` + `cancelled` are captured and exposed to the curl layer).
- The mechanics of bridging the base-class `cancelled` flag into each platform-layer curl call site (shared_ptr handoff, per-transfer context struct, etc.).
- `dynamic_cast` null-handling specifics (log message form, early-return placement) — the safe-no-op contract is fixed; the form is open.
- Ordering/serialization of the DEBT-03 cast edits vs CONC-02 edits within the four shared activity files (planner to coordinate to avoid merge churn).
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements & roadmap
- `.planning/REQUIREMENTS.md` — CONC-02, CONC-03, DEBT-03, TEST-04 definitions + acceptance-criteria refs (AC-6B, AC-6C, AC-L).
- `.planning/ROADMAP.md` §"Phase 4: C++ Activity Hardening" — goal, 5 success criteria, and the four planning flags (brls::View::cast absence, CONC-03-after-CONC-02, DEBT-03/CONC-02 shared-file serialization, S2 constraint).

### Codebase analysis
- `.planning/codebase/CONCERNS.md` — the original activity-layer concurrency / tech-debt findings this phase closes.
- `.planning/codebase/CONVENTIONS.md` — activity/async coding patterns to follow.
- `.planning/codebase/ARCHITECTURE.md` — activity layer ↔ platform/curl layer boundaries.

### Prior-phase decisions
- `.planning/phases/03-c-platform-hardening/03-CONTEXT.md` — Phase 3 decisions, incl. the D-04 "all call-sites" precedent (mirrored here as D-01) and the CONC-01 atomic `cloudBusy` that unblocks the S2 `alive`-removal in `save_detail_activity.hpp`.

### Resolved facts (no further research needed)
- **`brls::View::cast<T>()` does NOT exist** in the vendored Borealis (`lib/borealis/library/include/borealis/core/view.hpp`) — DEBT-03 MUST use `dynamic_cast` + null guard in all cases. (Roadmap planning flag confirmed during discuss scout.)
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `source/platform/mods/mod_download.cpp:22` `xferInfo` + `:64-65` (`CURLOPT_NOPROGRESS 0` / `CURLOPT_XFERINFOFUNCTION`) — the existing progress-callback shape to replicate for the `cancelled`-flag abort across other transfer paths (CONC-03).
- Current `alive` guard idiom — `std::shared_ptr<std::atomic<bool>> alive` (e.g. `save_detail_activity.hpp:60`), hand-captured per call (`auto alive = this->alive; brls::async([this, alive, ...]{ ... brls::sync([this, alive, ...]{ if (!alive->load()) return; ... }); })` as in `mod_browser_activity.cpp:53-69`). `runAsync` should encapsulate exactly this capture+guard so call sites stop repeating it.

### Established Patterns
- All activities subclass `brls::Activity` directly today (e.g. `save_manager_activity.hpp`, `game_list_activity.hpp`, ...). Introducing `ThomazActivity` between them and `brls::Activity` is the migration vehicle for D-01.
- Destructor-sets-flag pattern already present: `mod_browser_activity.cpp:33` `*this->alive = false;` — the base class destructor should set both `alive=false` and `cancelled=true`.

### Integration Points
- Activity destructor → base-class `cancelled` flag → platform curl `XFERINFOFUNCTION` (the activity→platform bridge that D-02/D-03 require).
- Cloud-save conflict path (`source/core/saves/save_sync*.cpp` / `save_package.cpp`) — TEST-04's `plan_push`/conflict branch lives here (already host-tested infrastructure: `tests/test_save_sync*.cpp`).
</code_context>

<specifics>
## Specific Ideas

All four discussed decisions took the thorough/complete option (all call-sites, unified base-class mechanism, all-network-ops cancellation, both tests) — consistent with the milestone's "fix everything, no half-measures" hardening posture and the Phase 3 D-04 precedent.
</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope. (Two non-gating items carried from Phase 3 remain on the milestone hardware checklist, unrelated to this phase: the forced-latch TLS-banner desktop smoke and the Switch-toolchain build for the IN-03 `uid_from_hex` refactor.)
</deferred>

---

*Phase: 4-c-activity-hardening*
*Context gathered: 2026-06-05*
