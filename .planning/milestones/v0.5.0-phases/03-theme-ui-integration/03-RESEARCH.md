# Phase 3: Theme UI Integration — Research

**Researched:** 2026-06-05
**Domain:** Borealis C++ UI wiring, Switch homebrew state persistence, i18n
**Confidence:** HIGH — all findings drawn directly from the codebase

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** First-class "Extrair layouts do firmware" action lives on the **theme browser screen** — a status row/card at the top of the theme list.
- **D-01a:** Keep `showBaseMissingDialog()` → `doExtract()` as the reactive shortcut; both call the same extraction path.
- **D-02:** Status is a **headline, all-or-nothing** indicator driven by `cfw_paths::base_present_for()`. Per-title detail only in post-run results UI.
- **D-03:** Re-extract is a **manual button** ("Reextrair") behind a lightweight confirm dialog.
- **D-04:** During the run: **blocking, non-dismissable spinner dialog** wrapped around `brls::async`. No granular progress bar.
- **D-05:** On completion: **results dialog** with success / named systemic failure / partial count. Replaces the ephemeral `notify()` toast in `doExtract()`.
- **D-06:** Persist the firmware version at extraction time; display in resting status line.
- **D-07:** Passive mismatch hint only — display-only advisory when current fw differs from recorded fw. No forced modal, no auto-trigger.

### Claude's Discretion
- Persistence format/location of the recorded fw version — a small thomaz-owned marker. Do NOT reuse the bare name `ver.cfg` (collides with firmware's own `ver.cfg`).
- Whether resting status reads "extracted" off `base_present_for()` live, off the persisted marker, or both.
- Exact Borealis widgets (row/card/cell type, dialog vs custom view) and where in `theme_browser_activity` the status row mounts.
- Per-title progress (trade-off only — requires engine-interface change; default D-04 needs no engine change).

### Deferred Ideas (OUT OF SCOPE)
- Proactive firmware-update detection + auto-prompt to re-extract (future requirement).
- Forwarder / Application-mode launcher (Phase 4 / TAKEOVER-03).
- Surfacing/installing the qlaunch IPS patch from the extraction action.
- Per-title progress UI (requires engine callback — out of scope unless user opts in).
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INTEG-01 | User can start extraction from an "Extrair layouts do firmware" action in the theme UI | Status row on ThemeBrowserActivity; doExtract() wiring |
| INTEG-02 | Extracted layouts written to `/themes/systemData/` so "Aplicar Tema" works immediately with no `base_missing` block | `base_present_for()` is already the gate; refreshing the status row post-run satisfies this |
| INTEG-03 | User can see whether layouts are already extracted and can re-extract on demand | Status row state (extracted/not) + Reextrair button |
| INTEG-04 | Extraction records the firmware version it ran against | `.thomaz_extract.json` in `/switch/thomaz/config/` |
| INTEG-05 | User gets a clear success message, or a failure message naming the reason | Results dialog using `ExtractAllResult.systemic_error` / `failed_parts` / `written_parts` |
</phase_requirements>

---

## Summary

Phase 3 is a pure UI wiring phase over a locked extraction engine. The code surface is three files: `theme_browser_activity.{cpp,hpp}` (new status row), `theme_detail_activity.{cpp,hpp}` (promote the Phase 2 verification trigger into the permanent entry point), and the new `extract_state_store.{cpp,hpp}` (firmware-version marker). No engine, no paths, no install logic changes.

The codebase already has every primitive needed: `base_present_for()` for status truth, `get_console_firmware()` / `fw_to_string()` for the version string, `extract_all_base_layouts()` + `ExtractAllResult` for the async call and result mapping, and `brls::async`/`brls::sync` + `busy`/`alive` guards for thread safety. The only thing missing is the status row widget in the browser, the results dialog, the marker store, and the new i18n keys.

**Primary recommendation:** Model `extract_state_store` exactly after `active_theme_store` (nlohmann JSON, same read/write/clear pattern, `/switch/thomaz/config/.thomaz_extract.json`). For the browser status row, follow the `makeActionRow` pattern from `settings_activity.cpp` — a focusable `brls::Box` with two `brls::Label` children (status text + optional mismatch advisory). The spinner dialog pattern is `new brls::Dialog(text)` with zero buttons opened before the async call and closed (or replaced) in the `brls::sync` callback; follow the existing `showBaseMissingDialog` / `showApplyChoiceDialog` structure.

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Extraction engine invocation | Platform layer (`firmware_extract_switch.cpp`) | — | Locked Phase 2; UI only calls the entry point |
| Firmware version capture | Platform layer (`theme_compat.cpp` `get_console_firmware()`) | — | Already wraps `setsysGetFirmwareVersion` |
| Extraction state persistence | Platform layer (new `extract_state_store`) | — | Follows `active_theme_store` model |
| Status row / action | App layer (`ThemeBrowserActivity`) | — | D-01 locks this placement |
| Spinner + results dialog | App layer (`ThemeBrowserActivity` + `ThemeDetailActivity`) | — | Borealis dialogs live on the UI thread |
| i18n | Resources (`resources/i18n/`) | — | All string changes here |
| base_missing reactive path | App layer (`ThemeDetailActivity::showBaseMissingDialog`) | — | D-01a — kept, just promoted to call the same permanent doExtract() |

---

## Standard Stack

No new external libraries. Phase 3 uses only what is already linked:

| Symbol | Already in | Purpose |
|--------|-----------|---------|
| `brls::Dialog` | `<borealis.hpp>` | Spinner dialog (D-04) and results dialog (D-05) |
| `brls::Box`, `brls::Label` | `<borealis.hpp>` | Status row widget in browser |
| `brls::async` / `brls::sync` | `<borealis/core/thread.hpp>` | Thread-safe async extraction call |
| `brls::Application::giveFocus` | `<borealis.hpp>` | Return focus after results dialog closes |
| `nlohmann::json` | already linked via `active_theme_store` | Marker file JSON |
| `read_text_file` / `write_text_file` | `cheat_store.hpp` | Simpler alternative to nlohmann for a one-field marker |
| `get_console_firmware()` / `fw_to_string()` | `theme_compat.hpp` | Version capture + display |
| `base_present_for()` | `cfw_paths.hpp` | Status truth for D-02 |
| `extract_all_base_layouts()` / `ExtractAllResult` | `firmware_extract.hpp` | The engine — do NOT modify |

**Installation:** none required.

---

## Package Legitimacy Audit

Not applicable — no external packages are installed in this phase.

---

## Architecture Patterns

### System Architecture Diagram

```
[User taps "Extrair / Reextrair" on ThemeBrowserActivity status row]
        |
        v
[showExtractConfirmDialog() -- D-03 if already extracted]
        |
        v (confirmed or fresh extract)
[doExtract() -- shared entry point, also called from showBaseMissingDialog]
        |
        +---> busy=true; open spinner Dialog (no buttons, non-dismissable) -- D-04
        |
        +---> brls::async([...]{ ExtractAllResult = extract_all_base_layouts(); })
                    |
                    v (worker thread completes)
              brls::sync([alive, result]{
                    if (!alive) return;
                    busy=false;
                    close spinner dialog;
                    if (result.ok) {
                        fw = get_console_firmware();
                        write .thomaz_extract.json (fw + timestamp);  -- D-06
                    }
                    open results Dialog (success / systemic / partial)  -- D-05
                    refreshExtractionStatus();  -- refresh status row
                    if (detail page open) refreshActionButton();  -- INTEG-02
              })

[ThemeBrowserActivity - resting state]
     |
     +--- onContentAvailable():
     |       call base_present_for(all_known_targets)  -- D-02 live read
     |       call load_extract_state()  -- read fw version from .thomaz_extract.json
     |       render status row: "Layouts extraídos — firmware X.Y.Z"
     |                       or "Layouts não extraídos"
     |       render mismatch hint if consoleFw != recordedFw  -- D-07
     +--- extractActionRow click -> [doExtract flow above]
```

### Recommended Project Structure

```
source/platform/themes/
├── extract_state_store.hpp      # new: ExtractState {bool extracted; std::string fw_version}
├── extract_state_store.cpp      # new: load/save/clear (follows active_theme_store pattern)
source/app/
├── theme_browser_activity.hpp   # add: extractionStatusRow*; doExtract(); refreshExtractionStatus()
├── theme_browser_activity.cpp   # add: status row build + action wire + doExtract impl
├── theme_detail_activity.cpp    # modify: doExtract() promoted (remove printf, add spinner+dialog)
resources/i18n/pt-BR/themes.json # add: new keys (see i18n section)
resources/i18n/en-US/themes.json # add: same keys in English
```

### Pattern 1: Marker File (Persistence)

**What:** A small JSON file at `/switch/thomaz/config/.thomaz_extract.json` recording the firmware version at extraction time.

**Why this location over `/themes/systemData/`:**
- `/switch/thomaz/config/` is the established "app state" directory (locale.txt, api_url.txt, session.txt all live here). It is thomaz's private area, never ambiguous.
- `/themes/systemData/` holds only `.szs` firmware layout files written by the extraction engine. Putting a `.json` marker there mixes responsibilities and risks confusion with firmware-owned files (the `ver.cfg` collision the CONTEXT.md warns about lives in the Nintendo NCA dump, not in our output dir, but the principle holds: don't co-locate app state with engine outputs).
- The name `.thomaz_extract.json` is clearly namespaced, cannot collide with anything Nintendo-owned.

**Format (matches `active_theme_store` JSON style):**
```cpp
// Source: active_theme_store.cpp (codebase — VERIFIED)
// Proposed schema for .thomaz_extract.json:
// {
//   "fw_version": "22.1.0"   // fw_to_string(get_console_firmware()) at extraction time
// }

struct ExtractState {
    std::string fw_version;   // empty = no recorded extraction
};
```

**Read/write (mirrors `active_theme_store.cpp` exactly):**
```cpp
// Source: active_theme_store.cpp lines 14-50 (codebase — VERIFIED)
namespace {
std::string extract_state_path() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/.thomaz_extract.json";
#else
    return "thomaz-cache/.thomaz_extract.json";
#endif
}
} // namespace

std::optional<ExtractState> load_extract_state() {
    std::ifstream in(extract_state_path());
    if (!in) return std::nullopt;
    auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    ExtractState s;
    s.fw_version = j.value("fw_version", std::string());
    return s.fw_version.empty() ? std::nullopt : std::optional<ExtractState>(s);
}

void save_extract_state(const ExtractState& s) {
    // ensure_parent_dirs is NOT needed here: /switch/thomaz/config/ is
    // guaranteed to exist because locale.txt and api_url.txt are written
    // there at first settings save. On desktop, write_text_file handles dirs.
    nlohmann::json j;
    j["fw_version"] = s.fw_version;
    std::ofstream out(extract_state_path(), std::ios::trunc);
    out << j.dump(2);
}
```

**Alternative considered (plain text via `write_text_file`):** Could store just `"22.1.0\n"` via `write_text_file`. This is slightly simpler but is less extensible if a future phase wants to record a timestamp or extraction count. The JSON approach is already established and adds only 3 extra lines. Recommend JSON.

### Pattern 2: Status Row (Theme Browser)

**What:** A focusable banner row at the top of the results area in `ThemeBrowserActivity`, built programmatically in `onContentAvailable()` and added before the `resultsBox`.

**Widget type:** `brls::Box(brls::Axis::ROW)` — same as every other action widget in this codebase. The `makeActionRow` helper in `settings_activity.cpp` (lines 30-47) is the exact analog; replicate it inline or extract to a shared helper.

**Structure:**
```
[statusRow  brls::Box ROW height=56 cornerRadius=12 focusable=true]
  ├── [statusLabel  brls::Label]       "Layouts extraídos — firmware 22.1.0"
  │                                  or "Layouts não extraídos"
  ├── [mismatchLabel brls::Label]      "Console agora em 22.2.0 — considere reextrair"
  │                                    visibility=GONE unless mismatch  -- D-07
  └── [actionLabel  brls::Label]       "Extrair" / "Reextrair"
```

**Where to mount:** Insert the `statusRow` before `resultsBox` inside the `ScrollingFrame`'s column `Box`, similar to how the tab row is positioned. The XML cannot express it dynamically, so it must be done in `onContentAvailable()` by getting the parent `Box` that contains `resultsBox` and calling `addView()` or `insertView()` before it, or by reserving a named empty `Box` container (`id="extractionRow"`) in the XML.

**Recommended approach — reserve a named slot in the XML:**
```xml
<!-- add to theme_browser.xml, between the tab row Box and spinner -->
<brls:Box id="extractionRow" axis="column" width="auto" marginBottom="14"/>
```
Then populate it in `onContentAvailable()`. This avoids C++ sibling-insertion complexity and mirrors how `detailContent` is populated lazily in `theme_detail.xml`.

**Click action (same pattern as cards):**
```cpp
// Source: theme_browser_activity.cpp lines 197-202 (codebase — VERIFIED)
statusRow->registerClickAction([this](brls::View*) {
    brls::sync([this]() { this->doExtract(); });
    return true;
});
statusRow->addGestureRecognizer(new brls::TapGestureRecognizer(statusRow));
```

### Pattern 3: Spinner Dialog (D-04)

**What:** A `brls::Dialog` opened with a spinner label and zero buttons (non-dismissable) before the `brls::async` call; dismissed by calling `dialog->dismiss()` inside `brls::sync` on completion.

**Key insight from codebase audit:** `brls::Dialog` takes a string body. For a spinner we want a `brls::ProgressSpinner` widget, not just text. The codebase uses in-activity `ProgressSpinner` widgets (set visibility VISIBLE/GONE) for inline spinners, and `brls::Dialog(text)` for blocking confirmations. For the blocking spinner dialog (D-04), the cleanest approach consistent with the codebase is:
- Open `new brls::Dialog("themes/extracting"_i18n)` with **zero buttons** before the async call.
- Hold a raw pointer to it; call `dialog->dismiss()` in `brls::sync` when done.
- The dialog is non-dismissable because there are no buttons; controller B is not wired unless a button is added.

**Concrete implementation shape:**
```cpp
// Source: theme_detail_activity.cpp lines 376-382 showApplyChoiceDialog() (codebase — VERIFIED)
// Adapted for spinner pattern:
void ThemeBrowserActivity::doExtract() {
    if (this->busy) return;
    this->busy = true;

    auto* spinnerDialog = new brls::Dialog("themes/extracting"_i18n);
    // No addButton() -> non-dismissable
    spinnerDialog->open();

    auto alive = this->alive;
    brls::async([this, alive, spinnerDialog]() {
        ExtractAllResult res = extract_all_base_layouts();

        brls::sync([this, alive, spinnerDialog, res]() {
            if (!alive->load()) {
                spinnerDialog->dismiss();
                return;
            }
            this->busy = false;
            spinnerDialog->dismiss();

            if (res.ok) {
                ExtractState st;
                st.fw_version = fw_to_string(get_console_firmware());
                save_extract_state(st);
            }

            this->showExtractResultDialog(res);
            this->refreshExtractionStatus();
            // INTEG-02: if a detail page is stacked, it will re-check
            // base_present_for() when the user returns (activity resumes).
        });
    });
}
```

**Thread safety:** `spinnerDialog` is heap-allocated by Borealis and lives until `dismiss()` is called. The raw pointer captured by the lambda is safe: the dialog outlives the async call because it is owned by Borealis's view tree, not by the activity. This is the same ownership model as the existing dialog patterns in the codebase. [ASSUMED: Borealis dialog ownership model — could not verify via official docs, but consistent with all existing usage in codebase]

### Pattern 4: Results Dialog (D-05)

**What:** A `brls::Dialog` opened after the spinner closes, with a descriptive body and an "OK" dismiss button.

**Body string assembly (maps `ExtractAllResult` to human text):**
```cpp
// Source: firmware_extract.hpp lines 28-33 (codebase — VERIFIED)
void ThemeBrowserActivity::showExtractResultDialog(const ExtractAllResult& res) {
    std::string body;
    if (!res.ok) {
        // Systemic failure — name the reason (INTEG-05)
        body = "themes/extract_fail"_i18n + "\n" + res.systemic_error;
    } else if (res.failed_parts.empty()) {
        // Clean success
        body = "themes/extract_ok"_i18n;
    } else {
        // Partial success — show count (D-05)
        body = "themes/extract_partial"_i18n;  // new key: "X escritos, Y falharam"
        body += " (" + std::to_string(res.written_parts.size()) + " / " +
                std::to_string(res.written_parts.size() + res.failed_parts.size()) + ")";
        // List failed parts (truncate at 4 to avoid overflow on Switch 720p UI)
        for (size_t i = 0; i < res.failed_parts.size() && i < 4; ++i)
            body += "\n  " + res.failed_parts[i];
    }
    auto* dialog = new brls::Dialog(body);
    dialog->addButton("themes/extract_result_ok"_i18n, []() {});  // new key: "OK"
    dialog->open();
}
```

**Analog:** `showApplyChoiceDialog()` (lines 376-382) and `showRebootDialog()` (lines 463-467) — same `new brls::Dialog(text)` + `addButton` + `open()` pattern.

### Pattern 5: Status Refresh (INTEG-02)

**What:** After a successful extraction run, `ThemeBrowserActivity` must update its status row. If a `ThemeDetailActivity` is currently on the stack, it will re-read `base_present_for()` when it regains focus (because Borealis re-calls `willAppear` or the user navigates back), which is sufficient for INTEG-02 — `base_present_for()` reads the filesystem live.

**Refresh shape:**
```cpp
void ThemeBrowserActivity::refreshExtractionStatus() {
    // Re-read live status
    bool extracted = base_present_for(all_known_targets_vector());
    auto state     = load_extract_state();  // may be nullopt if write failed

    // Update labels in-place (no rebuild needed)
    if (this->statusLabel) {
        if (extracted && state) {
            this->statusLabel->setText("themes/extract_status_ok"_i18n + " — " + state->fw_version);
        } else {
            this->statusLabel->setText("themes/extract_status_none"_i18n);
        }
    }
    if (this->actionLabel) {
        this->actionLabel->setText(extracted ? "themes/reextract"_i18n : "themes/extract_now"_i18n);
    }
    if (this->mismatchLabel) {
        // D-07 mismatch check
        FwVersion console = get_console_firmware();
        std::string consoleFwStr = fw_to_string(console);
        if (extracted && state && !state->fw_version.empty() &&
            consoleFwStr != state->fw_version) {
            this->mismatchLabel->setText("themes/extract_mismatch"_i18n + " " + consoleFwStr);
            this->mismatchLabel->setVisibility(brls::Visibility::VISIBLE);
        } else {
            this->mismatchLabel->setVisibility(brls::Visibility::GONE);
        }
    }
}
```

**`all_known_targets_vector()` helper:** Returns `{"ResidentMenu","Entrance","Flaunch","Set","Notification","Psl","MyPage","common"}` — the full set of targets in `target_map()` (verified in `cfw_paths.cpp` lines 22-32). `base_present_for()` returns false if ANY target is missing, matching D-02's "all-or-nothing" headline.

**Detail page INTEG-02:** `ThemeDetailActivity::doApply()` already calls `base_layouts_available(detail)` which calls `base_present_for()` live (line 268). After extraction, the next Apply tap will see the files and proceed. No explicit refresh message needs to be sent across activity boundaries.

**Re-extract confirm dialog (D-03):**
```cpp
void ThemeBrowserActivity::doExtractOrConfirm() {
    bool extracted = base_present_for(all_known_targets_vector());
    if (extracted) {
        auto* dialog = new brls::Dialog("themes/reextract_confirm"_i18n);
        dialog->addButton("themes/reextract_confirm_yes"_i18n, [this]() { this->doExtract(); });
        dialog->addButton("themes/apply_cancel"_i18n, []() {});  // reuse existing key
        dialog->open();
    } else {
        this->doExtract();
    }
}
```

### Anti-Patterns to Avoid

- **Storing fw version in `base_layout_dir()` (= `/themes/systemData/`)**: Mixes app state with engine outputs. The `ver.cfg` name collision risk noted in CONTEXT.md applies here. Use `/switch/thomaz/config/` instead.
- **Reading status only from the persisted marker:** The marker can be absent (first run, or if write failed). Always use `base_present_for()` as the authoritative source; use the marker only for the fw version string and mismatch check.
- **Calling `get_console_firmware()` on the UI thread in `refreshExtractionStatus()`:** `get_console_firmware()` calls `setsysInitialize/GetFirmwareVersion/Exit` — three IPC calls. On Switch these are fast (< 1ms each) but blocking. Since `refreshExtractionStatus()` runs in `brls::sync` (already on the UI thread post-extraction), this is acceptable for a single call. Do NOT call it on every frame or inside a tight loop.
- **Fabricating per-part progress:** The engine exposes no callback. D-04 explicitly forbids fake progress. Show the spinner text only.
- **Forgetting to set `busy=false` on the alive-guard early return:** The existing `doExtract()` in `theme_detail_activity.cpp` has this correct (line 445: `this->busy = false` before every return path). Preserve this.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| JSON state persistence | Custom text parser | `nlohmann::json` + `active_theme_store` pattern | Already linked, battle-tested, handles malformed input gracefully |
| File I/O with parent dir creation | `mkdir` chains | `write_text_file()` from `cheat_store.hpp` | Already handles `ensure_parent_dirs` |
| Firmware version string | Manual `sprintf` | `fw_to_string(FwVersion)` from `theme_compat.hpp` | Already implemented, returns `"22.1.0"` format |
| Thread-safe async | Raw `std::thread` | `brls::async` / `brls::sync` | Borealis-aware thread pool with UI-thread callback |
| Modal blocking dialog | Custom overlay | `new brls::Dialog(text)` with zero buttons | Established Borealis pattern; non-dismissable with no buttons |
| Status truth for "extracted" | Custom file scan | `base_present_for(targets)` | Single source of truth already consumed by the apply path |

---

## Open Decisions (Discretion — Recommendations)

### 1. Persistence format/location

**Recommendation:** `/switch/thomaz/config/.thomaz_extract.json` (Switch) / `thomaz-cache/.thomaz_extract.json` (desktop), nlohmann JSON, `fw_version` key only.

**Rationale:**
- Follows the `active_theme_store` precedent exactly — the planner can use it as a blueprint.
- `/switch/thomaz/config/` is already the established app-state directory; no new directory creation needed.
- `.thomaz_` prefix is the existing naming convention for thomaz-owned files in shared directories (`.thomaz_active.json`).
- Plain-text alternative (just `"22.1.0\n"`) is simpler but breaks the established JSON convention and is less extensible.

### 2. Resting status read strategy

**Recommendation: Read BOTH, with `base_present_for()` as authoritative and the marker for display-only data.**

- `extracted = base_present_for(all_known_targets)` — this is the D-02 truth; it also serves INTEG-02 (no cache needed, always live).
- `state = load_extract_state()` — used only to get `fw_version` string for the status label and mismatch check; if absent/malformed, show no fw version but still show the extracted/not status correctly.

This means if someone manually puts files in `/themes/systemData/` without using thomaz, the status row shows "Layouts extraídos" (correct by D-02) but no fw version (acceptable — we don't know the version).

### 3. Exact Borealis widgets

**Recommendation:** In-line `brls::Box` row built in `onContentAvailable()`, mounted via a named XML placeholder `<brls:Box id="extractionRow"/>`. Store raw label pointers as member fields for `refreshExtractionStatus()` to update in-place. See Pattern 2 above for the full spec.

**No Borealis List / RecyclerView:** The browser doesn't use `brls::List` or recycler widgets — it builds everything as `brls::Box` trees with `addView`. The status row is one more box in that tree.

### 4. Threading/refresh shape

**Confirmed pattern (from `doExtract()` lines 420-454):**
```
busy=true → open spinner dialog → brls::async([alive]{
    result = extract_all_base_layouts();
    brls::sync([alive, result]{
        if (!alive->load()) { dismiss dialog; return; }
        busy=false;
        dismiss dialog;
        if (ok) save_extract_state();
        showExtractResultDialog(result);
        refreshExtractionStatus();
    });
})
```

The existing `doExtract()` in `theme_detail_activity.cpp` already uses this shape (lines 421-453). Phase 3 promotes and extends it:
- Remove the `printf` logging (Phase 2 verification code).
- Add spinner dialog open/close around the async call.
- Add `save_extract_state()` on success.
- Replace `brls::Application::notify()` toast with `showExtractResultDialog()`.
- Add `refreshExtractionStatus()` call.

**Where `doExtract()` lives:** It can stay in `ThemeDetailActivity` (for the reactive `showBaseMissingDialog` entry point, D-01a) and be duplicated/extracted to `ThemeBrowserActivity` for the browser entry point, OR it can be refactored into a shared free function. Given the `alive`/`busy` guards are per-activity fields, the safest approach is to duplicate the logic in `ThemeBrowserActivity::doExtract()` and simplify `ThemeDetailActivity::doExtract()` to call the same underlying flow (or to call into the browser's doExtract if reachable). The simplest plan-able approach: both activities have their own `doExtract()` with the same body but their own `busy`/`alive` guards. Note that both cannot run concurrently on the same device, so no synchronization between them is needed.

---

## i18n Inventory

### Existing `themes/*` keys (relevant to Phase 3)

| Key | pt-BR value | Phase 3 use |
|-----|-------------|-------------|
| `themes/extract_now` | "Extrair agora" | Reused in status row action label (fresh extract) |
| `themes/extracting` | "Extraindo layouts do firmware…" | Spinner dialog body (D-04) — **keep as-is** |
| `themes/extract_ok` | "Layouts extraídos para sd:/themes/systemData. Toque em Aplicar novamente." | Partial reuse — the new success dialog body replaces this toast |
| `themes/extract_fail` | "Falha na extração" | Systemic failure dialog header (D-05) |
| `themes/base_missing` | "Layouts base não encontrados" | Still used by reactive dialog title |
| `themes/base_missing_help` | "Os layouts base do firmware ainda não foram extraídos..." | Still used by reactive dialog body |
| `themes/base_missing_close` | "Fechar" | Still used by reactive dialog dismiss button |
| `themes/apply_cancel` | "Cancelar" | **Reuse** for re-extract confirm cancel button (D-03) |

### New keys needed (Phase 3)

| Key | pt-BR | en-US | Purpose |
|-----|-------|-------|---------|
| `themes/extract_status_ok` | "Layouts extraídos — firmware" | "Layouts extracted — firmware" | Status row: extracted state prefix (fw version appended with `+ " " + fw_version`) |
| `themes/extract_status_none` | "Layouts não extraídos" | "Layouts not extracted" | Status row: not-yet-extracted state |
| `themes/extract_mismatch` | "Console agora em" | "Console now at" | Status row: mismatch hint prefix (fw version appended); D-07 |
| `themes/reextract` | "Reextrair" | "Re-extract" | Status row action label when already extracted; D-03 button |
| `themes/reextract_confirm` | "Reextrair do firmware atual? Isto sobrescreve os layouts existentes." | "Re-extract from current firmware? This overwrites existing layouts." | Re-extract confirm dialog body; D-03 |
| `themes/reextract_confirm_yes` | "Reextrair" | "Re-extract" | Re-extract confirm dialog confirm button |
| `themes/extract_partial` | "Extração parcial" | "Partial extraction" | Results dialog: partial success label; D-05 |
| `themes/extract_result_ok` | "OK" | "OK" | Results dialog dismiss button; D-05 |
| `themes/extract_success_title` | "Layouts extraídos com sucesso." | "Layouts extracted successfully." | Results dialog: full success body (replaces ephemeral toast) |

**Notes:**
- `themes/extract_ok` currently says "Toque em Aplicar novamente" which implies it was a toast shown after the detail-page flow. The new results dialog on the browser doesn't need that guidance. Propose replacing the string body for `extract_ok` to "Layouts extraídos com sucesso." in the new results dialog, OR introducing `themes/extract_success_title` and keeping `extract_ok` for any future toast use. The planner should pick one; recommend using `extract_success_title` and leaving `extract_ok` unchanged for backward compat.
- `themes/extract_mismatch` is display-only advisory. The full assembled string is: `"themes/extract_mismatch"_i18n + " " + consoleFwStr + " — " + "themes/extract_mismatch_advice"_i18n`. Consider splitting or keeping as a format string — recommend a single key with the advice baked in, appending only the version: `"Console agora em {version} — considere reextrair"`. The planner picks the exact split.

---

## Common Pitfalls

### Pitfall 1: Stale status row after extraction

**What goes wrong:** The browser status row keeps showing "Layouts não extraídos" after a successful run because `onContentAvailable()` ran once on mount and is not called again.

**Why it happens:** Borealis activities don't re-run `onContentAvailable()` when they come back into focus from a stacked activity. The in-place label update in `refreshExtractionStatus()` is the only refresh mechanism.

**How to avoid:** Store raw `brls::Label*` pointers to the status/mismatch/action labels as member fields. Call `refreshExtractionStatus()` from the `brls::sync` callback after extraction completes.

**Warning signs:** Status row still shows "not extracted" after a completed run; apply button still shows base_missing dialog.

### Pitfall 2: Double-call via both entry points

**What goes wrong:** User has the base_missing dialog open from the reactive path AND clicks the browser status row — `doExtract()` runs twice.

**Why it happens:** The `busy` flag is per-activity. If the browser's `busy` is false and the detail page's `busy` is also false, both can fire.

**How to avoid:** The `showBaseMissingDialog` → `doExtract()` path in `ThemeDetailActivity` already has `if (!this->resolved || this->busy) return`. The browser's `doExtract()` has its own `busy` guard. Since both activities can be on the stack simultaneously, add a static or singleton "extraction in progress" gate at the engine layer... OR simply accept that the engine is idempotent (D-03: overwrite in place) and the worst case is two runs back-to-back, not a crash. **Recommendation:** Add a simple `static std::atomic_bool sExtractionRunning{false}` in a shared translation unit (e.g., `extract_state_store.cpp`) gating both entry points. Mark it in the plan as a concurrency guard.

### Pitfall 3: `get_console_firmware()` called before `setsys` is initialized

**What goes wrong:** On desktop, `get_console_firmware()` returns `{0,0,0}` (no-op). On Switch, it calls `setsysInitialize()` / `setsysGetFirmwareVersion()` / `setsysExit()` — three IPC calls. If called from `brls::async` (worker thread), these calls work but bypass the Borealis thread model.

**How to avoid:** Call `get_console_firmware()` inside the `brls::sync` callback (UI thread) after the extraction completes, not inside `brls::async`. The fw version is only needed for persistence, which happens after the run. This is consistent with how `analyzeCompat()` in `theme_detail_activity.cpp` calls `get_console_firmware()` inside a `brls::async` lambda — which is technically correct but not ideal. For Phase 3, do it in `brls::sync` to be safe.

### Pitfall 4: `all_known_targets` missing `common`

**What goes wrong:** `base_present_for()` returns false if any target in the list is missing its file. If the list for the "all-or-nothing" status check doesn't include `common`, and the user hasn't extracted yet, the status row shows the wrong state.

**Why it happens:** `common` was added to `target_map()` in Phase 2 (cfw_paths.cpp line 31). It must be in the `all_known_targets` vector used for the headline check.

**How to avoid:** The `all_known_targets_vector()` helper must include all 8 entries: `ResidentMenu`, `Entrance`, `Flaunch`, `Set`, `Notification`, `Psl`, `MyPage`, `common`. Verify against `cfw_paths.cpp target_map()` at implementation time.

### Pitfall 5: Dialog pointer dangling after activity pop

**What goes wrong:** If the user somehow pops the activity while the async extraction is running (should be impossible with busy guard, but if the guard fails), the `spinnerDialog->dismiss()` in `brls::sync` crashes.

**How to avoid:** The `alive` guard already handles this (`if (!alive->load()) return`). Add `spinnerDialog->dismiss()` in the alive-guard early return path too (before `return`), so the dialog is always cleaned up.

---

## Code Examples

### Active theme store — the persistence blueprint

```cpp
// Source: source/platform/themes/active_theme_store.cpp lines 10-41 (codebase — VERIFIED)
namespace {
std::string active_path() { return themes_root() + "/.thomaz_active.json"; }
} // namespace

std::optional<ActiveTheme> get_active_theme() {
    std::ifstream in(active_path());
    if (!in) return std::nullopt;
    auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    // ... field extraction with j.value() defaults
    return result;
}

void set_active_theme(const ActiveTheme& t) {
    ::mkdir(themes_root().c_str(), 0777);  // best-effort
    nlohmann::json j;
    j["field"] = t.field;
    std::ofstream out(active_path(), std::ios::trunc);
    out << j.dump(2);
}
```

### doExtract() current shape — the body to promote

```cpp
// Source: source/app/theme_detail_activity.cpp lines 420-454 (codebase — VERIFIED)
void ThemeDetailActivity::doExtract() {
    if (!this->resolved || this->busy) return;
    this->busy = true;
    brls::Application::notify("themes/extracting"_i18n);  // Phase 3: replace with dialog
    auto alive = this->alive;
    brls::async([this, alive]() {
        ExtractAllResult res = extract_all_base_layouts();
        // Phase 2 printf logging removed in Phase 3
        brls::sync([this, alive, res]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!res.ok) {
                brls::Application::notify("themes/extract_fail"_i18n + ...);  // → results dialog
                return;
            }
            brls::Application::notify("themes/extract_ok"_i18n);  // → results dialog
            this->refreshActionButton();
        });
    });
}
```

### Dialog pattern

```cpp
// Source: source/app/theme_detail_activity.cpp lines 456-461 showBaseMissingDialog() (codebase — VERIFIED)
void ThemeDetailActivity::showBaseMissingDialog() {
    auto* dialog = new brls::Dialog("themes/base_missing_help"_i18n);
    dialog->addButton("themes/extract_now"_i18n, [this]() { this->doExtract(); });
    dialog->addButton("themes/base_missing_close"_i18n, []() {});
    dialog->open();
}
```

### Inline action row pattern

```cpp
// Source: source/app/settings_activity.cpp lines 30-47 makeActionRow() (codebase — VERIFIED)
auto* row = new brls::Box(brls::Axis::ROW);
row->setHeight(56.0f);
row->setFocusable(true);
row->setMarginTop(10.0f);
row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
row->setCornerRadius(12.0f);
row->setAlignItems(brls::AlignItems::CENTER);
row->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
auto* label = new brls::Label();
label->setText(text);
label->setFontSize(16.0f);
label->setGrow(1.0f);
row->addView(label);
```

---

## Runtime State Inventory

Not applicable (greenfield UI addition, no rename/refactor). No runtime state migration required.

---

## Environment Availability

All dependencies are in-tree or already available from Phase 2. No new external tools needed.

| Dependency | Required By | Available | Notes |
|------------|------------|-----------|-------|
| `firmware_extract.hpp` / `extract_all_base_layouts()` | INTEG-01/05 | Yes (Phase 2 complete) | Do not modify |
| `theme_compat.hpp` / `get_console_firmware()` | INTEG-04 / D-06/D-07 | Yes | Already used by analyzeCompat() |
| `cfw_paths.hpp` / `base_present_for()` | INTEG-02 / D-02 | Yes | Already gate for apply path |
| `nlohmann::json` | extract_state_store | Yes (linked via active_theme_store) | |
| `cheat_store.hpp` / `write_text_file` | Optional alt for marker | Yes | Alternative to nlohmann JSON |

---

## Validation Architecture

`nyquist_validation` is explicitly `false` in `.planning/config.json` — this section is omitted.

---

## Security Domain

This phase reads/writes only to `/switch/thomaz/config/` (app's own data directory) and reads the filesystem state of `/themes/systemData/`. No network calls, no user input, no authentication changes. No new ASVS categories apply beyond those already covered by the existing apply path.

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `brls::Dialog` with zero buttons is non-dismissable (controller B does not close it) | Pattern 3 — Spinner Dialog | If B still closes it, the spinner dialog becomes dismissable mid-extraction; mitigation: test and add a `registerCancelAction` no-op if needed |
| A2 | `brls::Dialog::dismiss()` is safe to call from `brls::sync` (UI thread) while the dialog is open | Pattern 3 | If dismiss() must be called from a different mechanism, the spinner teardown needs adjustment |
| A3 | `spinnerDialog` raw pointer remains valid until `dismiss()` is called (Borealis owns dialog lifetime via view tree) | Pattern 3 | If Borealis deletes dialogs eagerly, calling dismiss() would be a use-after-free; mitigation: wrap in an alive flag or use a shared_ptr |

All three relate to Borealis dialog lifecycle — the existing codebase uses dialogs in the same way (new + open + no explicit delete) so A3 is highly confident. A1 and A2 are consistent with all observed usage patterns but were not verified against Borealis source or documentation.

---

## Open Questions

1. **`doExtract()` ownership — share or duplicate?**
   - What we know: both `ThemeBrowserActivity` (D-01) and `ThemeDetailActivity` (D-01a reactive) need to call extraction with the same busy/alive/spinner/results pattern.
   - What's unclear: whether to duplicate the method body or extract to a free function + pass alive/busy by ref.
   - Recommendation: Duplicate in Phase 3 for simplicity. A free function refactor is a low-risk follow-up.

2. **`extract_ok` key reuse vs new key**
   - What we know: `themes/extract_ok` currently reads "Layouts extraídos para sd:/themes/systemData. Toque em Aplicar novamente." — written for the toast. The new results dialog doesn't need "Toque em Aplicar" since the user is already on the browser.
   - What's unclear: whether to update the existing key text or add `themes/extract_success_title`.
   - Recommendation: Add `themes/extract_success_title` = "Layouts extraídos com sucesso."; leave `extract_ok` unchanged. Simpler plan, no risk of breaking the reactive path if it still uses the toast path transitionally.

---

## Sources

### Primary (HIGH confidence — codebase direct read)
- `source/app/theme_detail_activity.cpp` lines 420-468 — existing `doExtract()` and `showBaseMissingDialog()`; the threading pattern to promote
- `source/app/theme_browser_activity.cpp` lines 1-253 — full browser activity; no status row exists yet
- `source/platform/themes/active_theme_store.{hpp,cpp}` — JSON persistence blueprint
- `source/platform/app_settings.cpp` — `/switch/thomaz/config/` directory convention
- `source/platform/themes/cfw_paths.{hpp,cpp}` — `base_present_for()` and `target_map()` (8 targets confirmed)
- `source/platform/themes/theme_compat.{hpp,cpp}` — `get_console_firmware()` / `fw_to_string()`
- `source/platform/themes/firmware_extract.hpp` — `ExtractAllResult` contract
- `source/app/settings_activity.cpp` lines 30-47 — `makeActionRow()` pattern
- `resources/i18n/pt-BR/themes.json` — complete existing key inventory
- `resources/xml/activity/theme_browser.xml` — current browser XML (no extraction row)

### Secondary (MEDIUM confidence — CONTEXT.md / verified against codebase)
- `03-CONTEXT.md` — locked decisions D-01 through D-07, all verified against existing code patterns

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new packages; all symbols already in codebase
- Architecture patterns: HIGH — derived directly from codebase reading
- i18n inventory: HIGH — read from actual JSON files
- Borealis dialog lifecycle (A1-A3): MEDIUM — consistent with codebase usage, not verified against Borealis docs

**Research date:** 2026-06-05
**Valid until:** Stable — no fast-moving external dependencies; valid until the codebase changes
