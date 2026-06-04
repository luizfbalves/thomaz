# Phase 1: Privileged Extraction Spike - Context

**Gathered:** 2026-06-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Prove the entire privileged extraction path works end-to-end on real hardware with
the smallest possible vertical slice: re-vendor the removed dependencies, run thomaz
under title takeover, mount raw BIS → resolve via `lr` → derive the NCA header key
on-device via SPL → decrypt with the bundled hactool fork, and extract **one** qlaunch
layout (`ResidentMenu.szs`) to the SD card — keyless to the user (no `prod.keys`).

The goal is to validate the two highest-risk, hardware-only unknowns BEFORE building
the full engine on top:
1. Do the pinned public key sources still derive a valid header key on the target firmware?
2. Is title-takeover permission sufficient for raw BIS / SPL / pmdmnt / lr?

**This is a thin vertical slice of REAL code, not a throwaway harness.** The port done
here is the foundation Phase 2 (Full Extraction Engine) grows in scope — it is not
rewritten. "Spike" = narrow scope (one szs, one title) to de-risk, not disposable code.

In scope: one-szs extraction path + re-vendoring + title-takeover enablement/docs.
Out of scope: all-titles/all-szs extraction (Phase 2), the theme UI action and
`base_missing` unblock (Phase 3), a forwarder NSP (Phase 4 / TAKEOVER-03).

</domain>

<decisions>
## Implementation Decisions

### Code Shape (the central decision this discussion resolved)
- **D-01:** Phase 1 produces **real, production-shaped, keeper code** — the actual port
  of `key_loader` (BIS mount + `lr` resolve + SPL key derivation) and the `hactool`
  in-memory NCA RomFS extractor. Phase 2 **extends** this (more titles/szs); it does
  **not** replace or rewrite it. The mechanism is already proven in production by
  exelix (Option A is not theory) — the user's explicit reason to proceed directly to
  real code. The "spike" framing is about *scope* (one szs to validate hardware
  unknowns), not code quality or disposability.

### Spike Scope
- **D-02:** Extract exactly **one** szs: `ResidentMenu.szs` from the qlaunch title
  `0100000000001000`. One title → one file is the thinnest slice that still exercises
  the full BIS→lr→SPL→hactool chain and validates both hardware unknowns.

### Output Path
- **D-03:** Write the extracted szs to the **canonical** location the rest of the app
  already consumes — `base_layout_dir()/ResidentMenu.szs` (i.e. `/themes/systemData/ResidentMenu.szs`
  on the flat layout `cfw_paths` expects) — NOT a throwaway `/thomaz_spike/` dir. If
  extraction succeeds, the file is immediately in the place `cfw_paths`/`theme_install`
  expect, so the spike result is reusable rather than discarded.

### Title-Takeover Method
- **D-04:** The spike uses and documents the **hbloader title override** path
  (hold-`R` while launching an installed game/title, which relaunches thomaz in
  Application mode), because it is zero-install and manual — appropriate for a spike.
  A **forwarder NSP** (installable Application-mode icon) is explicitly **deferred to
  Phase 4 (TAKEOVER-03)**. Phase 1 only needs *a* working Application-mode launch path.
- **D-05:** Detect applet vs Application mode at runtime (`appletGetAppletType() != Application`).
  In applet mode, show a clear user-facing "relaunch via title takeover" message and
  exit cleanly — no crash, no silent `fsOpenBisFileSystem` failure (TAKEOVER-01).

### Dependency Re-vendoring (note: thomaz builds with CMake, not exelix's Makefile)
- **D-06:** Re-vendor the **hactool fork** and a **custom `libmbedtls.a` built with
  `MBEDTLS_CMAC_C`** (reversing the Phase B exclusion). Build mbedtls **from source in
  CI/Docker (devkitA64)** under the existing CMake build — do **not** commit a prebuilt
  binary `.a` blob — for reproducibility. Translate exelix's Makefile `LIBS`/`LIBDIRS`/`INCLUDES`
  wiring into the equivalent CMake target wiring. Re-add hactool/mbedtls attribution to
  `THIRD_PARTY.md`.
- **D-07:** Pin the public SPL key **sources** from a specific, recorded Atmosphère
  release (version + commit). Record provenance — the Atmosphère source version AND the
  firmware version the spike actually ran against — in BOTH `THIRD_PARTY.md` and the
  user-facing title-takeover doc (satisfies Success Criterion #4: "SPL key-source
  provenance recorded against the firmware the spike ran on").

### Desktop Build Stays Green
- **D-08:** Extraction entry point lives in `platform/themes/*_switch.cpp` (real impl)
  with a `platform/themes/*_fake.cpp` desktop no-op. No BIS/SPL/hactool symbols may be
  pulled into the desktop target — the desktop build must compile and link green.

### Claude's Discretion
- Exact wording of the applet-mode message and the surface used (default: a user-facing
  Borealis dialog, consistent with existing app dialogs, rather than only a log line).
- Internal file/function/class names for the ported `key_loader`/`hactool` code.
- Whether to port the optional `RomfsCache` now or defer to Phase 2 (default: defer the
  cache — not needed to validate the spike).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Research & approach (locked)
- `.planning/research/EXTRACTION.md` — Verified Option A mechanism (BIS→lr→SPL→hactool),
  title-ID/szs tables, hardware-only open questions, exelix permalinks pinned to `2618b0c`.
- `.planning/ROADMAP.md` — Phase 1 section: goal, success criteria, 5-plan breakdown.
- `.planning/REQUIREMENTS.md` — EXTRACT-04, TAKEOVER-01, TAKEOVER-02 definitions.

### Consumer code (output must match what these expect)
- `source/platform/themes/cfw_paths.hpp` / `cfw_paths.cpp` — `base_layout_dir()`,
  `target_map()`, `base_szs_path()`; the flat `/themes/systemData/<szs>` layout the
  spike must write into.
- `source/platform/themes/theme_install.cpp` — `base_present_for()` gate that consumes
  the extracted base layouts (Phase 3 clears it; Phase 1 just produces the file).

### Reference implementation (exelix, GPLv2 @ `2618b0c`) — port faithfully
- `SwitchThemesNX/source/SwitchTools/key_loader.cpp` (`__SWITCH__` branch) — BIS mount,
  `lr` resolve, SPL key derivation.
- `SwitchThemesNX/source/SwitchTools/hactool.cpp` — in-memory NCA RomFS extraction + filter.
- `SwitchThemesNX/source/SwitchThemesCommon/Common.{hpp,cpp}` — title-ID/szs tables.
- `SwitchThemesNX/source/SwitchTools/RomfsCache.cpp` — optional cache (likely deferred).
- `SwitchThemesNX/source/Pages/NcaDumpPage.cpp` — original entry-point UI for reference.
  (Full permalinks pinned to `2618b0c` are listed in `.planning/research/EXTRACTION.md`.)

### Codebase conventions
- `.planning/codebase/ARCHITECTURE.md` — core/ vs platform/ (switch/fake) split.
- `.planning/codebase/CONVENTIONS.md` — coding conventions to match.
- `.planning/codebase/STACK.md` — build system (CMake + devkitA64), CI/Docker.
- `THIRD_PARTY.md` — where hactool/mbedtls attribution + key-source provenance gets recorded.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `cfw_paths::base_szs_path("ResidentMenu")` / `base_layout_dir()` — gives the exact
  canonical output path the spike should write to (D-03).
- Phase B vendored the exelix theme engine behind an `apply_facade` (commit `2cdec9d`) —
  the established pattern for wrapping vendored GPLv2 exelix code; the extraction port
  should follow the same vendoring/facade shape.

### Established Patterns
- `core/` = pure, host-tested via doctest; `platform/` = `*_switch.cpp` real impl +
  `*_fake.cpp` desktop no-op. The privileged extraction is a `platform/themes` concern
  and CANNOT be host-tested — host coverage applies only to pure parsing/mapping logic.
- Build is **CMake** (`CMakeLists.txt`), not a Makefile — exelix's `LIBS`/`LIBDIRS`
  wiring must be translated into CMake target wiring. (`tests/Makefile` is host-test only.)

### Integration Points
- Output flows into the canonical `/themes/systemData/` dir that `cfw_paths` and
  `theme_install::base_present_for()` already read — the spike feeds, but does not yet
  change, the Phase B apply flow.

</code_context>

<specifics>
## Specific Ideas

- Single concrete target for the spike: `ResidentMenu.szs`, qlaunch title `0100000000001000`.
- Rationale to proceed straight to real code: Option A is a **proven, in-production
  mechanism** in exelix — not unverified theory. The only genuinely unknown variables
  are thomaz-specific and hardware-only (target firmware key derivation + thomaz's
  title-takeover permission set), which is exactly what the one-szs slice validates.

</specifics>

<deferred>
## Deferred Ideas

- **Forwarder NSP** (installable Home-menu icon launching thomaz directly in Application
  mode) → Phase 4 / TAKEOVER-03 (optional).
- **Full extraction** — all qlaunch szs (Entrance/Flaunch/Set/Notification/common) plus
  Psl and MyPage → Phase 2.
- **Theme UI action** ("Extrair layouts do firmware"), `base_missing` unblock, already-
  extracted state, firmware-version recording, success/failure messaging → Phase 3.
- **`RomfsCache` port** — likely unnecessary for the spike; revisit in Phase 2.

</deferred>

---

*Phase: 1-privileged-extraction-spike*
*Context gathered: 2026-06-04*
