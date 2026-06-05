# Phase 3: Theme UI Integration — Pattern Map

**Mapped:** 2026-06-05
**Files analyzed:** 7 new/modified files
**Analogs found:** 7 / 7

---

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `source/platform/themes/extract_state_store.hpp` | model/store | file-I/O | `source/platform/themes/active_theme_store.hpp` | exact |
| `source/platform/themes/extract_state_store.cpp` | service | file-I/O | `source/platform/themes/active_theme_store.cpp` | exact |
| `source/app/theme_browser_activity.hpp` | component | request-response | `source/app/theme_browser_activity.hpp` (extend) | self |
| `source/app/theme_browser_activity.cpp` | component | request-response + async | `source/app/settings_activity.cpp` (makeActionRow + async) | role-match |
| `source/app/theme_detail_activity.cpp` | component | async | `source/app/theme_detail_activity.cpp` (promote doExtract) | self |
| `resources/i18n/pt-BR/themes.json` | config | — | `resources/i18n/pt-BR/themes.json` (extend) | self |
| `resources/i18n/en-US/themes.json` | config | — | `resources/i18n/en-US/themes.json` (extend) | self |

---

## Pattern Assignments

### `source/platform/themes/extract_state_store.hpp` (model/store, file-I/O)

**Analog:** `source/platform/themes/active_theme_store.hpp` (lines 1–29)

**Struct + free-function declarations pattern** (active_theme_store.hpp lines 1–29):
```cpp
#pragma once
#include <optional>
#include <string>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

struct ActiveTheme {
    std::string hex_id;
    std::string name;
    std::string author;
    std::vector<std::string> targets;
};

std::optional<ActiveTheme> get_active_theme();
void set_active_theme(const ActiveTheme& t);
void clear_active_theme();
bool is_active_theme(const thomaz::core::ThemeEntry& entry);

} // namespace thomaz
```

**Adapt to:**
```cpp
#pragma once
#include <optional>
#include <string>

namespace thomaz {

struct ExtractState {
    std::string fw_version;   // fw_to_string() at extraction time; empty = no record
};

// Read /switch/thomaz/config/.thomaz_extract.json; nullopt if absent/malformed/empty.
std::optional<ExtractState> load_extract_state();

// Write/overwrite the extract-state record.
void save_extract_state(const ExtractState& s);

// Remove the record (e.g. after a clear).
void clear_extract_state();

// Global extraction-in-progress guard (prevents double-fire from both entry points).
// Set true before starting brls::async; reset false inside brls::sync on completion.
extern std::atomic_bool sExtractionRunning;

} // namespace thomaz
```

---

### `source/platform/themes/extract_state_store.cpp` (service, file-I/O)

**Analog:** `source/platform/themes/active_theme_store.cpp` (lines 1–52) — copy structure verbatim, swap fields.

**Imports + path helper** (active_theme_store.cpp lines 1–11):
```cpp
#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_paths.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sys/stat.h>

namespace thomaz {

namespace {
std::string active_path() { return themes_root() + "/.thomaz_active.json"; }
} // namespace
```

Adapt path helper to:
```cpp
namespace {
std::string extract_state_path() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/.thomaz_extract.json";
#else
    return "thomaz-cache/.thomaz_extract.json";
#endif
}
} // namespace

std::atomic_bool sExtractionRunning{false};
```

**Read pattern** (active_theme_store.cpp lines 14–30):
```cpp
std::optional<ActiveTheme> get_active_theme() {
    std::ifstream in(active_path());
    if (!in) return std::nullopt;
    auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    ActiveTheme t;
    t.hex_id = j.value("hex_id", std::string());
    t.name   = j.value("name", std::string());
    t.author = j.value("author", std::string());
    if (j.contains("targets") && j["targets"].is_array()) {
        for (const auto& e : j["targets"])
            if (e.is_string()) t.targets.push_back(e.get<std::string>());
    }
    if (t.hex_id.empty()) return std::nullopt;
    return t;
}
```

Adapt to:
```cpp
std::optional<ExtractState> load_extract_state() {
    std::ifstream in(extract_state_path());
    if (!in) return std::nullopt;
    auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    ExtractState s;
    s.fw_version = j.value("fw_version", std::string());
    return s.fw_version.empty() ? std::nullopt : std::optional<ExtractState>(s);
}
```

**Write pattern** (active_theme_store.cpp lines 32–41):
```cpp
void set_active_theme(const ActiveTheme& t) {
    ::mkdir(themes_root().c_str(), 0777);  // best-effort
    nlohmann::json j;
    j["hex_id"]  = t.hex_id;
    j["name"]    = t.name;
    j["author"]  = t.author;
    j["targets"] = t.targets;
    std::ofstream out(active_path(), std::ios::trunc);
    out << j.dump(2);
}
```

Adapt to (no mkdir needed — `/switch/thomaz/config/` already exists):
```cpp
void save_extract_state(const ExtractState& s) {
    nlohmann::json j;
    j["fw_version"] = s.fw_version;
    std::ofstream out(extract_state_path(), std::ios::trunc);
    out << j.dump(2);
}
```

**Clear pattern** (active_theme_store.cpp lines 43–45):
```cpp
void clear_active_theme() {
    ::remove(active_path().c_str());
}
```

---

### `source/app/theme_browser_activity.hpp` (component, request-response)

**Analog:** `source/app/theme_browser_activity.hpp` (self, lines 1–41) — extend with new members.

**Existing header pattern** (theme_browser_activity.hpp lines 1–41):
```cpp
#pragma once
#include <atomic>
#include <memory>
#include <string>

#include <borealis.hpp>

#include "core/themes/themezer_browse.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

class ThemeBrowserActivity : public brls::Activity {
  public:
    explicit ThemeBrowserActivity(IHttpClient* http);
    ~ThemeBrowserActivity() override;

    CONTENT_FROM_XML_RES("activity/theme_browser.xml");
    void onContentAvailable() override;

  private:
    void reload();
    void runQuery(int page);
    void populate(const thomaz::core::BrowsePage& page);
    void loadThumb(const std::string& url, brls::Image* into);
    void openSearch();
    void cyclePart();

    IHttpClient* http;
    bool         packsMode = true;
    std::string  query;
    std::string  target;
    int          page = 1;
    bool         isComplete = true;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

**Add to private section:**
```cpp
    // --- Extraction status row (Phase 3) ---
    void buildExtractionRow();          // called from onContentAvailable(); populates extractionRow
    void doExtractOrConfirm();          // entry: confirm if already extracted (D-03), then doExtract
    void doExtract();                   // busy/async/sync/alive extraction flow (D-04/D-05)
    void showExtractResultDialog(const ExtractAllResult& res);  // D-05 results dialog
    void refreshExtractionStatus();     // in-place label update post-run (INTEG-02)

    // Raw label pointers for in-place refresh (lifetime owned by view tree)
    brls::Label* statusLabel   = nullptr;
    brls::Label* mismatchLabel = nullptr;
    brls::Label* actionLabel   = nullptr;

    bool busy = false;  // extraction guard (same pattern as ThemeDetailActivity)
```

**Also add includes needed** (after existing includes):
```cpp
#include "platform/themes/extract_state_store.hpp"
#include "platform/themes/firmware_extract.hpp"
#include "platform/themes/cfw_paths.hpp"
#include "platform/themes/theme_compat.hpp"
```

---

### `source/app/theme_browser_activity.cpp` (component, request-response + async)

**Three sub-patterns apply:**

#### Sub-pattern A: makeActionRow — inline status row construction

**Analog:** `source/app/settings_activity.cpp` lines 30–47 (`makeActionRow` helper)

```cpp
// settings_activity.cpp lines 30-47
brls::Box* makeActionRow(const std::string& text)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setHeight(56.0f);
    row->setFocusable(true);
    row->setMarginTop(10.0f);
    row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
    row->setCornerRadius(12.0f);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(16.0f);
    label->setGrow(1.0f);
    row->addView(label);
    return row;
}
```

**Click wire pattern** (settings_activity.cpp lines 102–108):
```cpp
auto* updateRow = makeActionRow("thomaz/update/check"_i18n);
updateRow->registerClickAction([this, status](brls::View*) {
    this->checkForUpdate(status);
    return true;
});
updateRow->addGestureRecognizer(new brls::TapGestureRecognizer(updateRow));
listBox->addView(updateRow);
```

**Adapt for the status row (with multiple labels + mismatch advisory):**
```cpp
void ThemeBrowserActivity::buildExtractionRow() {
    auto* container = (brls::Box*)this->getView("extractionRow");
    if (!container) return;

    auto* row = new brls::Box(brls::Axis::ROW);
    row->setHeight(56.0f);
    row->setFocusable(true);
    row->setMarginTop(0.0f);
    row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
    row->setCornerRadius(12.0f);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));

    this->statusLabel = new brls::Label();
    this->statusLabel->setFontSize(15.0f);
    this->statusLabel->setGrow(1.0f);
    row->addView(this->statusLabel);

    this->mismatchLabel = new brls::Label();
    this->mismatchLabel->setFontSize(13.0f);
    this->mismatchLabel->setMarginLeft(12.0f);
    this->mismatchLabel->setVisibility(brls::Visibility::GONE);
    row->addView(this->mismatchLabel);

    this->actionLabel = new brls::Label();
    this->actionLabel->setFontSize(15.0f);
    this->actionLabel->setMarginLeft(16.0f);
    row->addView(this->actionLabel);

    row->registerClickAction([this](brls::View*) {
        brls::sync([this]() { this->doExtractOrConfirm(); });
        return true;
    });
    row->addGestureRecognizer(new brls::TapGestureRecognizer(row));

    container->addView(row);
    this->refreshExtractionStatus();
}
```

#### Sub-pattern B: busy/async/sync/alive — async extraction flow

**Analog:** `source/app/theme_detail_activity.cpp` lines 420–454 (existing `doExtract()`)

**Existing shape to promote (theme_detail_activity.cpp lines 420–454):**
```cpp
void ThemeDetailActivity::doExtract() {
    if (!this->resolved || this->busy) return;

    this->busy = true;
    brls::Application::notify("themes/extracting"_i18n);  // Phase 3: replace with spinner dialog

    auto alive = this->alive;
    brls::async([this, alive]() {
        ExtractAllResult res = extract_all_base_layouts();

        // ... printf logging (remove in Phase 3) ...

        brls::sync([this, alive, res]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!res.ok) {
                brls::Application::notify("themes/extract_fail"_i18n + ...);  // replace with dialog
                return;
            }
            brls::Application::notify("themes/extract_ok"_i18n);  // replace with dialog
            this->refreshActionButton();
        });
    });
}
```

**Promoted shape for ThemeBrowserActivity::doExtract():**
```cpp
void ThemeBrowserActivity::doExtract() {
    if (this->busy) return;
    if (sExtractionRunning.exchange(true)) return;  // global double-fire guard
    this->busy = true;

    auto* spinnerDialog = new brls::Dialog("themes/extracting"_i18n);
    // No addButton() → non-dismissable (A1 assumption)
    spinnerDialog->open();

    auto alive = this->alive;
    brls::async([this, alive, spinnerDialog]() {
        ExtractAllResult res = extract_all_base_layouts();

        brls::sync([this, alive, spinnerDialog, res]() {
            spinnerDialog->dismiss();  // always dismiss, even on alive=false
            if (!alive->load()) {
                sExtractionRunning = false;
                return;
            }
            this->busy = false;
            sExtractionRunning = false;

            if (res.ok) {
                ExtractState st;
                st.fw_version = fw_to_string(get_console_firmware());  // UI thread: safe
                save_extract_state(st);
            }

            this->showExtractResultDialog(res);
            this->refreshExtractionStatus();
        });
    });
}
```

#### Sub-pattern C: Dialog construction

**Analog:** `source/app/theme_detail_activity.cpp` lines 456–467

**showBaseMissingDialog + showRebootDialog** (theme_detail_activity.cpp lines 456–467):
```cpp
void ThemeDetailActivity::showBaseMissingDialog() {
    auto* dialog = new brls::Dialog("themes/base_missing_help"_i18n);
    dialog->addButton("themes/extract_now"_i18n, [this]() { this->doExtract(); });
    dialog->addButton("themes/base_missing_close"_i18n, []() {});
    dialog->open();
}

void ThemeDetailActivity::showRebootDialog() {
    auto* dialog = new brls::Dialog("themes/reboot_prompt"_i18n);
    dialog->addButton("themes/reboot_now"_i18n, []() { thomaz::reboot_to_payload(); });
    dialog->addButton("themes/reboot_later"_i18n, []() {});
    dialog->open();
}
```

**Adapt for confirm + results dialogs:**
```cpp
void ThemeBrowserActivity::doExtractOrConfirm() {
    bool extracted = base_present_for({"ResidentMenu","Entrance","Flaunch","Set",
                                       "Notification","Psl","MyPage","common"});
    if (extracted) {
        auto* dialog = new brls::Dialog("themes/reextract_confirm"_i18n);
        dialog->addButton("themes/reextract_confirm_yes"_i18n, [this]() { this->doExtract(); });
        dialog->addButton("themes/apply_cancel"_i18n, []() {});  // reuse existing key
        dialog->open();
    } else {
        this->doExtract();
    }
}

void ThemeBrowserActivity::showExtractResultDialog(const ExtractAllResult& res) {
    std::string body;
    if (!res.ok) {
        body = "themes/extract_fail"_i18n + "\n" + res.systemic_error;
    } else if (res.failed_parts.empty()) {
        body = "themes/extract_success_title"_i18n;  // new key
    } else {
        body = "themes/extract_partial"_i18n;  // new key
        body += " (" + std::to_string(res.written_parts.size()) + " / " +
                std::to_string(res.written_parts.size() + res.failed_parts.size()) + ")";
        for (size_t i = 0; i < res.failed_parts.size() && i < 4; ++i)
            body += "\n  " + res.failed_parts[i];
    }
    auto* dialog = new brls::Dialog(body);
    dialog->addButton("themes/extract_result_ok"_i18n, []() {});  // new key: "OK"
    dialog->open();
}
```

**Also adapt for checkForUpdate's async pattern** (settings_activity.cpp lines 133–180) — this is the exact analog for the busy guard + brls::sync label update shape used in `refreshExtractionStatus()`.

---

### `source/app/theme_detail_activity.cpp` (component, async — modify only)

**Analog:** Self — promote the existing `doExtract()` (lines 420–454).

**What to change in the existing body:**

1. Remove `brls::Application::notify("themes/extracting"_i18n)` (line 424) — replaced by spinner dialog in browser's doExtract.
2. Remove all `std::printf` logging (lines 432–441) — Phase 2 verification code.
3. In `brls::sync` callback: replace the two `brls::Application::notify()` calls (lines 447–451) with a call to `showExtractResultDialog(res)` and `this->refreshActionButton()`.
4. Add `sExtractionRunning = false` and `sExtractionRunning.exchange(true)` guard mirroring the browser version.
5. Add spinner dialog open/close around the async call (same as browser shape above).

**Existing busy/alive guard shape to preserve** (theme_detail_activity.cpp lines 420–422):
```cpp
void ThemeDetailActivity::doExtract() {
    if (!this->resolved || this->busy) return;
    this->busy = true;
```

**Existing alive guard in brls::sync to preserve** (theme_detail_activity.cpp lines 443–445):
```cpp
brls::sync([this, alive, res]() {
    if (!alive->load()) return;
    this->busy = false;
```

Note: add `spinnerDialog->dismiss()` before the `if (!alive->load()) return` so the dialog is always cleaned up even when the activity has been popped.

---

### `resources/i18n/pt-BR/themes.json` + `resources/i18n/en-US/themes.json` (config)

**Analog:** Existing `themes.json` files — extend with new keys only, do not modify existing keys.

**Existing key patterns** (pt-BR/themes.json lines 57–66):
```json
"base_missing": "Layouts base não encontrados",
"base_missing_help": "Os layouts base do firmware ainda não foram extraídos...",
"base_missing_close": "Fechar",
"extract_now": "Extrair agora",
"extracting": "Extraindo layouts do firmware…",
"extract_ok": "Layouts extraídos para sd:/themes/systemData. Toque em Aplicar novamente.",
"extract_fail": "Falha na extração",
"extract_no_targets": "Este tema não tem layouts para extrair.",
"reboot_prompt": "Reiniciar o console agora para aplicar?",
"reboot_now": "Reiniciar",
"reboot_later": "Depois"
```

**New keys to append (pt-BR):**
```json
"extract_status_ok": "Layouts extraídos — firmware",
"extract_status_none": "Layouts não extraídos",
"extract_mismatch": "Console agora em",
"extract_mismatch_advice": "— considere reextrair",
"reextract": "Reextrair",
"reextract_confirm": "Reextrair do firmware atual? Isto sobrescreve os layouts existentes.",
"reextract_confirm_yes": "Reextrair",
"extract_partial": "Extração parcial",
"extract_result_ok": "OK",
"extract_success_title": "Layouts extraídos com sucesso."
```

**New keys to append (en-US):**
```json
"extract_status_ok": "Layouts extracted — firmware",
"extract_status_none": "Layouts not extracted",
"extract_mismatch": "Console now at",
"extract_mismatch_advice": "— consider re-extracting",
"reextract": "Re-extract",
"reextract_confirm": "Re-extract from current firmware? This overwrites existing layouts.",
"reextract_confirm_yes": "Re-extract",
"extract_partial": "Partial extraction",
"extract_result_ok": "OK",
"extract_success_title": "Layouts extracted successfully."
```

**Existing keys NOT changed:** `extract_ok`, `extract_fail`, `extracting`, `extract_now`, `apply_cancel` — reused as-is.

---

### `resources/xml/activity/theme_browser.xml` (config — modify)

**Analog:** `resources/xml/activity/theme_browser.xml` (self, lines 1–32) — add named slot.

**Existing structure** (theme_browser.xml lines 1–32):
```xml
<brls:AppletFrame id="themeBrowserFrame" title="@i18n/themes/title" iconInterpolation="linear">
    <brls:ScrollingFrame width="auto" height="auto" grow="1.0">
        <brls:Box width="auto" height="auto" axis="column" paddingTop="28" paddingLeft="40" paddingRight="40" paddingBottom="24">
            <brls:Box axis="row" alignItems="center" marginBottom="14">
                <!-- tab buttons ... -->
            </brls:Box>
            <brls:ProgressSpinner id="spinner" .../>
            <brls:Label id="emptyLabel" .../>
            <AnimatedBox id="resultsBox" .../>
        </brls:Box>
    </brls:ScrollingFrame>
</brls:AppletFrame>
```

**Add extraction row slot** — insert between the tab row `</brls:Box>` (line 26) and the `<brls:ProgressSpinner>` (line 27):
```xml
<!-- Extraction status row slot — populated by buildExtractionRow() in onContentAvailable() -->
<brls:Box id="extractionRow" axis="column" width="auto" marginBottom="14"/>
```

This follows the same pattern as `detailContent` in `theme_detail.xml` — a named empty container populated programmatically.

---

## Shared Patterns

### busy/alive Guard
**Source:** `source/app/theme_detail_activity.hpp` lines 47–52 + `source/app/theme_detail_activity.cpp` lines 420–445
**Apply to:** `ThemeBrowserActivity::doExtract()`, `ThemeDetailActivity::doExtract()` (promoted)

```cpp
// hpp: member fields
bool busy = false;
std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);

// dtor: poison the alive flag
~ThemeBrowserActivity() { *this->alive = false; }

// method guard at top
if (this->busy) return;
this->busy = true;

// brls::sync early-exit
if (!alive->load()) {
    spinnerDialog->dismiss();
    sExtractionRunning = false;
    return;
}
this->busy = false;
sExtractionRunning = false;
```

### brls::async / brls::sync Thread Model
**Source:** `source/app/theme_browser_activity.cpp` lines 92–106 (runQuery) and `source/app/settings_activity.cpp` lines 143–180 (checkForUpdate)
**Apply to:** Both `doExtract()` implementations

```cpp
// Include
#include <borealis/core/thread.hpp>

// Pattern
brls::async([this, alive, /* captures */]() {
    // worker thread: only compute, no UI calls
    auto result = expensive_operation();

    brls::sync([this, alive, result]() {
        if (!alive->load()) return;
        // UI thread: update labels, open dialogs, refresh views
    });
});
```

### Dialog Construction
**Source:** `source/app/theme_detail_activity.cpp` lines 456–467
**Apply to:** `showExtractResultDialog()`, `doExtractOrConfirm()` confirm dialog, spinner dialog

```cpp
auto* dialog = new brls::Dialog(bodyString);
dialog->addButton("key"_i18n, [/*capture*/]() { /* action */ });
// Zero addButton() calls = non-dismissable
dialog->open();
// To dismiss from brls::sync:
dialog->dismiss();
```

### i18n String Lookup
**Source:** `source/app/settings_activity.cpp` line 23; `source/app/theme_browser_activity.cpp` line 17
**Apply to:** All new string literals in browser + detail activities

```cpp
using namespace brls::literals;
// Then anywhere:
"themes/some_key"_i18n
// Concatenation:
"themes/extract_status_ok"_i18n + std::string(" ") + state->fw_version
```

### Click Registration on Focusable Box
**Source:** `source/app/theme_browser_activity.cpp` lines 56–57 + 67–70
**Apply to:** Status row click wire, `doExtractOrConfirm` entry

```cpp
someBox->registerClickAction([this](brls::View*) {
    brls::sync([this]() { this->someMethod(); });
    return true;
});
someBox->addGestureRecognizer(new brls::TapGestureRecognizer(someBox));
```

### nlohmann::json Read/Write
**Source:** `source/platform/themes/active_theme_store.cpp` lines 14–41
**Apply to:** `extract_state_store.cpp`

```cpp
#include <nlohmann/json.hpp>
#include <fstream>

// Read
std::ifstream in(path);
if (!in) return std::nullopt;
auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
if (j.is_discarded() || !j.is_object()) return std::nullopt;
std::string val = j.value("key", std::string());

// Write
nlohmann::json j;
j["key"] = value;
std::ofstream out(path, std::ios::trunc);
out << j.dump(2);
```

---

## All-Known-Targets Vector

The `base_present_for()` call for the browser's "all-or-nothing" status check must use all 8 targets confirmed in `source/platform/themes/cfw_paths.cpp` lines 22–32:

```cpp
// cfw_paths.cpp lines 22-32 — all 8 entries
static const std::vector<std::string> kAllTargets = {
    "ResidentMenu", "Entrance", "Flaunch", "Set",
    "Notification", "Psl", "MyPage", "common"
};
// Use as: base_present_for(kAllTargets)
// Define this constant in theme_browser_activity.cpp anonymous namespace.
```

---

## No Analog Found

All 7 files have direct analogs. No gaps.

---

## Metadata

**Analog search scope:** `source/app/`, `source/platform/themes/`, `resources/i18n/`, `resources/xml/activity/`
**Files scanned:** 12 source files read directly
**Pattern extraction date:** 2026-06-05
