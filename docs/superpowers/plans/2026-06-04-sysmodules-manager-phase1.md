# Sysmodules Manager — Phase 1 (Detection + Toggle + Reboot) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Sistema" card to the thomaz home that lists the sysmodules installed under `/atmosphere/contents/`, lets the user enable/disable each with one tap (boot2 flag), and offers a reboot when a change requires it. Works fully offline.

**Architecture:** Mirror the existing mods module. Pure, host-tested logic lives in `core/sysmod/` (path building, `toolbox.json` parsing, turning a raw directory listing into a typed sysmodule list). Real filesystem IO lives in `platform/sysmod/` as free functions that take explicit paths (host-testable in a temp dir, like `mod_store`), plus an `ISysmoduleStore` interface with a real POSIX implementation and a desktop fake. The Borealis `SystemActivity` consumes the interface.

**Tech Stack:** C++17, Borealis (UI), nlohmann/json (parsing), doctest (host tests), libnx (`reboot_to_payload` on Switch), CMake + a plain Makefile for the host test suite.

**Spec:** `docs/superpowers/specs/2026-06-04-sysmodules-overlays-manager-design.md`

**Commit identity:** commit as `luizfbalves <luizzbanndera@gmail.com>` (project convention), not the ambient git user.

---

## Key correctness note (read before starting)

`/atmosphere/contents/<id>/` holds **two unrelated things**:
1. **Sysmodules** — background services. They ship an `exefs.nsp` and are launched at boot when `flags/boot2.flag` exists.
2. **Game LayeredFS mods** — the thomaz mods feature writes `romfs/` here under a game's Title ID.

Phase 1 must list **only sysmodules**, never game mod folders. The discriminator is the presence of **`exefs.nsp`** in the folder. A romfs-only mod folder has no `exefs.nsp` and must be skipped.

## File structure

```
source/core/sysmod/
  sysmod_types.hpp        # RawSysmoduleEntry, Sysmodule, ToolboxInfo (no .cpp)
  sysmod_paths.{hpp,cpp}  # contents_root, contents_dir, flags_dir, boot2_flag_path
  toolbox_json.{hpp,cpp}  # parse_toolbox(raw) -> ToolboxInfo
  sysmod_scan.{hpp,cpp}   # build_sysmodule_list(entries) -> vector<Sysmodule>
source/platform/sysmod/
  sysmod_store.{hpp,cpp}      # scan_contents(root), set_boot2_flag(dir,bool); ISysmoduleStore + SysmoduleStore (POSIX)
  sysmod_store_fake.{hpp,cpp} # FakeSysmoduleStore (in-memory, desktop UI iteration)
  system_reboot.{hpp,cpp}     # reboot_to_payload() with fallback (Switch-guarded)
source/app/
  system_activity.{hpp,cpp}   # the "Sistema" screen
resources/xml/activity/system.xml
resources/i18n/{pt-BR,en-US}/system.json
tests/
  test_sysmod_paths.cpp
  test_toolbox_json.cpp
  test_sysmod_scan.cpp
  test_sysmod_store.cpp
```

Modified: `tests/Makefile` (add new core + store sources), `resources/xml/activity/home.xml` (add card), `source/app/home_activity.cpp` (wire card).

---

## Task 1: Core types

**Files:**
- Create: `source/core/sysmod/sysmod_types.hpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include <string>
#include <vector>

namespace thomaz::core {

// Parsed contents of a sysmodule's toolbox.json.
struct ToolboxInfo {
    std::string name;             // friendly name; empty if not parseable
    bool requires_reboot = true;  // safe default: assume a reboot is needed
    bool valid = false;           // true only if json parsed into an object
};

// One folder found under /atmosphere/contents, with the facts the pure
// scanner needs. The platform layer fills this in from the real filesystem.
struct RawSysmoduleEntry {
    std::string program_id;        // folder name (16-hex), used verbatim
    bool has_exefs = false;        // exefs.nsp present -> this is a sysmodule
    bool has_boot2_flag = false;   // flags/boot2.flag present -> enabled at boot
    std::string toolbox_json;      // raw toolbox.json contents, empty if none
};

// A sysmodule presented to the user.
struct Sysmodule {
    std::string program_id;       // 16-hex folder name
    std::string name;             // toolbox name, or program_id when absent
    bool requires_reboot = true;  // from toolbox, default true
    bool enabled = false;         // boot2 flag present
    bool has_metadata = false;    // a valid toolbox.json was found
};

} // namespace thomaz::core
```

- [ ] **Step 2: Commit**

```bash
git add source/core/sysmod/sysmod_types.hpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): core types for sysmodule detection"
```

---

## Task 2: Core paths

**Files:**
- Create: `source/core/sysmod/sysmod_paths.hpp`, `source/core/sysmod/sysmod_paths.cpp`
- Test: `tests/test_sysmod_paths.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "core/sysmod/sysmod_paths.hpp"

using namespace thomaz::core;

TEST_CASE("contents root is the Atmosphere contents directory") {
    CHECK(sysmod_contents_root() == "/atmosphere/contents");
}

TEST_CASE("contents dir appends the program id verbatim") {
    CHECK(sysmod_contents_dir("00FF0000636C6BFF")
          == "/atmosphere/contents/00FF0000636C6BFF");
}

TEST_CASE("flags dir and boot2 flag path are nested under the program dir") {
    CHECK(sysmod_flags_dir("00FF0000636C6BFF")
          == "/atmosphere/contents/00FF0000636C6BFF/flags");
    CHECK(sysmod_boot2_flag_path("00FF0000636C6BFF")
          == "/atmosphere/contents/00FF0000636C6BFF/flags/boot2.flag");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C tests test`
Expected: FAIL to compile — `core/sysmod/sysmod_paths.hpp` not found. (Makefile is updated in Task 5; for now this confirms the test references the new API.)

- [ ] **Step 3: Write the header**

```cpp
#pragma once
#include <string>

namespace thomaz::core {

// Root of Atmosphere's content overrides (sysmodules + LayeredFS mods).
std::string sysmod_contents_root();

// <root>/<program_id>  (program_id used verbatim, it is not a title id we own).
std::string sysmod_contents_dir(const std::string& program_id);

// <root>/<program_id>/flags
std::string sysmod_flags_dir(const std::string& program_id);

// <root>/<program_id>/flags/boot2.flag  — presence means "enabled at boot".
std::string sysmod_boot2_flag_path(const std::string& program_id);

} // namespace thomaz::core
```

- [ ] **Step 4: Write the implementation**

```cpp
#include "core/sysmod/sysmod_paths.hpp"

namespace thomaz::core {

std::string sysmod_contents_root() {
    return "/atmosphere/contents";
}

std::string sysmod_contents_dir(const std::string& program_id) {
    return sysmod_contents_root() + "/" + program_id;
}

std::string sysmod_flags_dir(const std::string& program_id) {
    return sysmod_contents_dir(program_id) + "/flags";
}

std::string sysmod_boot2_flag_path(const std::string& program_id) {
    return sysmod_flags_dir(program_id) + "/boot2.flag";
}

} // namespace thomaz::core
```

- [ ] **Step 5: Commit** (test will run green after Task 5 wires the Makefile)

```bash
git add source/core/sysmod/sysmod_paths.hpp source/core/sysmod/sysmod_paths.cpp tests/test_sysmod_paths.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): sysmodule SD path helpers"
```

---

## Task 3: Parse toolbox.json

**Files:**
- Create: `source/core/sysmod/toolbox_json.hpp`, `source/core/sysmod/toolbox_json.cpp`
- Test: `tests/test_toolbox_json.cpp`

The real toolbox.json looks like: `{"name":"sys-clk","tid":"00FF0000636C6BFF","requires_reboot":true}`.

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "core/sysmod/toolbox_json.hpp"

using namespace thomaz::core;

TEST_CASE("parses name and requires_reboot from a well-formed toolbox") {
    ToolboxInfo t = parse_toolbox(R"({"name":"sys-clk","requires_reboot":true})");
    CHECK(t.valid);
    CHECK(t.name == "sys-clk");
    CHECK(t.requires_reboot == true);
}

TEST_CASE("requires_reboot defaults to true when the field is absent") {
    ToolboxInfo t = parse_toolbox(R"({"name":"MissionControl"})");
    CHECK(t.valid);
    CHECK(t.name == "MissionControl");
    CHECK(t.requires_reboot == true);
}

TEST_CASE("respects requires_reboot=false") {
    ToolboxInfo t = parse_toolbox(R"({"name":"x","requires_reboot":false})");
    CHECK(t.requires_reboot == false);
}

TEST_CASE("empty / malformed / non-object input is not valid") {
    CHECK(parse_toolbox("").valid == false);
    CHECK(parse_toolbox("not json").valid == false);
    CHECK(parse_toolbox("[1,2,3]").valid == false);
}

TEST_CASE("wrong-typed fields fall back to defaults but stay valid") {
    ToolboxInfo t = parse_toolbox(R"({"name":123,"requires_reboot":"yes"})");
    CHECK(t.valid);
    CHECK(t.name == "");          // wrong type -> empty
    CHECK(t.requires_reboot == true); // wrong type -> default
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C tests test`
Expected: FAIL to compile — `toolbox_json.hpp` not found.

- [ ] **Step 3: Write the header**

```cpp
#pragma once
#include "core/sysmod/sysmod_types.hpp"
#include <string>

namespace thomaz::core {

// Parse a sysmodule's toolbox.json. Never throws. `valid` is true only when the
// input parses to a JSON object; individual wrong-typed fields fall back to
// defaults (name="", requires_reboot=true) without invalidating the result.
ToolboxInfo parse_toolbox(const std::string& raw);

} // namespace thomaz::core
```

- [ ] **Step 4: Write the implementation**

```cpp
#include "core/sysmod/toolbox_json.hpp"

#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

ToolboxInfo parse_toolbox(const std::string& raw) {
    ToolboxInfo info;
    json doc = json::parse(raw, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object())
        return info; // valid stays false

    info.valid = true;
    if (doc.contains("name") && doc["name"].is_string())
        info.name = doc["name"].get<std::string>();
    if (doc.contains("requires_reboot") && doc["requires_reboot"].is_boolean())
        info.requires_reboot = doc["requires_reboot"].get<bool>();
    return info;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Commit**

```bash
git add source/core/sysmod/toolbox_json.hpp source/core/sysmod/toolbox_json.cpp tests/test_toolbox_json.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): parse sysmodule toolbox.json"
```

---

## Task 4: Build the sysmodule list (pure scan)

**Files:**
- Create: `source/core/sysmod/sysmod_scan.hpp`, `source/core/sysmod/sysmod_scan.cpp`
- Test: `tests/test_sysmod_scan.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "core/sysmod/sysmod_scan.hpp"

using namespace thomaz::core;

TEST_CASE("a folder with exefs becomes a sysmodule; enabled reflects the flag") {
    std::vector<RawSysmoduleEntry> raw = {
        {"00FF0000636C6BFF", /*exefs*/ true, /*flag*/ true,
         R"({"name":"sys-clk","requires_reboot":true})"},
    };
    std::vector<Sysmodule> out = build_sysmodule_list(raw);
    REQUIRE(out.size() == 1);
    CHECK(out[0].program_id == "00FF0000636C6BFF");
    CHECK(out[0].name == "sys-clk");
    CHECK(out[0].enabled == true);
    CHECK(out[0].requires_reboot == true);
    CHECK(out[0].has_metadata == true);
}

TEST_CASE("a romfs-only mod folder (no exefs) is skipped") {
    std::vector<RawSysmoduleEntry> raw = {
        {"0100000000010000", /*exefs*/ false, /*flag*/ false, ""},
    };
    CHECK(build_sysmodule_list(raw).empty());
}

TEST_CASE("a sysmodule without toolbox falls back to its program id as name") {
    std::vector<RawSysmoduleEntry> raw = {
        {"4200000000000000", /*exefs*/ true, /*flag*/ false, ""},
    };
    std::vector<Sysmodule> out = build_sysmodule_list(raw);
    REQUIRE(out.size() == 1);
    CHECK(out[0].name == "4200000000000000");
    CHECK(out[0].has_metadata == false);
    CHECK(out[0].enabled == false);
    CHECK(out[0].requires_reboot == true); // safe default
}

TEST_CASE("output is sorted by display name, case-insensitively") {
    std::vector<RawSysmoduleEntry> raw = {
        {"1", true, false, R"({"name":"zsys"})"},
        {"2", true, false, R"({"name":"Atmo"})"},
    };
    std::vector<Sysmodule> out = build_sysmodule_list(raw);
    REQUIRE(out.size() == 2);
    CHECK(out[0].name == "Atmo");
    CHECK(out[1].name == "zsys");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `make -C tests test`
Expected: FAIL to compile — `sysmod_scan.hpp` not found.

- [ ] **Step 3: Write the header**

```cpp
#pragma once
#include "core/sysmod/sysmod_types.hpp"
#include <vector>

namespace thomaz::core {

// Turn a raw /atmosphere/contents listing into the user-facing sysmodule list.
// Skips entries without an exefs.nsp (those are LayeredFS game mods, not
// sysmodules). Result is sorted by display name, case-insensitively.
std::vector<Sysmodule> build_sysmodule_list(const std::vector<RawSysmoduleEntry>& entries);

} // namespace thomaz::core
```

- [ ] **Step 4: Write the implementation**

```cpp
#include "core/sysmod/sysmod_scan.hpp"
#include "core/sysmod/toolbox_json.hpp"

#include <algorithm>
#include <cctype>

namespace thomaz::core {

namespace {
std::string lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // namespace

std::vector<Sysmodule> build_sysmodule_list(const std::vector<RawSysmoduleEntry>& entries) {
    std::vector<Sysmodule> out;
    for (const RawSysmoduleEntry& e : entries) {
        if (!e.has_exefs)
            continue; // a game mod's romfs folder, not a sysmodule

        Sysmodule s;
        s.program_id = e.program_id;
        s.enabled    = e.has_boot2_flag;

        ToolboxInfo t = parse_toolbox(e.toolbox_json);
        s.has_metadata    = t.valid;
        s.requires_reboot = t.requires_reboot; // default true when absent/invalid
        s.name = (!t.name.empty()) ? t.name : e.program_id;

        out.push_back(std::move(s));
    }

    std::sort(out.begin(), out.end(), [](const Sysmodule& a, const Sysmodule& b) {
        return lower(a.name) < lower(b.name);
    });
    return out;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Commit**

```bash
git add source/core/sysmod/sysmod_scan.hpp source/core/sysmod/sysmod_scan.cpp tests/test_sysmod_scan.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): build sysmodule list from contents listing"
```

---

## Task 5: Wire the host test suite and run the core tests green

**Files:**
- Modify: `tests/Makefile:3` (the `SRCS :=` line)

- [ ] **Step 1: Add the new core sources to the test build**

In `tests/Makefile`, the `SRCS :=` line ends with several `$(wildcard ../source/core/<area>/*.cpp)` entries. Add `core/sysmod` and the host-testable store source. Find:

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/mods/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) $(wildcard ../source/core/themes/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp ../source/platform/saves/fake_cloud_save_client.cpp ../source/platform/saves/http_cloud_save_client.cpp ../source/platform/saves/save_backup_io.cpp ../source/platform/mods/mod_store.cpp ../source/platform/themes/theme_paths.cpp
```

Replace with (adds `../source/core/sysmod/*.cpp` and `../source/platform/sysmod/sysmod_store.cpp`):

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/mods/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) $(wildcard ../source/core/themes/*.cpp) $(wildcard ../source/core/sysmod/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp ../source/platform/saves/fake_cloud_save_client.cpp ../source/platform/saves/http_cloud_save_client.cpp ../source/platform/saves/save_backup_io.cpp ../source/platform/mods/mod_store.cpp ../source/platform/themes/theme_paths.cpp ../source/platform/sysmod/sysmod_store.cpp
```

(The `sysmod_store.cpp` source is created in Task 6; until then the wildcard for it does not exist. If you run tests between Task 5 and Task 6, temporarily omit the trailing `../source/platform/sysmod/sysmod_store.cpp` token — re-add it in Task 6.)

- [ ] **Step 2: Run the core tests (Tasks 2-4) green**

Run: `make -C tests clean && make -C tests test`
Expected: PASS — all new `test_sysmod_paths`, `test_toolbox_json`, `test_sysmod_scan` cases pass, plus the existing suite stays green.

- [ ] **Step 3: Commit**

```bash
git add tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "test(system): build core sysmod sources into host suite"
```

---

## Task 6: Filesystem scan + flag toggle (host-tested free functions) and the store interface

**Files:**
- Create: `source/platform/sysmod/sysmod_store.hpp`, `source/platform/sysmod/sysmod_store.cpp`
- Test: `tests/test_sysmod_store.cpp`

These free functions take an explicit root path so they run on the host against a temp dir (exactly how `test_mod_store` works).

- [ ] **Step 1: Write the failing test**

```cpp
#include "doctest.h"
#include "platform/sysmod/sysmod_store.hpp"

#include <cstdio>
#include <sys/stat.h>
#include <string>

using namespace thomaz;

namespace {
const std::string ROOT = "test-sysmod-tmp";

void rm_rf(const std::string& p) {
    std::string cmd = "rm -rf '" + p + "'";
    (void)std::system(cmd.c_str());
}
void mkpath(const std::string& p) {
    std::string cmd = "mkdir -p '" + p + "'";
    (void)std::system(cmd.c_str());
}
void touch(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "wb");
    if (f) std::fclose(f);
}
bool exists(const std::string& p) {
    struct stat st; return ::stat(p.c_str(), &st) == 0;
}
} // namespace

TEST_CASE("scan_contents finds an exefs sysmodule and reads its flag + toolbox") {
    rm_rf(ROOT);
    mkpath(ROOT + "/00FF0000636C6BFF/flags");
    touch(ROOT + "/00FF0000636C6BFF/exefs.nsp");
    touch(ROOT + "/00FF0000636C6BFF/flags/boot2.flag");
    std::FILE* tb = std::fopen((ROOT + "/00FF0000636C6BFF/toolbox.json").c_str(), "wb");
    REQUIRE(tb != nullptr);
    std::fputs(R"({"name":"sys-clk"})", tb);
    std::fclose(tb);

    // A romfs-only mod folder that must be ignored.
    mkpath(ROOT + "/0100000000010000/romfs");

    auto entries = sysmod_scan_contents(ROOT);
    rm_rf(ROOT);

    REQUIRE(entries.size() == 2); // scan returns all folders; core filters exefs
    // Find the sysmodule entry.
    const core::RawSysmoduleEntry* mod = nullptr;
    const core::RawSysmoduleEntry* game = nullptr;
    for (auto& e : entries) {
        if (e.program_id == "00FF0000636C6BFF") mod = &e;
        if (e.program_id == "0100000000010000") game = &e;
    }
    REQUIRE(mod != nullptr);
    CHECK(mod->has_exefs == true);
    CHECK(mod->has_boot2_flag == true);
    CHECK(mod->toolbox_json == R"({"name":"sys-clk"})");
    REQUIRE(game != nullptr);
    CHECK(game->has_exefs == false);
}

TEST_CASE("scan_contents on a missing root returns empty") {
    CHECK(sysmod_scan_contents("does-not-exist-xyz").empty());
}

TEST_CASE("set_boot2_flag creates and removes the flag file") {
    rm_rf(ROOT);
    mkpath(ROOT + "/AAAA");
    const std::string dir = ROOT + "/AAAA";

    CHECK(sysmod_set_boot2_flag(dir, true));
    CHECK(exists(dir + "/flags/boot2.flag"));

    CHECK(sysmod_set_boot2_flag(dir, false));
    CHECK(!exists(dir + "/flags/boot2.flag"));

    rm_rf(ROOT);
}
```

- [ ] **Step 2: Run test to verify it fails**

First re-add the store source token to `tests/Makefile` if you omitted it in Task 5 (the trailing `../source/platform/sysmod/sysmod_store.cpp`).
Run: `make -C tests test`
Expected: FAIL to compile — `platform/sysmod/sysmod_store.hpp` not found.

- [ ] **Step 3: Write the header**

```cpp
#pragma once
#include "core/sysmod/sysmod_types.hpp"
#include <string>
#include <vector>

namespace thomaz {

// Scan a contents root (e.g. /atmosphere/contents) into raw entries: one per
// immediate subfolder, recording exefs.nsp presence, flags/boot2.flag presence,
// and the raw toolbox.json contents. Missing root -> empty. Pure POSIX; runs on
// host and Switch.
std::vector<core::RawSysmoduleEntry> sysmod_scan_contents(const std::string& root);

// Create (enabled=true) or remove (enabled=false) <contents_dir>/flags/boot2.flag.
// Creates the flags/ dir as needed. Returns true on success (including the
// already-in-target-state case).
bool sysmod_set_boot2_flag(const std::string& contents_dir, bool enabled);

// Interface consumed by the UI. Real impl scans /atmosphere/contents; the
// desktop fake (sysmod_store_fake) is in-memory.
struct ISysmoduleStore {
    virtual ~ISysmoduleStore() = default;
    virtual std::vector<core::Sysmodule> list() = 0;
    // Enable/disable a sysmodule by its program id. Returns success.
    virtual bool setEnabled(const std::string& program_id, bool enabled) = 0;
};

// POSIX implementation over a fixed contents root (defaults to
// core::sysmod_contents_root()).
class SysmoduleStore : public ISysmoduleStore {
  public:
    explicit SysmoduleStore(std::string contents_root);
    SysmoduleStore();
    std::vector<core::Sysmodule> list() override;
    bool setEnabled(const std::string& program_id, bool enabled) override;
  private:
    std::string root;
};

} // namespace thomaz
```

- [ ] **Step 4: Write the implementation**

```cpp
#include "platform/sysmod/sysmod_store.hpp"
#include "core/sysmod/sysmod_paths.hpp"
#include "core/sysmod/sysmod_scan.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace thomaz {

namespace {

bool path_exists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
}

bool is_dir(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string read_file(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (!f)
        return {};
    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    return out;
}

} // namespace

std::vector<core::RawSysmoduleEntry> sysmod_scan_contents(const std::string& root) {
    std::vector<core::RawSysmoduleEntry> out;
    DIR* d = ::opendir(root.c_str());
    if (!d)
        return out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string dir = root + "/" + name;
        if (!is_dir(dir))
            continue;
        core::RawSysmoduleEntry entry;
        entry.program_id     = name;
        entry.has_exefs      = path_exists(dir + "/exefs.nsp");
        entry.has_boot2_flag = path_exists(dir + "/flags/boot2.flag");
        entry.toolbox_json   = read_file(dir + "/toolbox.json");
        out.push_back(std::move(entry));
    }
    ::closedir(d);
    return out;
}

bool sysmod_set_boot2_flag(const std::string& contents_dir, bool enabled) {
    std::string flags_dir = contents_dir + "/flags";
    std::string flag      = flags_dir + "/boot2.flag";
    if (enabled) {
        ::mkdir(flags_dir.c_str(), 0777); // ignore EEXIST
        if (path_exists(flag))
            return true;
        std::FILE* f = std::fopen(flag.c_str(), "wb");
        if (!f)
            return false;
        return std::fclose(f) == 0;
    }
    ::remove(flag.c_str());
    return !path_exists(flag);
}

SysmoduleStore::SysmoduleStore(std::string contents_root) : root(std::move(contents_root)) {}
SysmoduleStore::SysmoduleStore() : root(core::sysmod_contents_root()) {}

std::vector<core::Sysmodule> SysmoduleStore::list() {
    return core::build_sysmodule_list(sysmod_scan_contents(root));
}

bool SysmoduleStore::setEnabled(const std::string& program_id, bool enabled) {
    return sysmod_set_boot2_flag(root + "/" + program_id, enabled);
}

} // namespace thomaz
```

- [ ] **Step 5: Run tests green**

Run: `make -C tests clean && make -C tests test`
Expected: PASS — `test_sysmod_store` cases pass, full suite green.

- [ ] **Step 6: Commit**

```bash
git add source/platform/sysmod/sysmod_store.hpp source/platform/sysmod/sysmod_store.cpp tests/test_sysmod_store.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): scan contents + toggle boot2 flag (host-tested)"
```

---

## Task 7: Desktop fake store

**Files:**
- Create: `source/platform/sysmod/sysmod_store_fake.hpp`, `source/platform/sysmod/sysmod_store_fake.cpp`

Lets the UI be exercised on the desktop build, where `/atmosphere/contents` does not exist. Not host-tested (trivial in-memory logic); compiled into the desktop binary only.

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "platform/sysmod/sysmod_store.hpp"
#include <vector>

namespace thomaz {

// In-memory ISysmoduleStore for the desktop build: a few fictional sysmodules
// whose enabled state toggles in memory.
class FakeSysmoduleStore : public ISysmoduleStore {
  public:
    FakeSysmoduleStore();
    std::vector<core::Sysmodule> list() override;
    bool setEnabled(const std::string& program_id, bool enabled) override;
  private:
    std::vector<core::Sysmodule> mods;
};

} // namespace thomaz
```

- [ ] **Step 2: Write the implementation**

```cpp
#ifndef __SWITCH__

#include "platform/sysmod/sysmod_store_fake.hpp"

namespace thomaz {

FakeSysmoduleStore::FakeSysmoduleStore() {
    mods.push_back({"00FF0000636C6BFF", "sys-clk", true, true, true});
    mods.push_back({"010000000000bd00", "MissionControl", true, false, true});
    mods.push_back({"420000000000000E", "sys-ftpd", true, true, true});
    mods.push_back({"4200000000000000", "4200000000000000", true, false, false});
}

std::vector<core::Sysmodule> FakeSysmoduleStore::list() {
    return mods;
}

bool FakeSysmoduleStore::setEnabled(const std::string& program_id, bool enabled) {
    for (core::Sysmodule& m : mods) {
        if (m.program_id == program_id) {
            m.enabled = enabled;
            return true;
        }
    }
    return false;
}

} // namespace thomaz

#endif // __SWITCH__
```

- [ ] **Step 3: Commit**

```bash
git add source/platform/sysmod/sysmod_store_fake.hpp source/platform/sysmod/sysmod_store_fake.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): in-memory fake sysmodule store for desktop"
```

---

## Task 8: Reboot helper

**Files:**
- Create: `source/platform/sysmod/system_reboot.hpp`, `source/platform/sysmod/system_reboot.cpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once

namespace thomaz {

// Reboot the console back into CFW via reboot_to_payload (Switch only).
// Returns false if rebooting is unsupported or failed (desktop, or no payload
// registered) — the caller then tells the user to reboot manually. On success
// this does not return (the system reboots).
bool system_reboot_to_payload();

} // namespace thomaz
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "platform/sysmod/system_reboot.hpp"

#ifdef __SWITCH__

#include <switch.h>

namespace thomaz {

bool system_reboot_to_payload() {
    Result rc = spsmInitialize();
    if (R_FAILED(rc))
        return false;
    // Reboot into the registered payload (Hekate/fusee). On success this call
    // does not return.
    rc = spsmShutdown(true);
    spsmExit();
    return R_SUCCEEDED(rc);
}

} // namespace thomaz

#else

namespace thomaz {

bool system_reboot_to_payload() {
    return false; // unsupported on desktop
}

} // namespace thomaz

#endif // __SWITCH__
```

> **Hardware-confirm note for the executor:** `spsmShutdown(true)` performs a reboot. Whether it lands back in CFW depends on the user's boot chain (Hekate auto-launch). The fallback path (return false → "reboot manually") covers setups where it does not. If a more reliable `reboot_to_payload` (writing the payload to the IRAM magic address) is wanted later, that is a follow-up; Phase 1 uses the libnx path with the manual fallback per the spec.

- [ ] **Step 3: Verify it compiles in the desktop build** (full app build happens in Task 10; this is just the file)

No host test (Switch-specific / trivial). Proceed.

- [ ] **Step 4: Commit**

```bash
git add source/platform/sysmod/system_reboot.hpp source/platform/sysmod/system_reboot.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): reboot-to-payload helper with desktop fallback"
```

---

## Task 9: i18n strings and the activity XML

**Files:**
- Create: `resources/i18n/pt-BR/system.json`, `resources/i18n/en-US/system.json`
- Create: `resources/xml/activity/system.xml`

- [ ] **Step 1: Write the pt-BR strings**

`resources/i18n/pt-BR/system.json`:

```json
{
    "title": "Sistema",
    "home": "Sistema",
    "subtitle": "Ative ou desative módulos do sistema (sysmodules)",
    "loading": "Carregando…",
    "empty": "Nenhum sysmodule encontrado em /atmosphere/contents.",
    "no_metadata": "sem metadados",
    "enabled": "Ativado",
    "disabled": "Desativado",
    "toggle_failed": "Falha ao alterar o módulo (SD cheio ou somente leitura?)",
    "reboot_banner": "Mudanças aplicam no próximo boot.",
    "reboot_now": "Reiniciar agora",
    "reboot_manual": "Reinicie o console manualmente para aplicar.",
    "reboot_failed": "Não foi possível reiniciar automaticamente. Reinicie manualmente."
}
```

- [ ] **Step 2: Write the en-US strings**

`resources/i18n/en-US/system.json`:

```json
{
    "title": "System",
    "home": "System",
    "subtitle": "Enable or disable system modules (sysmodules)",
    "loading": "Loading…",
    "empty": "No sysmodules found in /atmosphere/contents.",
    "no_metadata": "no metadata",
    "enabled": "Enabled",
    "disabled": "Disabled",
    "toggle_failed": "Could not change the module (SD full or read-only?)",
    "reboot_banner": "Changes apply on the next boot.",
    "reboot_now": "Reboot now",
    "reboot_manual": "Reboot the console manually to apply.",
    "reboot_failed": "Could not reboot automatically. Please reboot manually."
}
```

- [ ] **Step 3: Write the activity XML**

`resources/xml/activity/system.xml` (AppletFrame with a header subtitle, a reboot banner that starts hidden, and a scrollable list box the activity fills at runtime):

```xml
<brls:AppletFrame id="systemFrame" title="@i18n/system/title" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0"
              paddingTop="28" paddingBottom="24" paddingLeft="40" paddingRight="40">

        <brls:Label text="@i18n/system/subtitle" fontSize="15"
                    textColor="@theme/thomaz/text_dim" marginBottom="14"/>

        <!-- Reboot banner: hidden until a requires_reboot toggle happens. -->
        <brls:Box id="rebootBanner" axis="row" visibility="gone"
                  cornerRadius="12" padding="14" marginBottom="14"
                  alignItems="center" justifyContent="spaceBetween"
                  backgroundColor="@theme/thomaz/accent_bright">
            <brls:Label id="rebootBannerLabel" text="@i18n/system/reboot_banner"
                        fontSize="15" textColor="#FFFFFF"/>
            <brls:Button id="rebootButton" style="highlight"
                         text="@i18n/system/reboot_now"/>
        </brls:Box>

        <brls:ScrollingFrame grow="1.0">
            <brls:Box id="sysmodListBox" axis="column" width="auto"/>
        </brls:ScrollingFrame>

    </brls:Box>
</brls:AppletFrame>
```

- [ ] **Step 4: Commit**

```bash
git add resources/i18n/pt-BR/system.json resources/i18n/en-US/system.json resources/xml/activity/system.xml
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): i18n strings and Sistema activity layout"
```

---

## Task 10: The SystemActivity

**Files:**
- Create: `source/app/system_activity.hpp`, `source/app/system_activity.cpp`

- [ ] **Step 1: Write the header**

```cpp
/*
    thomaz — system (sysmodules) activity.
    Lists installed sysmodules under /atmosphere/contents, lets the user toggle
    each one's boot2 flag, and offers a reboot when a change requires it.
*/

#pragma once

#include <memory>

#include <borealis.hpp>
#include "platform/sysmod/sysmod_store.hpp"

namespace thomaz {

class SystemActivity : public brls::Activity
{
  public:
    explicit SystemActivity(std::shared_ptr<ISysmoduleStore> store);

    CONTENT_FROM_XML_RES("activity/system.xml");

    void onContentAvailable() override;

  private:
    // (Re)build the sysmodule rows into sysmodListBox.
    void refreshList();
    // Apply a toggle; on a requires_reboot module, reveal the reboot banner.
    void onToggle(const core::Sysmodule& mod, bool enabled);
    // Reveal/refresh the reboot banner.
    void showRebootBanner();

    std::shared_ptr<ISysmoduleStore> store;
    bool rebootPending = false;
};

} // namespace thomaz
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "app/system_activity.hpp"
#include "app/app_header.hpp"
#include "platform/sysmod/system_reboot.hpp"

#include <borealis.hpp>

namespace thomaz {

SystemActivity::SystemActivity(std::shared_ptr<ISysmoduleStore> store)
    : store(std::move(store))
{
}

void SystemActivity::onContentAvailable()
{
    install_header_username(this);

    if (brls::View* btn = this->getView("rebootButton"))
    {
        btn->registerClickAction([this](brls::View*) {
            if (!system_reboot_to_payload())
            {
                brls::Application::notify(brls::getStr("system/reboot_failed"));
            }
            return true;
        });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }

    this->refreshList();
}

void SystemActivity::refreshList()
{
    auto* listBox = dynamic_cast<brls::Box*>(this->getView("sysmodListBox"));
    if (!listBox)
        return;

    listBox->clearViews();

    std::vector<core::Sysmodule> mods = this->store->list();
    if (mods.empty())
    {
        auto* empty = new brls::Label();
        empty->setText(brls::getStr("system/empty"));
        empty->setFontSize(16);
        listBox->addView(empty);
        return;
    }

    for (const core::Sysmodule& mod : mods)
    {
        // A boolean cell: label + on/off, toggling the boot2 flag.
        auto* cell = new brls::BooleanCell();
        std::string title = mod.name;
        if (!mod.has_metadata)
            title += "  (" + brls::getStr("system/no_metadata") + ")";
        cell->title->setText(title);
        cell->setState(mod.enabled);

        core::Sysmodule captured = mod;
        cell->getEvent()->subscribe([this, captured, cell](bool value) {
            this->onToggle(captured, value);
        });

        listBox->addView(cell);
    }
}

void SystemActivity::onToggle(const core::Sysmodule& mod, bool enabled)
{
    if (!this->store->setEnabled(mod.program_id, enabled))
    {
        brls::Application::notify(brls::getStr("system/toggle_failed"));
        // Rebuild so the visible state matches reality after a failed write.
        this->refreshList();
        return;
    }
    if (mod.requires_reboot)
        this->showRebootBanner();
}

void SystemActivity::showRebootBanner()
{
    this->rebootPending = true;
    if (brls::View* banner = this->getView("rebootBanner"))
        banner->setVisibility(brls::Visibility::VISIBLE);
}

} // namespace thomaz
```

> **Note for the executor:** `brls::BooleanCell` is the toggle cell used elsewhere in Borealis; if this codebase's Borealis fork names it differently, mirror whatever `settings_activity.cpp` uses for on/off rows (check there first and match it). The event subscription API (`getEvent()->subscribe`) likewise should match the fork — grep `settings_activity.cpp` for the existing toggle pattern and follow it exactly.

- [ ] **Step 3: Commit**

```bash
git add source/app/system_activity.hpp source/app/system_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): Sistema activity — list + toggle + reboot banner"
```

---

## Task 11: Add the "Sistema" card to the home and wire it

**Files:**
- Modify: `resources/xml/activity/home.xml` (add a card view with `id="systemCard"`)
- Modify: `source/app/home_activity.cpp` (include + wire the card)

- [ ] **Step 1: Add the card to home.xml**

Open `resources/xml/activity/home.xml`, find the rail/grid that holds the existing cards (look for `id="savesCard"` / `id="themesCard"`). Add a sibling card matching the surrounding markup. Use this block, adjusting attributes to match the neighbouring cards' exact style (copy a sibling card's attributes; only the `id` and label text differ):

```xml
<brls:Box id="systemCard" axis="column" cornerRadius="16" padding="20"
          backgroundColor="@theme/thomaz/card" marginRight="16"
          focusable="true">
    <brls:Label text="@i18n/system/home" fontSize="22"
                textColor="@theme/thomaz/text"/>
    <brls:Label text="@i18n/system/subtitle" fontSize="13"
                textColor="@theme/thomaz/text_dim" marginTop="6"/>
</brls:Box>
```

- [ ] **Step 2: Wire the card in home_activity.cpp**

Add the include near the other activity includes (after line 10, `#include "app/theme_browser_activity.hpp"`):

```cpp
#include "app/system_activity.hpp"
#include "platform/sysmod/sysmod_store.hpp"
#ifndef __SWITCH__
#include "platform/sysmod/sysmod_store_fake.hpp"
#endif
```

Inside `HomeActivity::onContentAvailable()`, after the `savesCard` block (before the closing brace of the method), add:

```cpp
    if (brls::View* system = this->getView("systemCard")) {
        system->registerClickAction([this](brls::View*) {
#ifdef __SWITCH__
            auto store = std::make_shared<SysmoduleStore>();
#else
            auto store = std::make_shared<FakeSysmoduleStore>();
#endif
            brls::Application::pushActivity(new SystemActivity(store),
                                            brls::TransitionAnimation::NONE);
            return true;
        });
        system->addGestureRecognizer(new brls::TapGestureRecognizer(system));
    }
```

- [ ] **Step 3: Build the desktop app and smoke-test**

Run: `./scripts/build-desktop.sh && timeout 8 ./build_desktop/thomaz`
Expected: builds clean; app launches; the home shows a **Sistema** card; opening it lists the four fake sysmodules; toggling one that requires reboot reveals the reboot banner. (Exit 124 from `timeout` is a healthy smoke run.)

- [ ] **Step 4: Commit**

```bash
git add resources/xml/activity/home.xml source/app/home_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(system): add Sistema card to home and wire the activity"
```

---

## Task 12: Full host suite + desktop build green

- [ ] **Step 1: Run the whole host test suite**

Run: `make -C tests clean && make -C tests test`
Expected: PASS — entire suite green, including all new sysmod tests.

- [ ] **Step 2: Build desktop once more to confirm no regressions**

Run: `./scripts/build-desktop.sh`
Expected: builds clean with no warnings introduced by the new files.

- [ ] **Step 3: Final verification note**

Phase 1 is host-verified (logic) and desktop-smoke-verified (UI with fake data). The real `/atmosphere/contents` scan, boot2 flag writes, and `reboot_to_payload` remain **pending hardware validation** — consistent with the project's current status. Record this in the PR description.

---

## Self-review against the spec

- **Detection (list installed sysmodules):** Tasks 1, 4, 6 — `scan_contents` + `build_sysmodule_list`, filtered by `exefs.nsp` so game mods are excluded. ✓
- **toolbox.json (friendly name, requires_reboot):** Tasks 1, 3. ✓
- **Toggle via boot2 flag:** Task 6 (`set_boot2_flag`, `SysmoduleStore::setEnabled`). ✓
- **Folder without toolbox.json shows ProgramID + "no metadata":** Task 4 (name fallback, `has_metadata=false`) + Task 10 (label suffix). ✓
- **`/atmosphere/contents` missing → empty list + hint:** Task 6 (empty scan) + Task 10/9 (`system/empty`). ✓
- **Toggle write failure → revert UI + warn:** Task 10 (`toggle_failed` + `refreshList`). ✓
- **Warn + offer reboot; reboot_to_payload with fallback:** Tasks 8, 9, 10 (banner, button, `reboot_failed`). ✓
- **Desktop fake for UI iteration:** Task 7. ✓
- **Borealis touch + mouse + controller:** Task 10/11 (`TapGestureRecognizer` on card + button, like existing cards). ✓
- **i18n pt-BR + en-US:** Task 9. ✓
- **Offline:** entire Phase 1 reads/writes the SD only; no network. ✓
- **Out of scope (per spec):** install/catalog (Phase 2), overlays (Phase 3), overclock presets — none included. ✓

**Placeholder scan:** no TBD/"handle errors"/uncode steps — every code step shows full code. Two executor notes (BooleanCell naming, reboot reliability) flag fork-specific verification, not missing content.

**Type consistency:** `RawSysmoduleEntry`/`Sysmodule`/`ToolboxInfo` fields are used identically across Tasks 1, 3, 4, 6, 7, 10. Function names (`sysmod_scan_contents`, `sysmod_set_boot2_flag`, `build_sysmodule_list`, `parse_toolbox`, `system_reboot_to_payload`, `ISysmoduleStore::list`/`setEnabled`) match between declaration and use. ✓
