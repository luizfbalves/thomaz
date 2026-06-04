# Native Theme Apply (Phase B) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn a downloaded `.nxtheme`/pack into an installed on-console theme — compile it to a firmware `.szs`, write it to LayeredFS, track/remove it, and prompt for reboot — all from the app.

**Architecture:** A vendored GPLv2 port of exelix11's C++ theme engine (`lib/switchthemes/`) does the binary work (SARC/Yaz0/BNTX/BFLYT) behind a single narrow facade. A thin app-owned layer (`cfw_paths`, `theme_install`, `active_theme_store`, `reboot`) picks base layouts from `/themes/systemData`, writes outputs to the CFW LayeredFS tree, persists which theme is active in our own JSON, and drives the detail-screen button state machine.

**Tech Stack:** C++17, libnx/devkitPro (Switch) + g++ (desktop), Borealis UI, nlohmann/json (vendored at `lib/json`), doctest. New vendored single-headers: `stb_image.h`, `miniz`.

**Spec:** `docs/superpowers/specs/2026-06-04-native-theme-apply-design.md`

**Branch:** `feat/themes-browser` (continues the themes work).

**Commit identity (MANDATORY for every commit):** `luizfbalves <luizzbanndera@gmail.com>`. Use:
`git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit ...`

---

## File Structure

| File | Responsibility |
|---|---|
| `LICENSE` | **Replace** MIT → GPLv2 |
| `THIRD_PARTY.md` | **New** — attribution for exelix11 engine (GPLv2), stb_image, miniz |
| `README.md` | License section: MIT → GPLv2 |
| `lib/switchthemes/**` | **New** — vendored exelix C++ engine subset (apply only) |
| `lib/switchthemes/apply_facade.{hpp,cpp}` | **New** — the ONLY engine entry point the app calls |
| `lib/switchthemes/README.md` | **New** — upstream origin + pinned commit + GPLv2 note |
| `source/platform/themes/cfw_paths.{hpp,cpp}` | **New** — LayeredFS root, base dir, target→(title,szs) map, base-presence check |
| `source/platform/themes/active_theme_store.{hpp,cpp}` | **New** — persist/read `/themes/.thomaz_active.json` |
| `source/platform/themes/theme_install.{hpp,cpp}` | **New** — `base_layouts_available` / `install` / `remove_active` |
| `source/platform/themes/theme_download.hpp` | **Modify** — expose `nxtheme_filename()` for reuse |
| `source/platform/system/reboot.{hpp,cpp}` | **New** — `reboot_to_payload()` (spsm / desktop no-op) |
| `source/app/theme_detail_activity.{hpp,cpp}` | **Modify** — button state machine + apply/remove/reboot/base-missing dialogs |
| `source/app/theme_browser_activity.cpp` | **Modify** — "Aplicado" badge from `active_theme_store` |
| `resources/xml/activity/theme_detail.xml` | **Modify** — give the action button's label an id |
| `resources/i18n/{en-US,pt-BR}/themes.json` | **Modify** — apply/remove/reboot/base keys |
| `CMakeLists.txt` | **Modify** — add `lib/switchthemes` include + sources |
| `tests/Makefile` | **Modify** — add `cfw_paths.cpp`, `active_theme_store.cpp` to SRCS |
| `tests/test_cfw_paths.cpp` | **New** — path/map/base-presence tests |
| `tests/test_active_theme_store.cpp` | **New** — JSON round-trip tests |
| `.gitignore` | **Modify** — allow new tracked files if needed |

**Key seam:** the app never touches engine internals. It calls exactly one function:

```cpp
// lib/switchthemes/apply_facade.hpp
namespace switchthemes {
struct ApplyOutput {
    bool ok = false;
    std::string error;                  // human message when !ok
    std::vector<unsigned char> szs;     // patched output when ok
    std::vector<std::string> warnings;  // incompatible parts dropped by the engine
};
// base_szs = original firmware layout bytes; nxtheme = .nxtheme file bytes.
ApplyOutput apply_nxtheme(const std::vector<unsigned char>& base_szs,
                          const std::vector<unsigned char>& nxtheme);
}
```

Everything testable-without-the-engine lives in `cfw_paths`/`active_theme_store` (in the unit build). `theme_install` and the facade are validated by compile gates + hardware.

---

## Task 1: Relicense MIT → GPLv2 + third-party attribution

**Files:**
- Modify: `LICENSE`
- Create: `THIRD_PARTY.md`
- Modify: `README.md:164-166`

Do this first: attribution must be in place before any GPLv2 engine code lands.

- [ ] **Step 1: Replace LICENSE with GPLv2**

Fetch the canonical GNU GPL v2 text and write it verbatim to `LICENSE`:

```bash
curl -fsSL https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt -o LICENSE
head -2 LICENSE
```
Expected: prints `                    GNU GENERAL PUBLIC LICENSE` / `                       Version 2, June 1991`.

- [ ] **Step 2: Create THIRD_PARTY.md**

```markdown
# Third-Party Components

## SwitchThemeInjector (theme engine)
- Source: https://github.com/exelix11/SwitchThemeInjector
- Files: vendored under `lib/switchthemes/` (the C++ `SwitchThemesCommon` apply subset)
- License: GNU GPL v2 — see `LICENSE`.
- This project is distributed under GPLv2 because it links this engine.

## stb_image
- Source: https://github.com/nothings/stb (`stb_image.h`)
- File: `lib/switchthemes/third_party/stb_image.h`
- License: Public Domain / MIT (dual). See header.

## miniz
- Source: https://github.com/richgel999/miniz
- Files: `lib/switchthemes/third_party/miniz.{h,c}`
- License: MIT. See header.

## nlohmann/json
- Source: https://github.com/nlohmann/json
- File: `lib/json/nlohmann/json.hpp`
- License: MIT.
```

- [ ] **Step 3: Update README license section**

In `README.md`, replace the license line:

```markdown
## 📄 Licença

Distribuído sob a **GNU GPL v2** — veja [`LICENSE`](LICENSE) e
[`THIRD_PARTY.md`](THIRD_PARTY.md). Este projeto era MIT até a v0.4; passou a
GPLv2 ao incorporar a engine de temas do
[exelix11/SwitchThemeInjector](https://github.com/exelix11/SwitchThemeInjector).
```

- [ ] **Step 4: Verify**

```bash
grep -q "Version 2, June 1991" LICENSE && grep -q "GPL v2" README.md && test -f THIRD_PARTY.md && echo OK
```
Expected: `OK`.

- [ ] **Step 5: Commit**

```bash
git add LICENSE THIRD_PARTY.md README.md
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "chore(license): relicense MIT -> GPLv2 for theme engine port"
```

---

## Task 2: `cfw_paths` — LayeredFS paths, target map, base presence

**Files:**
- Create: `source/platform/themes/cfw_paths.hpp`
- Create: `source/platform/themes/cfw_paths.cpp`
- Create: `tests/test_cfw_paths.cpp`
- Modify: `tests/Makefile:3`

Pure path/map logic + filesystem existence. The applet-target strings match the Themezer targets already used in `theme_browser_activity.cpp` (`ResidentMenu`, `Entrance`, …).

- [ ] **Step 1: Write the header**

`source/platform/themes/cfw_paths.hpp`:

```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

// CFW LayeredFS "contents" root. Switch: Atmosphère's
// /atmosphere/contents; desktop: a local "themes-out/contents" for smoke tests.
std::string layeredfs_root();

// Folder holding pre-extracted base firmware layouts (.szs). Switch:
// /themes/systemData (NXThemes Installer writes them there); desktop: local.
std::string base_layout_dir();

// A theme target maps to a system title + the SZS file inside its romfs/lyt.
struct TargetMap {
    std::string title_id;  // 16-hex, e.g. "0100000000001000"
    std::string szs;       // e.g. "ResidentMenu.szs"
};

// Map a Themezer applet target ("ResidentMenu", "Entrance", "Flaunch", "Set",
// "Notification", "Psl", "MyPage") to its title + szs. nullopt if unknown/empty.
std::optional<TargetMap> target_map(const std::string& target);

// base_layout_dir()/<szs>  — where the original layout must already exist.
// Empty string if target is unknown.
std::string base_szs_path(const std::string& target);

// layeredfs_root()/<title>/romfs/lyt/<szs>  — where we write the patched szs.
// Empty string if target is unknown.
std::string output_szs_path(const std::string& target);

// True only if EVERY target is known AND its base szs already exists on disk.
bool base_present_for(const std::vector<std::string>& targets);

} // namespace thomaz
```

- [ ] **Step 2: Write the failing tests**

`tests/test_cfw_paths.cpp`:

```cpp
#include "doctest.h"
#include "platform/themes/cfw_paths.hpp"
#include <filesystem>

using namespace thomaz;

TEST_CASE("target_map covers the known applet targets") {
    CHECK(target_map("ResidentMenu")->title_id == "0100000000001000");
    CHECK(target_map("ResidentMenu")->szs == "ResidentMenu.szs");
    CHECK(target_map("MyPage")->title_id == "0100000000001013");
    CHECK(target_map("Psl")->title_id == "0100000000001007");
    CHECK(target_map("Entrance")->szs == "Entrance.szs");
    CHECK_FALSE(target_map("").has_value());
    CHECK_FALSE(target_map("Bogus").has_value());
}

TEST_CASE("output_szs_path is the LayeredFS lyt path") {
    std::string p = output_szs_path("ResidentMenu");
    CHECK(p == layeredfs_root() + "/0100000000001000/romfs/lyt/ResidentMenu.szs");
    CHECK(output_szs_path("Bogus").empty());
}

TEST_CASE("base_szs_path joins the base dir") {
    CHECK(base_szs_path("Set") == base_layout_dir() + "/Set.szs");
}

TEST_CASE("base_present_for requires every base file to exist") {
    namespace fs = std::filesystem;
    fs::create_directories(base_layout_dir());
    fs::remove(base_szs_path("Entrance"));
    fs::remove(base_szs_path("Set"));

    CHECK_FALSE(base_present_for({"Entrance"}));

    std::ofstream(base_szs_path("Entrance")) << "x";
    CHECK(base_present_for({"Entrance"}));
    CHECK_FALSE(base_present_for({"Entrance", "Set"}));   // Set missing
    CHECK_FALSE(base_present_for({"Bogus"}));             // unknown target

    fs::remove(base_szs_path("Entrance"));
}
```

Add `#include <fstream>` at the top as well (used above):

```cpp
#include <fstream>
```

- [ ] **Step 3: Add the new source to the test Makefile**

In `tests/Makefile`, line 3 (`SRCS := ...`), append after `../source/platform/themes/theme_paths.cpp` exactly:

```
../source/platform/themes/cfw_paths.cpp
```

(`active_theme_store.cpp` is added in Task 3, not here — adding it now would break the build since that file doesn't exist yet.)

- [ ] **Step 4: Run tests, verify they fail**

```bash
cd tests && make clean && make test 2>&1 | tail -20
```
Expected: compile error (no `cfw_paths.cpp`) or link error for `target_map`.

- [ ] **Step 5: Implement `cfw_paths.cpp`**

`source/platform/themes/cfw_paths.cpp`:

```cpp
#include "platform/themes/cfw_paths.hpp"
#include <sys/stat.h>

namespace thomaz {

std::string layeredfs_root() {
#ifdef __SWITCH__
    return "/atmosphere/contents";
#else
    return "themes-out/contents";
#endif
}

std::string base_layout_dir() {
#ifdef __SWITCH__
    return "/themes/systemData";
#else
    return "themes/systemData";
#endif
}

std::optional<TargetMap> target_map(const std::string& target) {
    // qlaunch (home menu) hosts most layouts; MyPage/Psl are separate titles.
    if (target == "ResidentMenu") return TargetMap{"0100000000001000", "ResidentMenu.szs"};
    if (target == "Entrance")     return TargetMap{"0100000000001000", "Entrance.szs"};
    if (target == "Flaunch")      return TargetMap{"0100000000001000", "Flaunch.szs"};
    if (target == "Set")          return TargetMap{"0100000000001000", "Set.szs"};
    if (target == "Notification") return TargetMap{"0100000000001000", "Notification.szs"};
    if (target == "Psl")          return TargetMap{"0100000000001007", "Psl.szs"};
    if (target == "MyPage")       return TargetMap{"0100000000001013", "MyPage.szs"};
    return std::nullopt;
}

std::string base_szs_path(const std::string& target) {
    auto m = target_map(target);
    if (!m) return "";
    return base_layout_dir() + "/" + m->szs;
}

std::string output_szs_path(const std::string& target) {
    auto m = target_map(target);
    if (!m) return "";
    return layeredfs_root() + "/" + m->title_id + "/romfs/lyt/" + m->szs;
}

bool base_present_for(const std::vector<std::string>& targets) {
    if (targets.empty()) return false;
    for (const auto& t : targets) {
        std::string p = base_szs_path(t);
        if (p.empty()) return false;             // unknown target
        struct stat st;
        if (::stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
    }
    return true;
}

} // namespace thomaz
```

- [ ] **Step 6: Run tests, verify pass**

```bash
cd tests && make test 2>&1 | tail -20
```
Expected: all assertions pass (existing suite count grows; no failures).

- [ ] **Step 7: Commit**

```bash
git add source/platform/themes/cfw_paths.hpp source/platform/themes/cfw_paths.cpp tests/test_cfw_paths.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): cfw_paths — LayeredFS target map + base presence"
```

---

## Task 3: `active_theme_store` — persist the applied theme

**Files:**
- Create: `source/platform/themes/active_theme_store.hpp`
- Create: `source/platform/themes/active_theme_store.cpp`
- Create: `tests/test_active_theme_store.cpp`
- Modify: `tests/Makefile:3`

Persists which theme is currently applied, in `themes_root()/.thomaz_active.json`. Native install carries no theme identity, so this is our source of truth for the "Aplicado" badge and the Remover button.

- [ ] **Step 1: Write the header**

`source/platform/themes/active_theme_store.hpp`:

```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

// The theme currently applied to the console (as recorded by us).
struct ActiveTheme {
    std::string hex_id;
    std::string name;
    std::string author;
    std::vector<std::string> targets;  // applet targets we wrote, e.g. {"ResidentMenu"}
};

// Read themes_root()/.thomaz_active.json; nullopt if absent/malformed.
std::optional<ActiveTheme> get_active_theme();

// Write/overwrite the active-theme record.
void set_active_theme(const ActiveTheme& t);

// Remove the record (after a theme is removed).
void clear_active_theme();

// True if `entry` is the currently-applied theme (hex_id match).
bool is_active_theme(const thomaz::core::ThemeEntry& entry);

} // namespace thomaz
```

- [ ] **Step 2: Write the failing tests**

`tests/test_active_theme_store.cpp`:

```cpp
#include "doctest.h"
#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_paths.hpp"
#include <filesystem>

using namespace thomaz;
using thomaz::core::ThemeEntry;

TEST_CASE("active theme round-trips and clears") {
    namespace fs = std::filesystem;
    fs::create_directories(themes_root());
    clear_active_theme();
    CHECK_FALSE(get_active_theme().has_value());

    ActiveTheme t;
    t.hex_id = "A24";
    t.name = "Purple Skies";
    t.author = "Hsushi";
    t.targets = {"ResidentMenu", "Entrance"};
    set_active_theme(t);

    auto got = get_active_theme();
    REQUIRE(got.has_value());
    CHECK(got->hex_id == "A24");
    CHECK(got->name == "Purple Skies");
    CHECK(got->targets.size() == 2);
    CHECK(got->targets[1] == "Entrance");

    clear_active_theme();
    CHECK_FALSE(get_active_theme().has_value());
}

TEST_CASE("is_active_theme matches by hex_id") {
    clear_active_theme();
    ThemeEntry e; e.hex_id = "FF0"; e.name = "X";
    CHECK_FALSE(is_active_theme(e));

    ActiveTheme t; t.hex_id = "FF0"; t.name = "X";
    set_active_theme(t);
    CHECK(is_active_theme(e));

    ThemeEntry other; other.hex_id = "111";
    CHECK_FALSE(is_active_theme(other));

    clear_active_theme();
}
```

- [ ] **Step 3: Add source to the test Makefile**

In `tests/Makefile` line 3, append after the `cfw_paths.cpp` you added in Task 2:
```
../source/platform/themes/active_theme_store.cpp
```

- [ ] **Step 4: Run tests, verify they fail**

```bash
cd tests && make clean && make test 2>&1 | tail -20
```
Expected: link error for `get_active_theme`/`set_active_theme`.

- [ ] **Step 5: Implement `active_theme_store.cpp`**

`source/platform/themes/active_theme_store.cpp`:

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

void clear_active_theme() {
    ::remove(active_path().c_str());
}

bool is_active_theme(const thomaz::core::ThemeEntry& entry) {
    auto a = get_active_theme();
    return a && !a->hex_id.empty() && a->hex_id == entry.hex_id;
}

} // namespace thomaz
```

- [ ] **Step 6: Run tests, verify pass**

```bash
cd tests && make test 2>&1 | tail -20
```
Expected: all pass.

- [ ] **Step 7: Commit**

```bash
git add source/platform/themes/active_theme_store.hpp source/platform/themes/active_theme_store.cpp tests/test_active_theme_store.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): active_theme_store — persist applied theme as JSON"
```

---

## Task 4: `reboot` — reboot-to-payload shim

**Files:**
- Create: `source/platform/system/reboot.hpp`
- Create: `source/platform/system/reboot.cpp`

Not unit-tested (platform call). Verified by the main build compiling. Mirrors upstream `PlatformReboot` (spsm).

- [ ] **Step 1: Write the header**

`source/platform/system/reboot.hpp`:

```cpp
#pragma once

namespace thomaz {

// Reboot the console (reboot-to-payload via spsm) so LayeredFS theme changes
// take effect. No-op on desktop builds.
void reboot_to_payload();

} // namespace thomaz
```

- [ ] **Step 2: Implement `reboot.cpp`**

`source/platform/system/reboot.cpp`:

```cpp
#include "platform/system/reboot.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace thomaz {

void reboot_to_payload() {
#ifdef __SWITCH__
    // spsmShutdown(true) reboots; matches NXThemes Installer's PlatformReboot.
    spsmInitialize();
    spsmShutdown(true);
    spsmExit();
#endif
    // desktop: intentionally nothing.
}

} // namespace thomaz
```

- [ ] **Step 3: Verify desktop compile**

```bash
g++ -std=c++17 -fsyntax-only -Isource source/platform/system/reboot.cpp && echo OK
```
Expected: `OK`.

- [ ] **Step 4: Commit**

```bash
git add source/platform/system/reboot.hpp source/platform/system/reboot.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): reboot_to_payload shim (spsm / desktop no-op)"
```

---

## Task 5: Vendor the exelix engine + apply_facade + build wiring

**Files:**
- Create: `lib/switchthemes/**` (vendored upstream C++ subset)
- Create: `lib/switchthemes/third_party/{stb_image.h,miniz.h,miniz.c}`
- Create: `lib/switchthemes/apply_facade.hpp`
- Create: `lib/switchthemes/apply_facade.cpp`
- Create: `lib/switchthemes/README.md`
- Modify: `CMakeLists.txt:64-67,95`

> **Nature of this task:** this is a *vendoring + adaptation* task, not a from-scratch TDD task. You are importing ~8–12k lines of proven upstream C++ and exposing ONE function. Do not rewrite the engine. The "implementation" is: copy the right files, drop the firmware-extraction glue, and write `apply_facade.cpp` by mirroring upstream's `NxEntry::DoInstall` apply path. The gate is **it compiles on desktop g++ AND Switch**, and the facade contract holds.

Reference (already researched) upstream layout in `github.com/exelix11/SwitchThemeInjector`:
`SwitchThemesNX/source/SwitchThemesCommon/` (the C++ engine), `SwitchThemesNX/source/Pages/ThemeEntry/NxEntry.cpp` (apply orchestration), `SwitchThemesNX/source/fs.cpp` (paths — we do NOT use this; `cfw_paths` replaces it).

- [ ] **Step 1: Clone upstream at a pinned commit**

```bash
mkdir -p /tmp/sti && git clone --depth 1 https://github.com/exelix11/SwitchThemeInjector /tmp/sti/src
cd /tmp/sti/src && git rev-parse HEAD
```
Record the printed commit hash — it goes in `lib/switchthemes/README.md`.

- [ ] **Step 2: Copy the engine apply subset**

Copy the whole C++ common engine, then remove the firmware-extraction glue we are not using:

```bash
cd /home/solid/www/personal/playground/thomas
mkdir -p lib/switchthemes lib/switchthemes/third_party
cp -r /tmp/sti/src/SwitchThemesNX/source/SwitchThemesCommon/* lib/switchthemes/
# Keep: SarcLib/ Yaz0.* Bntx/ Layouts/ Patcher.* Common.* NXTheme.* MyTypes.h
#       Generated/ (DefaultTemplates) Fonts/ (only if referenced)
# Remove anything pulling hactool/keys/romfs cache if present in this dir:
rm -f lib/switchthemes/RomfsCache.* lib/switchthemes/hactool.* lib/switchthemes/key_loader.* 2>/dev/null || true
ls lib/switchthemes
```
Expected: lists `Common.cpp Common.hpp NXTheme.cpp Patcher.* SarcLib Yaz0.* Bntx Layouts Generated ...`.

> If upstream bundles its own `json.hpp` inside this tree, delete it and rely on our `lib/json/nlohmann/json.hpp` (add `-Ilib/json` — already global). If the engine `#include`s `"json.hpp"` (quoted, local), either keep upstream's copy under `lib/switchthemes/` OR adjust includes to `<nlohmann/json.hpp>`. Pick whichever compiles with least churn; note the choice in `lib/switchthemes/README.md`.

- [ ] **Step 3: Vendor stb_image + miniz**

```bash
curl -fsSL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h \
  -o lib/switchthemes/third_party/stb_image.h
curl -fsSL https://raw.githubusercontent.com/richgel999/miniz/master/miniz.h \
  -o lib/switchthemes/third_party/miniz.h
curl -fsSL https://raw.githubusercontent.com/richgel999/miniz/master/miniz.c \
  -o lib/switchthemes/third_party/miniz.c
ls lib/switchthemes/third_party
```
Expected: `miniz.c miniz.h stb_image.h`.

> Upstream uses SOIL2/stb for JPG/PNG decode and a zip lib for modern `.nxtheme`. If upstream's `NXTheme.cpp` references SOIL2 or its own zip wrapper, adapt those `#include`s to the vendored `stb_image.h` / `miniz.h`. The image path is only hit when a `.nxtheme` ships `image.jpg`/`*.png` instead of pre-encoded `image.dds`; keep it working.

- [ ] **Step 4: Write the facade header**

`lib/switchthemes/apply_facade.hpp`:

```cpp
#pragma once
#include <string>
#include <vector>

namespace switchthemes {

struct ApplyOutput {
    bool ok = false;
    std::string error;                  // human message when !ok
    std::vector<unsigned char> szs;     // patched output when ok
    std::vector<std::string> warnings;  // incompatible parts the engine dropped
};

// Apply a .nxtheme differential onto a base firmware layout, returning the
// patched SZS bytes. base_szs/nxtheme are raw file contents.
ApplyOutput apply_nxtheme(const std::vector<unsigned char>& base_szs,
                          const std::vector<unsigned char>& nxtheme);

} // namespace switchthemes
```

- [ ] **Step 5: Write the facade implementation by mirroring `NxEntry::DoInstall`**

Read `/tmp/sti/src/SwitchThemesNX/source/Pages/ThemeEntry/NxEntry.cpp` and
`.../SwitchThemesCommon/Patcher.hpp` to get the exact class/method names, then
translate the apply portion into `lib/switchthemes/apply_facade.cpp`. The shape
(method names per upstream `SzsPatcher` — confirm against the headers you copied):

```cpp
#include "apply_facade.hpp"
#include "NXTheme.hpp"
#include "Patcher.hpp"
#include "Common.hpp"

namespace switchthemes {

ApplyOutput apply_nxtheme(const std::vector<unsigned char>& base_szs,
                          const std::vector<unsigned char>& nxtheme) {
    ApplyOutput out;
    try {
        // 1. Parse the .nxtheme container (ZIP for v>=17, else Yaz0+SARC).
        auto theme = SwitchThemesCommon::NXTheme::LoadTheme(nxtheme);   // adjust to actual API

        // 2. Build a patcher over the base SZS.
        SwitchThemesCommon::SzsPatcher patcher(base_szs);              // adjust to actual ctor

        // 3. Apply background image (BNTX) + layout (BFLYT/BFLAN) patches.
        //    Mirror the exact call sequence in NxEntry::DoInstall.
        patcher.PatchMainBG(theme);          // <-- replace with the real calls
        patcher.PatchLayouts(theme);         // <-- replace with the real calls

        // 4. Collect compatibility warnings the engine surfaced.
        for (const auto& w : patcher.GetWarnings()) out.warnings.push_back(w);

        // 5. Serialize the patched SZS.
        out.szs = patcher.GetFinalSZS();     // <-- replace with the real call
        out.ok  = true;
    } catch (const std::exception& e) {
        out.ok = false;
        out.error = e.what();
    } catch (...) {
        out.ok = false;
        out.error = "unknown theme engine error";
    }
    return out;
}

} // namespace switchthemes
```

> The method names above are placeholders for the real `SzsPatcher` API — **you must replace them** with the actual methods found in the copied `Patcher.hpp`/`NxEntry.cpp`. The facade body is the only place upstream API knowledge is needed; keep all of it here.

- [ ] **Step 6: Write `lib/switchthemes/README.md`**

```markdown
# Vendored: SwitchThemeInjector C++ engine (apply subset)

- Upstream: https://github.com/exelix11/SwitchThemeInjector
- Pinned commit: <PASTE HASH FROM STEP 1>
- Imported path: SwitchThemesNX/source/SwitchThemesCommon/
- License: GPL v2 (see repo root LICENSE / our LICENSE).
- Removed: firmware extraction glue (hactool/mbedtls/key_loader/RomfsCache) and
  upstream fs.cpp — base layouts come from /themes/systemData via our cfw_paths;
  install paths via our theme_install.
- Entry point used by the app: apply_facade.{hpp,cpp} (only).
- json: <note whether using lib/json or upstream's bundled json.hpp>.
- images/zip: third_party/stb_image.h, third_party/miniz.{h,c}.
```

- [ ] **Step 7: Wire the engine into CMake**

In `CMakeLists.txt`, after line 66 (the `lib/json` include) add the engine include dir:

```cmake
list(APPEND APP_PLATFORM_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/json
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/switchthemes
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/switchthemes/third_party)
```

After line 67 (`file(GLOB_RECURSE MAIN_SRC ...)`) add the engine sources (C and C++):

```cmake
file(GLOB_RECURSE ENGINE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/switchthemes/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/switchthemes/*.c)
list(APPEND MAIN_SRC ${ENGINE_SRC})
```

- [ ] **Step 8: Desktop build (compile gate)**

```bash
cd /home/solid/www/personal/playground/thomas
cmake -B build-desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON >/dev/null
cmake --build build-desktop -j"$(nproc)" 2>&1 | tail -30
```
Expected: links to a `thomaz` binary. Fix include/namespace mismatches in the
vendored tree until it compiles. (Engine is pure C++; desktop is the fast gate.)

- [ ] **Step 9: Switch build (compile gate)**

```bash
cmake -B build-switch >/dev/null && cmake --build build-switch -j"$(nproc)" 2>&1 | tail -30
```
Expected: produces `thomaz.elf`/`.nro` with no undefined references.

- [ ] **Step 10: Commit**

```bash
git add lib/switchthemes CMakeLists.txt
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): vendor exelix theme engine (GPLv2) behind apply_facade"
```

---

## Task 6: `theme_install` — availability, install, remove

**Files:**
- Create: `source/platform/themes/theme_install.hpp`
- Create: `source/platform/themes/theme_install.cpp`
- Modify: `source/platform/themes/theme_download.hpp`

Integrates facade + cfw_paths + active_theme_store. Not in the unit build (links the engine); validated by the Task 5 builds + hardware. `base_layouts_available` is a thin wrapper over the unit-tested `cfw_paths::base_present_for`.

- [ ] **Step 1: Expose the downloaded-file naming for reuse**

In `source/platform/themes/theme_download.hpp`, add the declaration (the impl already exists privately in `theme_download.cpp` as `part_filename`; rename/expose it):

```cpp
// "<target>.nxtheme" (or "theme<index>.nxtheme" when target is empty) — the
// on-SD filename used for a downloaded part. Shared with theme_install.
std::string nxtheme_filename(const thomaz::core::ThemePart& part, int index);
```

In `source/platform/themes/theme_download.cpp`, replace the anonymous-namespace
`part_filename` with the public `thomaz::nxtheme_filename` (same body) and update
its one call site in `download_theme`.

- [ ] **Step 2: Write the header**

`source/platform/themes/theme_install.hpp`:

```cpp
#pragma once
#include <string>
#include <vector>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

struct InstallResult {
    bool ok = false;
    std::string error;                  // human message when !ok
    std::vector<std::string> warnings;  // engine-dropped incompatible parts
};

// True if every applet target this detail needs has a base layout on the SD.
bool base_layouts_available(const thomaz::core::ThemeDetail& detail);

// Apply every part of `detail`: read its .nxtheme + base szs, patch, write to
// LayeredFS, write fsmitm.flag markers, and record the active theme. Rolls back
// files written in this call on any hard failure.
InstallResult install_theme(const thomaz::core::ThemeDetail& detail);

// Delete the LayeredFS outputs recorded as active and clear active state.
InstallResult remove_active_theme();

} // namespace thomaz
```

- [ ] **Step 3: Implement `theme_install.cpp`**

`source/platform/themes/theme_install.cpp`:

```cpp
#include "platform/themes/theme_install.hpp"
#include "platform/themes/cfw_paths.hpp"
#include "platform/themes/theme_paths.hpp"
#include "platform/themes/theme_download.hpp"   // nxtheme_filename
#include "platform/themes/active_theme_store.hpp"
#include "apply_facade.hpp"

#include <fstream>
#include <sys/stat.h>
#include <cstdio>

namespace thomaz {

namespace {

std::vector<std::string> detail_targets(const thomaz::core::ThemeDetail& d) {
    std::vector<std::string> ts;
    for (const auto& p : d.parts)
        if (!p.target.empty()) ts.push_back(p.target);
    return ts;
}

bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const std::string& path, const std::vector<unsigned char>& data) {
    std::ofstream o(path, std::ios::binary | std::ios::trunc);
    if (!o) return false;
    if (!data.empty()) o.write((const char*)data.data(), (std::streamsize)data.size());
    return (bool)o;
}

// mkdir -p for the parent dirs of a file path (POSIX, FAT-safe).
void ensure_parent_dirs(const std::string& file) {
    std::string acc;
    for (size_t i = 0; i < file.size(); ++i) {
        acc.push_back(file[i]);
        if (file[i] == '/' && acc.size() > 1) ::mkdir(acc.c_str(), 0777);
    }
}

} // namespace

bool base_layouts_available(const thomaz::core::ThemeDetail& detail) {
    return base_present_for(detail_targets(detail));
}

InstallResult install_theme(const thomaz::core::ThemeDetail& detail) {
    InstallResult res;
    auto targets = detail_targets(detail);
    if (targets.empty()) { res.error = "theme has no installable parts"; return res; }
    if (!base_present_for(targets)) { res.error = "base layouts missing"; return res; }

    std::string folder = theme_folder(detail.entry);
    std::vector<std::string> written;   // for rollback
    std::vector<std::string> applied_targets;

    int index = 0;
    for (const auto& part : detail.parts) {
        const int i = index++;
        if (part.target.empty()) continue;

        std::string nx_path  = folder + "/" + nxtheme_filename(part, i);
        std::string base     = base_szs_path(part.target);
        std::string out_path = output_szs_path(part.target);

        std::vector<unsigned char> nx_bytes, base_bytes;
        if (!read_file(nx_path, nx_bytes)) {
            for (const auto& w : written) ::remove(w.c_str());
            res.error = "missing downloaded file: " + nx_path;
            return res;
        }
        if (!read_file(base, base_bytes)) {
            for (const auto& w : written) ::remove(w.c_str());
            res.error = "missing base layout: " + base;
            return res;
        }

        switchthemes::ApplyOutput ao =
            switchthemes::apply_nxtheme(base_bytes, nx_bytes);
        for (const auto& w : ao.warnings) res.warnings.push_back(part.target + ": " + w);
        if (!ao.ok) {
            for (const auto& w : written) ::remove(w.c_str());
            res.error = "engine: " + ao.error;
            return res;
        }

        ensure_parent_dirs(out_path);
        if (!write_file(out_path, ao.szs)) {
            for (const auto& w : written) ::remove(w.c_str());
            res.error = "could not write " + out_path;
            return res;
        }
        written.push_back(out_path);
        applied_targets.push_back(part.target);

        // Legacy FS-MITM marker (harmless on modern Atmosphère).
        auto m = target_map(part.target);
        if (m) {
            std::string flag = layeredfs_root() + "/" + m->title_id + "/fsmitm.flag";
            ensure_parent_dirs(flag);
            std::ofstream(flag).put('\0');
        }
    }

    ActiveTheme at;
    at.hex_id  = detail.entry.hex_id;
    at.name    = detail.entry.name;
    at.author  = detail.entry.author;
    at.targets = applied_targets;
    set_active_theme(at);

    res.ok = true;
    return res;
}

InstallResult remove_active_theme() {
    InstallResult res;
    auto active = get_active_theme();
    if (!active) { res.ok = true; return res; }   // nothing to remove

    for (const auto& t : active->targets) {
        std::string out = output_szs_path(t);
        if (!out.empty()) ::remove(out.c_str());
    }
    clear_active_theme();
    res.ok = true;
    return res;
}

} // namespace thomaz
```

- [ ] **Step 4: Compile gate (desktop + Switch)**

```bash
cmake --build build-desktop -j"$(nproc)" 2>&1 | tail -15
cmake --build build-switch -j"$(nproc)" 2>&1 | tail -15
```
Expected: both link cleanly (the new file is picked up by `GLOB_RECURSE`).

- [ ] **Step 5: Commit**

```bash
git add source/platform/themes/theme_install.hpp source/platform/themes/theme_install.cpp source/platform/themes/theme_download.hpp source/platform/themes/theme_download.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): theme_install — apply/remove via engine + LayeredFS"
```

---

## Task 7: i18n keys for apply/remove/reboot/base

**Files:**
- Modify: `resources/i18n/pt-BR/themes.json`
- Modify: `resources/i18n/en-US/themes.json`

- [ ] **Step 1: Add pt-BR keys**

In `resources/i18n/pt-BR/themes.json`, before the closing `}` (keep valid JSON — add a comma after the previous last entry), insert:

```json
    "apply": "Aplicar Tema",
    "apply_pack": "Aplicar Pack",
    "applying": "Aplicando tema…",
    "apply_ok": "Tema aplicado. Reinicie para ver as mudanças.",
    "apply_fail": "Falha ao aplicar",
    "remove": "Remover Tema",
    "removing": "Removendo tema…",
    "remove_ok": "Tema removido. Reinicie para voltar ao padrão.",
    "remove_fail": "Falha ao remover",
    "applied": "Aplicado",
    "warn_parts_removed": "Algumas partes incompatíveis foram removidas automaticamente.",
    "base_missing": "Layouts base não encontrados",
    "base_missing_help": "Você precisa extrair os layouts do firmware uma vez (ex.: com o NXThemes Installer) para sd:/themes/systemData antes de aplicar.",
    "reboot_prompt": "Reiniciar o console agora para aplicar?",
    "reboot_now": "Reiniciar",
    "reboot_later": "Depois"
```

- [ ] **Step 2: Add en-US keys**

In `resources/i18n/en-US/themes.json`, insert the matching keys:

```json
    "apply": "Apply Theme",
    "apply_pack": "Apply Pack",
    "applying": "Applying theme…",
    "apply_ok": "Theme applied. Reboot to see the changes.",
    "apply_fail": "Apply failed",
    "remove": "Remove Theme",
    "removing": "Removing theme…",
    "remove_ok": "Theme removed. Reboot to restore the default.",
    "remove_fail": "Remove failed",
    "applied": "Applied",
    "warn_parts_removed": "Some incompatible parts were removed automatically.",
    "base_missing": "Base layouts not found",
    "base_missing_help": "Extract the firmware layouts once (e.g. with NXThemes Installer) to sd:/themes/systemData before applying.",
    "reboot_prompt": "Reboot the console now to apply?",
    "reboot_now": "Reboot",
    "reboot_later": "Later"
```

- [ ] **Step 3: Validate JSON**

```bash
python3 -c "import json;[json.load(open(f)) for f in ['resources/i18n/pt-BR/themes.json','resources/i18n/en-US/themes.json']];print('OK')"
```
Expected: `OK`.

- [ ] **Step 4: Commit**

```bash
git add resources/i18n/pt-BR/themes.json resources/i18n/en-US/themes.json
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): i18n for apply/remove/reboot/base-missing"
```

---

## Task 8: Detail screen — button state machine + dialogs

**Files:**
- Modify: `resources/xml/activity/theme_detail.xml:37`
- Modify: `source/app/theme_detail_activity.hpp`
- Modify: `source/app/theme_detail_activity.cpp`

Turn the single "Baixar" button into Baixar → Aplicar/Aplicar Pack → Remover, with base-missing + reboot dialogs. Engine work runs on `brls::async`, guarded by the existing `alive` flag.

- [ ] **Step 1: Give the action button's label an id**

In `resources/xml/activity/theme_detail.xml`, line 37, change:

```xml
                <brls:Label text="@i18n/themes/download" fontSize="18" textColor="#FFFFFF"/>
```
to:
```xml
                <brls:Label id="downloadButtonLabel" text="@i18n/themes/download" fontSize="18" textColor="#FFFFFF"/>
```

- [ ] **Step 2: Extend the activity header**

In `source/app/theme_detail_activity.hpp`, add the new private methods/state (inside the class, after `void startDownload();`):

```cpp
    void refreshActionButton();   // sets label + which action the button performs
    void doApply();
    void doRemove();
    void showBaseMissingDialog();
    void showRebootDialog();

    bool downloaded = false;
    bool applied    = false;
    bool busy       = false;      // guards against double-taps during async work
```

Add the include near the top:

```cpp
#include "platform/themes/theme_install.hpp"
```

- [ ] **Step 3: Compute state on resolve and after download**

In `source/app/theme_detail_activity.cpp`, add includes:

```cpp
#include "platform/themes/theme_paths.hpp"
#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_install.hpp"
#include "platform/system/reboot.hpp"
```

At the end of `onResolved` (after the parts loop, only when `ok`), refresh state:

```cpp
    this->downloaded = theme_already_downloaded(d.entry);
    this->applied    = is_active_theme(d.entry);
    this->refreshActionButton();
```

In `startDownload`, in the success branch (where it currently notifies and pops),
replace the pop-on-success with a state refresh so the user can immediately
apply. Find the `brls::sync` success block and change it to:

```cpp
        brls::sync([this, alive, msg, ok]() {
            if (!alive->load()) return;
            brls::Application::notify(msg);
            if (ok) {
                this->downloaded = true;
                this->refreshActionButton();
            }
        });
```

- [ ] **Step 4: Implement the button state machine + actions**

Add to `source/app/theme_detail_activity.cpp` (anywhere in `namespace thomaz`):

```cpp
void ThemeDetailActivity::refreshActionButton() {
    auto* lbl = (brls::Label*)this->getView("downloadButtonLabel");
    if (!lbl) return;
    if (this->applied) {
        lbl->setText("themes/remove"_i18n);
    } else if (this->downloaded) {
        bool pack = (this->entry.kind == core::ThemeKind::Pack);
        lbl->setText(pack ? "themes/apply_pack"_i18n : "themes/apply"_i18n);
    } else {
        lbl->setText("themes/download"_i18n);
    }
}

void ThemeDetailActivity::doApply() {
    if (!this->resolved || this->busy) return;
    if (!base_layouts_available(this->detail)) { this->showBaseMissingDialog(); return; }

    this->busy = true;
    brls::Application::notify("themes/applying"_i18n);

    core::ThemeDetail d = this->detail;
    auto alive = this->alive;
    brls::async([this, d, alive]() {
        InstallResult r = install_theme(d);
        brls::sync([this, alive, r]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                brls::Application::notify("themes/apply_fail"_i18n + std::string(": ") + r.error);
                return;
            }
            if (!r.warnings.empty()) brls::Application::notify("themes/warn_parts_removed"_i18n);
            brls::Application::notify("themes/apply_ok"_i18n);
            this->applied = true;
            this->refreshActionButton();
            this->showRebootDialog();
        });
    });
}

void ThemeDetailActivity::doRemove() {
    if (this->busy) return;
    this->busy = true;
    brls::Application::notify("themes/removing"_i18n);

    auto alive = this->alive;
    brls::async([this, alive]() {
        InstallResult r = remove_active_theme();
        brls::sync([this, alive, r]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                brls::Application::notify("themes/remove_fail"_i18n + std::string(": ") + r.error);
                return;
            }
            brls::Application::notify("themes/remove_ok"_i18n);
            this->applied = false;
            this->refreshActionButton();
            this->showRebootDialog();
        });
    });
}

void ThemeDetailActivity::showBaseMissingDialog() {
    auto* dialog = new brls::Dialog("themes/base_missing_help"_i18n);
    dialog->addButton("themes/base_missing"_i18n, []() {});
    dialog->open();
}

void ThemeDetailActivity::showRebootDialog() {
    auto* dialog = new brls::Dialog("themes/reboot_prompt"_i18n);
    dialog->addButton("themes/reboot_now"_i18n, []() { thomaz::reboot_to_payload(); });
    dialog->addButton("themes/reboot_later"_i18n, []() {});
    dialog->open();
}
```

- [ ] **Step 5: Route the button to the right action**

In `onContentAvailable`, the existing `downloadButton` click handler calls
`startDownload()`. Replace its body to dispatch by state:

```cpp
    if (auto* btn = this->getView("downloadButton")) {
        btn->registerClickAction([this](brls::View*) {
            brls::sync([this]() {
                if (this->applied)        this->doRemove();
                else if (this->downloaded) this->doApply();
                else                       this->startDownload();
            });
            return true;
        });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }
```

- [ ] **Step 6: Build (desktop + Switch)**

```bash
cmake --build build-desktop -j"$(nproc)" 2>&1 | tail -15
cmake --build build-switch  -j"$(nproc)" 2>&1 | tail -15
```
Expected: both compile/link.

- [ ] **Step 7: Desktop boot smoke**

```bash
timeout 8 ./build-desktop/thomaz >/dev/null 2>&1; echo "exit=$?"
```
Expected: `exit=124` (ran 8s then killed = healthy boot, per project convention).

- [ ] **Step 8: Commit**

```bash
git add resources/xml/activity/theme_detail.xml source/app/theme_detail_activity.hpp source/app/theme_detail_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): detail button state machine (apply/remove + reboot)"
```

---

## Task 9: "Aplicado" badge in the browser grid + detail header

**Files:**
- Modify: `source/app/theme_browser_activity.cpp:177-189`

The downloaded badge already exists in the card meta. Add an "Aplicado" tag when the entry is the active theme.

- [ ] **Step 1: Include the store**

In `source/app/theme_browser_activity.cpp`, near the other platform includes (after `#include "platform/themes/theme_paths.hpp"`), add:

```cpp
#include "platform/themes/active_theme_store.hpp"
```

- [ ] **Step 2: Append the applied tag to the card meta**

In `populate()`, in the block that builds `std::string m` (currently appends
`[downloaded]`), after that `if (theme_already_downloaded(e)) { ... }` block, add:

```cpp
        if (is_active_theme(e)) {
            m += "  [";
            m += "themes/applied"_i18n;
            m += "]";
        }
```

- [ ] **Step 3: Build + smoke**

```bash
cmake --build build-desktop -j"$(nproc)" 2>&1 | tail -10
timeout 8 ./build-desktop/thomaz >/dev/null 2>&1; echo "exit=$?"
```
Expected: compiles; `exit=124`.

- [ ] **Step 4: Commit**

```bash
git add source/app/theme_browser_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(themes): show [Aplicado] badge for the active theme"
```

---

## Task 10: Final verification

- [ ] **Step 1: Full unit suite**

```bash
cd tests && make clean && make test 2>&1 | tail -15
```
Expected: all tests pass (Phase A suite + new cfw_paths/active_theme_store).

- [ ] **Step 2: Switch release build**

```bash
cd /home/solid/www/personal/playground/thomas
cmake --build build-switch -j"$(nproc)" 2>&1 | tail -15
ls build-switch/thomaz.nro
```
Expected: `.nro` produced.

- [ ] **Step 3: Hardware test checklist (manual, on console)**

  - With `/themes/systemData` populated: download a theme → button shows **Aplicar Tema** → apply → reboot prompt → reboot → home menu shows the theme.
  - Apply a **pack** → all parts applied → reboot → multiple screens themed.
  - Re-open the theme → button shows **Remover Tema**, card shows **[Aplicado]**.
  - Remove → reboot → home menu back to default.
  - With `/themes/systemData` empty: apply → **base_missing** dialog, nothing written.

  Document results in the PR description.

---

## Plan Self-Review

**Spec coverage:**
- Real native install (compile .nxtheme → .szs, write LayeredFS) → Tasks 5, 6.
- Engine port (exelix GPLv2, apply subset, no hactool) → Task 5.
- Relicense MIT→GPLv2 + attribution → Task 1.
- Base layouts from `/themes/systemData` → Tasks 2, 6.
- Apply themes + packs → Task 6 (`install_theme` loops parts), Task 8 (apply_pack label).
- Remove/restore → Task 6 (`remove_active_theme`), Task 8 (doRemove).
- Reboot prompt (spsm) → Tasks 4, 8.
- Active-theme indicator via our JSON → Tasks 3, 9.
- Missing-base dialog → Tasks 6, 8.
- i18n → Task 7.
- Build wiring + tests → Tasks 5 (CMake), 2/3 (Makefile).
- Error handling (rollback, warnings surfaced, async-guarded UI) → Task 6 (rollback), Task 8 (warn/notify on UI thread).

**Type consistency:** `ApplyOutput`/`apply_nxtheme` (Task 5) match calls in Task 6. `InstallResult`, `install_theme`, `remove_active_theme`, `base_layouts_available` (Task 6) match Task 8 calls. `ActiveTheme`, `get/set/clear/is_active_theme` (Task 3) match Tasks 6, 8, 9. `target_map`/`base_szs_path`/`output_szs_path`/`base_present_for`/`layeredfs_root`/`base_layout_dir` (Task 2) match Task 6. `nxtheme_filename` (Task 6 Step 1) matches its use in `install_theme`. `reboot_to_payload` (Task 4) matches Task 8.

**Known soft spots (validate during execution, not blockers):**
- Real `SzsPatcher` method names in `apply_facade.cpp` (Task 5 Step 5) must be taken from the copied headers — the shown names are placeholders to replace.
- `/themes/systemData` on-disk layout (flat `<Applet>.szs` vs per-title subdirs) — confirm on hardware; adjust `base_szs_path` if NXThemes uses subfolders.
- `fsmitm.flag`/`version_hash.bin` necessity on modern Atmosphère — we write `fsmitm.flag` (harmless) and skip `version_hash.bin`; verify a theme loads without it.
