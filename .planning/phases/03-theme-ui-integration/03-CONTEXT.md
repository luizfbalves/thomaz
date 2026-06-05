# Phase 3: Theme UI Integration - Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Surface the (already-built) firmware extraction engine as a first-class, one-time
(re-runnable) action in the theme UI. The Phase 2 engine
(`extract_all_base_layouts()`) is **done and unchanged** — Phase 3 only consumes
it: wire a discoverable action, show already-extracted/re-extract state, record
the firmware version the run executed against, and present clear success/failure
messaging. A successful run must immediately unblock "Aplicar Tema" because the
written outputs satisfy `cfw_paths::base_present_for()`.

Requirements: INTEG-01 (first-class action), INTEG-02 (base_missing clears →
"Aplicar Tema" proceeds), INTEG-03 (already-extracted status + manual re-extract),
INTEG-04 (record firmware version), INTEG-05 (clear success / named-reason failure).

**In scope:** the persistent "Extrair layouts do firmware" action + its placement;
extracted/not-extracted status display; manual re-extract with overwrite confirm;
during-run progress UI; completion results UI (success / named failure / partial);
firmware-version persistence + display; a passive (display-only) fw-mismatch hint.

**Out of scope (other phases / engine):**
- Any change to `extract_all_base_layouts()` or the privileged extraction chain
  (Phase 2 — locked). If granular progress needs an engine signature change, that
  is a flagged trade-off, not an assumed scope expansion (see Discretion).
- **Proactive** auto-detect-firmware-update-and-prompt-re-extraction — explicitly a
  deferred/future requirement. Phase 3 ships only a *passive* hint; re-extract stays
  user-initiated.
- The forwarder / Application-mode launcher (Phase 4 / TAKEOVER-03).
- The Phase B apply/remove/reboot flow itself — Phase 3 only feeds/unblocks it.
</domain>

<decisions>
## Implementation Decisions

### Action placement (INTEG-01)
- **D-01:** The first-class "Extrair layouts do firmware" action lives on the
  **theme browser screen** — a status row/card at the top of the theme list that
  shows extracted/not-extracted state and hosts the action. Rationale: it is
  in-context, exactly where a user goes to theme a fresh console (chosen over
  Settings, which is out of the theme flow, and a dedicated screen, which is
  heavier than needed).
- **D-01a:** **Keep** the existing `base_missing` dialog
  (`showBaseMissingDialog()` → `doExtract()`) as the **reactive** shortcut that
  fires when a user opens a theme whose base is missing. D-01's browser action is
  the *persistent/discoverable* entry; the dialog is the *just-in-time* entry.
  Both invoke the same extraction path.

### Status & re-extract UX (INTEG-03)
- **D-02:** Status is a **headline, all-or-nothing** indicator driven by
  `cfw_paths::base_present_for()` — e.g. "Layouts extraídos — firmware 21.2.0" vs
  "Layouts não extraídos". Per-title detail is NOT shown in the resting status;
  it surfaces only in the post-run results UI (D-04) when a run had failures.
- **D-03:** Re-extract is a **manual button** ("Reextrair"). Because the engine
  overwrites in place (D-03 from Phase 2) and is best-effort (a mid-run failure
  cannot leave the user base-less), risk is low — but re-extract still goes
  through a **lightweight confirm dialog** ("Reextrair do firmware atual? Isto
  sobrescreve os layouts existentes.") rather than firing silently.

### Progress & result UI (INTEG-05)
- **D-04:** **During the run:** show a **blocking, non-dismissable spinner dialog**
  ("Extraindo layouts do firmware…") wrapped around the existing `brls::async`
  call. No fake/granular progress bar — the engine exposes no progress callbacks
  and we will NOT fabricate progress.
- **D-05:** **On completion:** replace the current ephemeral
  `brls::Application::notify(...)` toast with a **results dialog** that:
  - states success clearly, OR
  - on systemic abort, **names the reason** (applet mode / key-derivation failure /
    missing title — from `systemic_error`), and
  - on partial success, shows a count ("X escritos, Y falharam") with the failed
    parts listed (from `written_parts` / `failed_parts`).
  This is what makes INTEG-05's "naming the reason" durable (a toast is too
  ephemeral to read a failure reason from).

### Firmware version (INTEG-04)
- **D-06:** **Store + show.** Persist the firmware version captured at extraction
  time (via `get_console_firmware()` / `setsysGetFirmwareVersion`) and display it
  in the resting status line (the "— firmware 21.2.0" of D-02).
- **D-07:** **Passive mismatch hint only.** When the current console firmware
  differs from the recorded extraction firmware, show a **display-only advisory**
  next to the status (e.g. "Console agora em 22.1.0 — considere reextrair"). No
  forced modal, no auto-trigger — proactive prompting is the deferred/future
  requirement. Re-extract stays user-initiated (D-03).

### Claude's Discretion
- **Persistence format/location of the recorded fw version** — a small thomaz-owned
  marker is the intent. ⚠ Do **not** reuse the bare name `ver.cfg` if it collides
  with the firmware's own `ver.cfg` observed in dumps; pick a thomaz-namespaced
  marker (e.g. under the app's data dir, or a clearly-named file in
  `/themes/systemData/`). Builder/research decides the exact scheme.
- **Whether the resting status reads "extracted" off `base_present_for()` live each
  visit, off the persisted marker, or both** — builder's call, as long as D-02's
  headline semantics and INTEG-02's immediate-unblock hold.
- **Exact Borealis widgets** (row/card/cell type, dialog vs custom view) and where
  in `theme_browser_activity` the status row mounts — builder/UI-spec decides.
- **If** truthful per-title progress is wanted during the run, it would require
  adding a progress callback to the Phase 2 engine interface. That is a
  cross-phase change — surface it as an explicit trade-off for the user, do not
  assume it. Default (D-04) needs no engine change.
</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` §"Phase 3: Theme UI Integration" — goal, success criteria
  (1–4), tentative plans 03-01/02/03.
- `.planning/REQUIREMENTS.md` — INTEG-01..05 (and the deferred "auto-detect firmware
  update" future requirement that bounds D-07).
- `.planning/phases/02-full-extraction-engine/02-CONTEXT.md` — the engine this phase
  consumes; its `<deferred>` block explicitly hands firmware-version recording and
  re-extract UX to Phase 3.

### Engine interface to consume (do NOT modify)
- `source/platform/themes/firmware_extract.hpp` — `extract_all_base_layouts()` and
  the `ExtractAllResult { ok, systemic_error, failed_parts, written_parts }`
  contract (systemic-abort vs per-part-warning). This is the ONLY engine symbol the
  UI calls. Drives D-04/D-05 messaging.

### UI integration points
- `source/app/theme_detail_activity.cpp` — current `doExtract()` (lines ~420-454,
  toast + printf, marked "Phase 3 will replace this") and `showBaseMissingDialog()`
  (~456-461). The temporary printf/notify hook is the thing Phase 3 promotes; the
  `base_missing` dialog is kept as the reactive entry (D-01a).
- `source/app/theme_detail_activity.hpp` — `doExtract()`, `consoleFw` (`FwVersion`
  captured during analysis), `analyzeCompat()`.
- `source/app/theme_browser_activity.{cpp,hpp}` — host of the new first-class status
  row/action (D-01).
- `source/app/settings_activity.{cpp,hpp}` — considered and NOT chosen for placement
  (rationale in D-01); listed so planners don't re-litigate.

### State / gating helpers to reuse
- `source/platform/themes/cfw_paths.{hpp,cpp}` — `base_present_for()` (status truth,
  D-02), `base_layout_dir()` (`/themes/systemData/`), `output_szs_path()`.
- `source/platform/themes/theme_install.cpp` — `base_present_for()` gate that the
  `base_missing` block reads; must clear immediately post-extraction (INTEG-02).
- `source/platform/themes/theme_compat.{hpp,cpp}` — `get_console_firmware()` →
  `FwVersion`; the source for D-06 recording and D-07 mismatch comparison.

### i18n
- The existing `themes/extracting`, `themes/extract_ok`, `themes/extract_fail`,
  `themes/base_missing*`, `themes/extract_now` keys — extend/replace for the new
  status, confirm, progress, and results strings (Portuguese UI).
</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `extract_all_base_layouts()` + `ExtractAllResult`: complete engine + a structured
  result the UI maps directly to D-05 messaging (no parsing, no engine change).
- `doExtract()` (theme_detail_activity): existing `busy`-lock + `brls::async`/
  `brls::sync` + `alive` guard scaffolding — reuse the threading shape; swap the
  printf/notify body for the spinner dialog (D-04) and results dialog (D-05).
- `get_console_firmware()` / `FwVersion`: already used in `analyzeCompat()`; reuse
  for D-06 recording and D-07 comparison.
- `base_present_for()`: the single source of truth for D-02 status and INTEG-02
  unblock — already consumed by the apply path.

### Established Patterns
- Borealis activities under `source/app/*_activity.{cpp,hpp}` (theme_browser,
  theme_detail, settings); `brls::Dialog` with `addButton` callbacks; i18n via
  `"key"_i18n`; toast via `brls::Application::notify`.
- Async work pattern: `busy` guard → `brls::async([...]{ ... brls::sync([...]{ if
  (!alive->load()) return; ... }); })`. Preserve for the extraction call.

### Integration Points
- Engine outputs already land flat in `/themes/systemData/`; Phase 3 adds NO file
  writing except the fw-version marker (Discretion). The "unblock" is purely that
  `base_present_for()` flips true once files exist — UI must re-read/refresh it
  after a run (D-05 → refresh the status row and the detail page's base_missing
  block).
- `consoleFw` is already captured in theme_detail during compat analysis — confirm
  whether to read fw once centrally vs per-surface (Discretion).
</code_context>

<specifics>
## Specific Ideas

- Resting status string shape: "Layouts extraídos — firmware {maj.min.mic}" /
  "Layouts não extraídos". Mismatch advisory: "Console agora em {cur} — considere
  reextrair". Re-extract confirm: "Reextrair do firmware atual? Isto sobrescreve os
  layouts existentes." During-run: "Extraindo layouts do firmware…". (UI is
  Portuguese — match existing `themes/*` i18n tone.)
- Phase 2 observed a real qlaunch `/lyt/` of ~16 szs on fw 22.1.0; a full run is a
  few seconds of privileged work — long enough that the blocking spinner (D-04) is
  warranted, short enough that no granular progress is needed.
</specifics>

<deferred>
## Deferred Ideas

- **Proactive firmware-update detection + auto-prompt to re-extract** — explicitly a
  future/deferred requirement; Phase 3 ships only the passive hint (D-07).
- **Forwarder / Application-mode launcher** so extraction needs no manual hold-`R`
  → Phase 4 (TAKEOVER-03).
- **Surfacing/installing the qlaunch IPS patch from the extraction action** — IPS is
  already wired at theme *apply* time (`qlaunch_patches.cpp`); a dedicated
  system-action surface for it is a separate nicety, not Phase 3.
- **Per-title progress UI** — would require an engine-interface change (callback);
  out of scope unless the user opts into that trade-off (see Discretion).

None of these block Phase 3.
</deferred>

---

*Phase: 3-theme-ui-integration*
*Context gathered: 2026-06-05*
