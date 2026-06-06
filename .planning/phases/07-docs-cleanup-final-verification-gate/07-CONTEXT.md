# Phase 7: Docs Cleanup & Final Verification Gate - Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Make the repository's **living** docs and code comments describe a Switch-only
tree (no desktop/PC build), document the milestone's two single-target
verification gates (host doctest + Switch build), and confirm both gates green
together. Requirements: DOC-01, VERIF-01.

This is the last phase of milestone v1.1. It changes documentation/comments and
runs verification — it does not change application behavior.
</domain>

<decisions>
## Implementation Decisions

### Cleanup scope (what counts as "docs")
- **D-01:** Clean only **living docs** — `README.md` and the `CMakeLists.txt`
  header comment (the header is already Switch-only after Plan 06-01, so README
  is the main target). There is no `CLAUDE.md` or `AGENTS.md` in the repo, so
  that clause of the success criterion is vacuously satisfied.
- **D-02:** Do **NOT** rewrite the historical archives under
  `docs/superpowers/plans/**` and `docs/superpowers/specs/**`. These are dated,
  point-in-time records of how past phases were built (when a desktop build
  existed); rewriting them would falsify project history. They are not current
  instructions.

### Grep verification gate (success criterion #1)
- **D-03:** The "grep for desktop-build instructions returns nothing stale" gate
  is scoped to **living docs**, explicitly **excluding** `docs/superpowers/**`.
  Terms to check: `build-desktop`, `run-desktop`, `PLATFORM_DESKTOP`,
  `build_desktop`, `-DUSE_SDL2`, `-DPLATFORM_DESKTOP`, "desktop PC".
  Concretely: `git grep -nE '<terms>' -- '*.md' 'CMakeLists.txt' ':!docs/superpowers/'`
  must return nothing. (Planner may also add a one-line note at the top of a
  `docs/superpowers/` index marking that tree as a historical archive — optional,
  not required.)

### README build/deploy section
- **D-04:** Remove the entire "### Desktop (PC) — para iterar na UI sem hardware"
  section from `README.md` (the `build-desktop.sh` + `build_desktop/thomaz`
  block) — the desktop target no longer exists.
- **D-05:** Point the Switch build at the canonical entrypoint
  `scripts/build-switch.sh`, noting it builds via Docker (`devkitpro/devkita64`,
  the CI path) by default and natively when `DEVKITPRO` is set. Keep the existing
  raw-cmake snippet only if useful; the script is the recommended path. Briefly
  mention deploying to a CFW Switch (copy `build_switch/thomaz.nro` to
  `/switch/thomaz.nro`, or push via `nxlink`).

### Verification flow docs (VERIF-01)
- **D-06:** Document the milestone's **two single-target gates** in the README
  build section, replacing the old desktop smoke run:
  1. Host doctest — `make -C tests test` (pure logic; still compiles the retained
     `source/platform/saves/fake_cloud_save_client.*` test double).
  2. Switch build — `scripts/build-switch.sh` produces `build_switch/thomaz.nro`.
  These two are the v1.1 verification flow.

### Claude's Discretion
- Exact prose/wording of the rewritten README sections.
- Whether to add the optional historical-archive note in `docs/superpowers/`.
- Whether to keep the raw `cmake -B build_switch ...` snippet alongside the
  `build-switch.sh` reference in README.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` — Phase 7 goal + success criteria (DOC-01, VERIF-01)
- `.planning/REQUIREMENTS.md` — DOC-01 / VERIF-01 definitions

### Files to edit (living docs)
- `README.md` §"🛠️ Compilar do código-fonte" (≈ lines 74-110) — has the stale
  "Desktop (PC)" section and `build-desktop.sh` reference to remove; "Testes"
  subsection is where the two-gate verification flow belongs
- `CMakeLists.txt` (header lines 1-4) — already Switch-only; verify, don't churn

### Build / verification entrypoints (source of truth for the rewritten docs)
- `scripts/build-switch.sh` — canonical Switch build (Docker default, native when
  `DEVKITPRO` set); produces `build_switch/thomaz.nro`
- `tests/Makefile` — host doctest gate (`make -C tests test`)

### Out of scope (historical archive — do NOT edit)
- `docs/superpowers/plans/**`, `docs/superpowers/specs/**` — dated records of past
  phases; intentionally retain desktop references as historical fact

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- The verification flow was just exercised end-to-end this session: host doctest
  passed **209/209** (`make -C tests test`), and `scripts/build-switch.sh`
  produced a working `thomaz.nro` that ran on real hardware. The docs can
  describe a flow already known to work.

### Established Patterns
- `CMakeLists.txt` is already `PLATFORM_DESKTOP`-free (Plan 06-01); the desktop
  helper scripts (`build-desktop.sh`, `run-desktop.sh`) are already deleted.
- This machine's quirks (for whoever runs the gates): native Switch build needs
  the devkitPro MSYS2 login shell; host doctest needs msys2 `gcc` +
  `-DDOCTEST_CONFIG_NO_POSIX_SIGNALS`. (Environment-specific — not for the README,
  which should stay platform-neutral; noted here for the executor.)

### Integration Points
- README is the single user-facing doc; no separate BUILDING.md exists. The
  two-gate flow fits in the existing "Testes" + Switch-build subsections.
</code_context>

<specifics>
## Specific Ideas

README is written in Portuguese (pt-BR) — keep the rewritten sections in pt-BR to
match the existing document voice.
</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope. (User skipped the gray-area
discussion; decisions above were made with sensible defaults and can be revised
before/after planning.)
</deferred>

---

*Phase: 7-Docs Cleanup & Final Verification Gate*
*Context gathered: 2026-06-05*
