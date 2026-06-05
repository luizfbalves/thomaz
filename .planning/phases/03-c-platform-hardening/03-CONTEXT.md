# Phase 3: C++ Platform Hardening - Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Resolve four isolated C++ platform-layer issues without adding features:
1. **DEBT-01/DEBT-02** — consolidate duplicated `ensure_parent_dirs` and `copy_tree` into a single shared `source/platform/fs_util.{hpp,cpp}`.
2. **CONC-01** — make `cloudBusy` a `std::atomic<bool>` with a documented threading contract; preserve existing guard behavior.
3. **SEC-03** — surface a visible, persistent on-screen warning when the TLS fail-safe degrades HTTPS to no-verification; the fail-safe *networking* behavior itself is unchanged.
4. **TEST-03** — a host doctest covers the `ca_ok == false` fail-safe branch so a regression that silently disables verification fails CI.

Verified by a clean desktop build (`-DUSE_SDL2=ON`, zero new warnings) and the host doctest suite under `tests/`.

**Not in scope:** the `runAsync` base-class refactor, unsafe-cast guarding, curl cancellation (all Phase 4). No new networking behavior — the fail-safe still proceeds with verification OFF (keeps the self-updater alive), per REQUIREMENTS SEC-03.
</domain>

<decisions>
## Implementation Decisions

### TLS fail-safe warning (SEC-03)
- **D-01:** When `ca_ok == false` and HTTPS drops to `VERIFYPEER 0`, show a **persistent on-screen warning**, not log-only. `brls::Logger::warning` alone is insufficient — SEC-03 requires on-screen visibility.
- **D-02:** Placement is **app-wide** — visible across all screens while the insecure state persists (not a one-time dialog, not per-network-screen). Maximum visibility.
- **D-02a (realization, resolved post-research):** There is **no single `brls::Application` shell** — each activity builds its own `AppletFrame`. The app-wide banner is realized via a **shared helper** following the existing `install_header_username` precedent (`source/app/app_header.cpp`): a helper called from each activity's `onContentAvailable` that injects the warning into the AppletFrame header when a process-wide `tls_insecure` flag is set. Touches ~14 activity files; uses the established codebase pattern rather than a custom always-on overlay. (Research Open Question 1 / Assumption A4 — confirmed by user.)
- **D-03:** The fail-safe networking behavior is **unchanged** — still degrades to no-verification rather than bricking all HTTPS (REQUIREMENTS SEC-03 locks this). The warning is additive; it does not block usage.

### fs_util consolidation scope (DEBT-01/DEBT-02)
- **D-04:** Consolidate **ALL** call-sites, not just the roadmap-flagged ones. `ensure_parent_dirs` currently lives in 4 files (`cheat_store.cpp`, `themes/theme_install.cpp`, `mods/libarchive_extractor.cpp`, `mods/mod_download.cpp`); `copy_tree` in 3 (`mods/mod_store.cpp`/`.hpp`, `mods/mod_actions.cpp`, `save_service_switch.cpp`). Every duplicate moves to `fs_util` and the local copy is removed — strict "exactly one" reading of DEBT-01/02. Larger diff accepted for complete hardening.
- **D-05:** The two `ensure_parent_dirs` variants (substring-at-slash in `cheat_store.cpp` vs char-by-char accumulator in `theme_install.cpp`) must be proven behaviorally equivalent **before** removing either — a doctest with a representative path (e.g. `romfs:/themes/a/b/c`) is the gate. The substring-at-slash form is the canonical candidate.

### Test seam for TLS (TEST-03)
- **D-06:** Extract a **pure policy function** — e.g. `tls_policy(bool ca_present) -> { verifypeer, verifyhost }` — that is compilable and testable on the host (outside `#ifdef __SWITCH__`). `apply_curl_tls()` consumes it. This gives a clean seam so TEST-03 exercises the real decision logic, not a mock. The current `#ifdef __SWITCH__`-gated `ca_ok` branch is not host-testable as written; the extraction fixes that.

### Claude's Discretion
- Exact `fs_util` namespace/signatures, header layout, and how the global banner view is wired into the `brls::Application` shell (planner/researcher to determine from existing UI setup).
- `cloudBusy` guard mechanics (`load`/`store` vs `compare_exchange_strong`) — preserve current semantics; planner picks the idiomatic atomic form.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements & roadmap
- `.planning/REQUIREMENTS.md` — SEC-03, CONC-01, DEBT-01, DEBT-02, TEST-03 acceptance criteria
- `.planning/ROADMAP.md` §"Phase 3: C++ Platform Hardening" — success criteria + planning flags (S1 serialize constraint: DEBT-01/02 share `fs_util` → single commit; S2 cross-phase: CONC-01 must land before Phase 4's CONC-02)
- `.planning/codebase/CONCERNS.md` — original audit entries for the tech-debt and SEC-03 items

### Source files this phase touches
- `source/platform/curl_tls.hpp` — `apply_curl_tls()`, the `ca_ok` fail-safe branch (SEC-03 + TEST-03 seam)
- `source/platform/cheat_store.cpp` — `ensure_parent_dirs` (substring-at-slash, canonical candidate)
- `source/platform/themes/theme_install.cpp` — `ensure_parent_dirs` (char-by-char variant; equivalence to verify)
- `source/platform/mods/libarchive_extractor.cpp`, `source/platform/mods/mod_download.cpp` — additional `ensure_parent_dirs` copies (D-04)
- `source/platform/mods/mod_store.cpp` / `mod_store.hpp`, `source/platform/mods/mod_actions.cpp`, `source/platform/save_service_switch.cpp` — `copy_tree` copies (D-04; `save_service_switch.cpp` confirmed present)
- `source/app/save_detail_activity.hpp` / `.cpp` — `cloudBusy` (CONC-01)
- `tests/` (doctest harness, `tests/Makefile`, `tests/run`) — where TEST-03 lands
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tests/` already hosts ~40 doctest files with a `Makefile` + `run` script — TEST-03 follows the existing host-test pattern (e.g. `test_session_codec.cpp`, `test_cfw_paths.cpp` are pure-logic precedents).
- `curl_tls.hpp` is header-only `inline` included across many TUs; `ca_ok` is a `static` local computed once per process — the warning should fire once, not per-handle.

### Established Patterns
- Platform helpers live under `source/platform/`; a new `fs_util.{hpp,cpp}` fits the existing layout (sibling to `cheat_store`, `curl_tls`).
- Desktop/host build is `-DUSE_SDL2=ON`; the `#ifdef __SWITCH__` split is the existing mechanism for Switch-only code — the pure `tls_policy` function must sit *outside* that guard to be host-compilable.

### Integration Points
- Global TLS banner connects at the `brls::Application` root UI setup (likely `source/app/` main/entry) — planner to locate the shell init.
- `fs_util` becomes a new compilation unit referenced by cheat/theme/mod/save platform code and added to the build (CMake + `tests/Makefile`).
</code_context>

<specifics>
## Specific Ideas

- TLS warning must be **persistent and global** — the user explicitly rejected log-only, transient-toast, and per-screen placement in favor of a fixed app-wide banner that stays while the insecure state lasts.
- Consolidation is deliberately the **maximal** scope (all 7 duplicate sites) rather than the minimal roadmap-flagged pair.
</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope. (Phase 4 dependencies noted: CONC-01's atomic `cloudBusy` must land before Phase 4 removes the `alive` member from the same header.)
</deferred>

---

*Phase: 3-C++ Platform Hardening*
*Context gathered: 2026-06-05*
