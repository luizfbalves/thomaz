# Phase 2: Full Extraction Engine - Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Generalize the proven single-target extraction spike (Phase 1) into the complete
extraction engine: pull every required `.szs` from all three system titles —
qlaunch (`0100000000001000`), Psl (`0100000000001007`), MyPage
(`0100000000001013`) — and write them into the flat `/themes/systemData/` layout
that `cfw_paths::base_layout_dir()` expects, with the exact filenames the theme
engine resolves.

Requirements: EXTRACT-01 (qlaunch layouts incl. `common`), EXTRACT-02 (Psl),
EXTRACT-03 (MyPage). EXTRACT-04 (keyless) is already satisfied and carried
forward.

**In scope:** multi-title / multi-szs extraction loop; per-title `/lyt/` filter;
flat output layout; per-output structural validation; best-effort failure
reporting; host doctest for the pure mapping/path logic.

**Out of scope (other phases):** the theme-UI "Extrair layouts" action, progress
UI, firmware-version recording, re-extract UX (all Phase 3); forwarder (Phase 4).
</domain>

<decisions>
## Implementation Decisions

### Target set
- **D-01:** Extract **every `/lyt/*.szs`** present in each title, not just the 8
  canonical theme targets. This guarantees the canonical set (ResidentMenu,
  Entrance, Flaunch, Set, Notification, **common** + Psl + MyPage) is always
  covered and is future-proof: themes that target less-common parts work without
  a code change. Cost is marginal — same BIS mount / NCA resolve / key
  derivation per title, only the RomFS filter widens from one filename to the
  whole `/lyt/` directory.
- **D-01a:** `cfw_paths::target_map()` is currently missing **`common`** (it has
  7 entries: ResidentMenu/Entrance/Flaunch/Set/Notification/Psl/MyPage). With
  D-01 the extraction filter no longer depends on enumerating `target_map()` —
  but `target_map()`/`base_present_for()` (used by the apply path) must still be
  reconciled so `common` and any newly-relevant target resolve correctly.

### Failure policy
- **D-02:** **Best-effort + report.** A missing/absent part or title is a warning
  — extract everything that succeeds, collect the failures, and report which
  parts/titles failed and why. The whole run does not abort because one `.szs`
  or one optional title is unavailable.
- **D-02a:** **Systemic** failures are a hard abort: if SPL key derivation or the
  BIS-System mount fails, stop the whole run (continuing is pointless). Distinguish
  per-part failures (warn, continue) from systemic ones (abort).

### Re-extraction / overwrite
- **D-03:** **Overwrite each file in place.** Re-running is idempotent and is the
  correct behavior after a firmware update. Do NOT skip existing files (would
  leave stale szs after an update → broken themes) and do NOT wipe-then-extract
  (a mid-run failure would leave the user with no base at all).

### Output validation
- **D-04:** A `.szs` is accepted only if it **structurally validates**: it must
  Yaz0-decompress and unpack as a valid SARC. A non-empty byte count is not
  sufficient. This is the exact validation built host-side this session (it
  catches a subtly-corrupt extraction before it silently reaches the apply path).
  Reuse the already-linked `lib/switchthemes` `Yaz0`/`SARC` for the check; keep
  the mapping/path logic host-testable (success criterion 4).

### Claude's Discretion
- HOW to structure the loop (mount BIS once and reuse across all three titles vs
  per-title), NCA resolution, and where validation runs in the pipeline — builder/
  research/planning decide.
- Whether "all `/lyt/*.szs`" is implemented as a directory-glob filter or an
  enumerated set — builder's call, as long as D-01 holds.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` §"Phase 2: Full Extraction Engine" — goal, success
  criteria (1–4), tentative plans 02-01/02/03.
- `.planning/REQUIREMENTS.md` — EXTRACT-01, EXTRACT-02, EXTRACT-03 (and EXTRACT-04
  already met).
- `.planning/phases/01-privileged-extraction-spike/` — the proven spike this
  phase generalizes (CONTEXT, RESEARCH, SUMMARYs).

### Code to generalize / reconcile
- `source/platform/themes/firmware_extract_switch.cpp` — the single-target spike:
  applet gate → BIS mount → `lr` NCA resolve → SPL key derive → NCA extract →
  validate → write. Generalize to iterate titles/parts.
- `source/platform/themes/nca_extract_switch.cpp` — in-memory NCA RomFS facade;
  `nca_romfs_filter` / `filter_list` is where the single-filename filter widens
  to `/lyt/`.
- `source/platform/themes/key_loader_switch.cpp` — per-title BIS/`lr`/SPL key
  derivation (keyless).
- `source/platform/themes/cfw_paths.cpp` — `target_map()` (missing `common`),
  `base_layout_dir()` (`/themes/systemData`), `output_szs_path()`,
  `base_present_for()`.

### Mapping table to port (per roadmap plan 02-01)
- `lib/switchthemes/Common.hpp`, `lib/switchthemes/Common.cpp` — exelix
  `ThemeTargetInfo` title-ID / szs-name tables to port and reconcile with
  `target_map()`.

### Validation primitives (reuse)
- `lib/switchthemes/SarcLib/Yaz0.hpp`, `lib/switchthemes/SarcLib/Sarc.hpp` —
  `Yaz0::Decompress` + `SARC::Unpack` for the D-04 structural check (already
  linked into the Switch build via the apply path).
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `firmware_extract_switch.cpp`: the entire single-target pipeline — wrap its body
  in a per-title / per-part loop; keep the `*_fake.cpp` desktop no-op pattern.
- `key_loader_switch.cpp`: NCA resolution + key derivation already works per
  title-ID — feed it `…1000`, `…1007`, `…1013`.
- `lib/switchthemes` `Yaz0`/`SARC`: already compiled into the Switch target; the
  exact Decompress→Unpack validation was exercised host-side this session.

### Established Patterns
- `*_switch.cpp` real impl + `*_fake.cpp` desktop stub keeps the desktop build
  green (no BIS/SPL/hactool symbols on desktop) — Phase 1 invariant, preserve it.
- Flat `/themes/systemData/<name>.szs` output (adapted from exelix's
  `extracted/{qlaunch,…}/` subdirs) — success criterion 3, do not nest.

### Integration Points
- Output feeds `cfw_paths::base_present_for()` → unblocks "Aplicar Tema" (Phase 3
  consumes this; Phase 2 just produces the files).
- `target_map()` is shared with the apply path — reconciling `common`/new targets
  must not break apply.
</code_context>

<specifics>
## Specific Ideas

- Real on-hardware dump (fw 22.1.0) of the qlaunch title `/lyt/` contained ~16
  szs: the canonical ResidentMenu/Entrance/Flaunch/Set/Notification/common plus
  DataTransfer, Eula, Gift, Interrupt, Migration, Option, SaveMove, Vgc, MyPage,
  Psl-adjacent. Confirms "all `/lyt/*.szs`" (D-01) is a clean superset of the
  canonical set — and that each title decompresses/parses as valid SARC.
</specifics>

<deferred>
## Deferred Ideas

- **Firmware-version recording** (`ver.cfg`, e.g. `21.2.0` seen in the spike dump)
  and the re-extract-after-update UX → Phase 3 (INTEG-03).
- **"Extrair layouts do firmware" UI action**, progress reporting, success/failure
  messaging → Phase 3 (INTEG-01/04).
- **qlaunch IPS patch installation** is already wired at theme **apply** time
  (this session, `qlaunch_patches.cpp`); whether to also surface/install it from
  the extraction or a dedicated system action is a Phase 3 integration nicety, not
  Phase 2.
</deferred>

---

*Phase: 2-full-extraction-engine*
*Context gathered: 2026-06-05*
