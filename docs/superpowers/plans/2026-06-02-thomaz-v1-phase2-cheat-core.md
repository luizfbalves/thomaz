# thomaz v1 — Phase 2: Cheat-format Core (pure logic, TDD) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement and unit-test (on the host PC, no devkitPro) the pure C++ core of the Trapaças module: parse/serialize Atmosphère cheat `.txt` files, adapt the switch-cheats-db JSON, parse the version map, and resolve a `build_id` from a game's version with the two fallbacks the spike identified.

**Architecture:** A `thomaz::core` library under `source/core/` containing ONLY pure logic — no libnx, no Borealis, no I/O. Everything takes strings/values in and returns values out, so it compiles and runs under the system `g++` and is fully covered by the doctest harness from Phase 1. Phase 3 (libnx borders) and Phase 4 (UI) will call into this core; the core never calls them.

**Tech Stack:** C++17, nlohmann/json (single-header, works on host AND devkitA64), doctest (host tests from Phase 1).

**Inputs from the spike** (`docs/superpowers/research/2026-06-02-cheat-format-and-db-spike.md`):
- Cheat `.txt`: header line `[Name]` (regular) or `{Name}` (master); following non-blank lines are opcode lines; blank lines are cosmetic; the next header ends the current cheat.
- db per-title cheats JSON: `{ "<BUILD_ID>": { "[Name]": "<full cheat text incl header>\n", ... }, "attribution": {...} }`. Top-level keys are build_ids except the literal `"attribution"`.
- db version map JSON: `{ "<version_u32_as_string>": "<BUILD_ID>", ..., "latest": <int>, "title": "<name>" }`.
- Resolution: exact version→build_id if that build_id has cheats; else fall back to the most recent (highest version) build_id that *does* have cheats; else not-in-db.
- "Active file" rule: write master cheats + only the enabled regular cheats.

---

## File Structure (created/modified in this phase)

```
thomaz/
├── lib/json/nlohmann/json.hpp        # vendored nlohmann/json single header (included as <nlohmann/json.hpp>)
├── source/core/
│   ├── cheat.hpp                     # Cheat value type (header-only)
│   ├── cheat_txt.hpp / cheat_txt.cpp # parse_txt() + serialize_txt()
│   ├── cheat_db.hpp  / cheat_db.cpp  # parse_db_cheats() + parse_versions() (+ VersionMap)
│   └── build_id.hpp  / build_id.cpp  # resolve_build_id() + Resolution
└── tests/
    ├── Makefile                      # MODIFIED: compile core/*.cpp + all test_*.cpp
    ├── test_cheat_txt.cpp            # NEW
    ├── test_cheat_db.cpp             # NEW
    └── test_build_id.cpp            # NEW
```

**Responsibilities:**
- `cheat.hpp` — the `Cheat` value type, shared by all core units. No logic.
- `cheat_txt.*` — text format only (SD-card `.txt` read + active-file write).
- `cheat_db.*` — switch-cheats-db JSON shapes only (cheats + versions).
- `build_id.*` — the version→build_id decision only.
- Each unit has one responsibility and a small, value-in/value-out interface.

All identifiers in English. No `using namespace std` in headers.

---

## Task 1: Vendor nlohmann/json and wire the core into the test build

**Files:**
- Create: `lib/json/json.hpp`
- Modify: `tests/Makefile`
- Create: `tests/test_core_smoke.cpp`

- [ ] **Step 1: Vendor nlohmann/json**

The header is included as `<nlohmann/json.hpp>` with `-I../lib/json`, so it MUST live at `lib/json/nlohmann/json.hpp`. Run:
```bash
mkdir -p lib/json/nlohmann
wget -O lib/json/nlohmann/json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
test -s lib/json/nlohmann/json.hpp && grep -q "namespace nlohmann" lib/json/nlohmann/json.hpp && echo "json vendored"
```
Expected: prints `json vendored`. (If wget is blocked, report BLOCKED.)

- [ ] **Step 2: Update `tests/Makefile`** to compile the core sources and auto-discover test files. Replace the ENTIRE file with this content (recipe lines start with a literal TAB):

```make
CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib/doctest -I../lib/json -I../source
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp)
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
```

- [ ] **Step 3: Add a core-include smoke test** to prove json.hpp + include paths compile. Create `tests/test_core_smoke.cpp`:

```cpp
#include "doctest.h"
#include <nlohmann/json.hpp>

TEST_CASE("json library is wired in") {
    auto j = nlohmann::json::parse(R"({"a": 1})");
    CHECK(j["a"].get<int>() == 1);
}
```

- [ ] **Step 4: Build and run — expect PASS**

Run:
```bash
make -C tests test
```
Expected: compiles (json.hpp resolves via `-I../lib/json`), all tests pass including "json library is wired in".

- [ ] **Step 5: Commit**

```bash
git add lib/json/nlohmann/json.hpp tests/Makefile tests/test_core_smoke.cpp
git commit -m "build: vendor nlohmann/json and wire core sources into host tests"
```

---

## Task 2: Cheat value type + `.txt` parser

**Files:**
- Create: `source/core/cheat.hpp`
- Create: `source/core/cheat_txt.hpp`, `source/core/cheat_txt.cpp`
- Create: `tests/test_cheat_txt.cpp`

- [ ] **Step 1: Define the `Cheat` value type.** Create `source/core/cheat.hpp`:

```cpp
#pragma once
#include <string>
#include <vector>

namespace thomaz::core {

// One cheat parsed from an Atmosphère cheat .txt file.
struct Cheat {
    std::string name;                     // display name, WITHOUT brackets/braces
    bool is_master = false;               // true when the header used { } (anchor/master code)
    std::vector<std::string> opcode_lines; // opcode lines (no blanks), in original order

    bool operator==(const Cheat& o) const {
        return name == o.name && is_master == o.is_master && opcode_lines == o.opcode_lines;
    }
};

} // namespace thomaz::core
```

- [ ] **Step 2: Declare the parser.** Create `source/core/cheat_txt.hpp`:

```cpp
#pragma once
#include "core/cheat.hpp"
#include <string>
#include <vector>
#include <set>

namespace thomaz::core {

// Parse an Atmosphère cheat .txt file body into ordered cheats.
// A header line is a trimmed line of the form [Name] (regular) or {Name} (master).
// Non-blank lines after a header are that cheat's opcode lines until the next header.
// Lines before the first header are ignored.
std::vector<Cheat> parse_txt(const std::string& content);

// Serialize "master cheats + only the enabled regular cheats" back to .txt body.
// Order is preserved from `cheats`. A cheat is included if it is_master OR its name is in enabled.
// Each cheat is written as: header line, then its opcode lines, then one blank separator line.
std::string serialize_txt(const std::vector<Cheat>& cheats, const std::set<std::string>& enabled);

} // namespace thomaz::core
```

- [ ] **Step 3: Write failing tests.** Create `tests/test_cheat_txt.cpp`:

```cpp
#include "doctest.h"
#include "core/cheat_txt.hpp"

using thomaz::core::parse_txt;
using thomaz::core::Cheat;

TEST_CASE("parse_txt splits regular and master cheats") {
    // Real shape from Super Mario Odyssey (spike example).
    const std::string body =
        "{Master Code}\n"
        "580F0000 0149D940\n"
        "\n"
        "[Infinite Health Save 1]\n"
        "11160000 5C3BE7DC 00000000\n"
        "01100000 5C3BE7DC 00000006\n"
        "20000000\n"
        "\n"
        "[9999 Coins Save 1]\n"
        "02100000 5C27B318 0000270f\n";

    auto cheats = parse_txt(body);
    REQUIRE(cheats.size() == 3);

    CHECK(cheats[0].is_master == true);
    CHECK(cheats[0].name == "Master Code");
    CHECK(cheats[0].opcode_lines == std::vector<std::string>{"580F0000 0149D940"});

    CHECK(cheats[1].is_master == false);
    CHECK(cheats[1].name == "Infinite Health Save 1");
    CHECK(cheats[1].opcode_lines.size() == 3);
    CHECK(cheats[1].opcode_lines[2] == "20000000");

    CHECK(cheats[2].name == "9999 Coins Save 1");
    CHECK(cheats[2].opcode_lines == std::vector<std::string>{"02100000 5C27B318 0000270f"});
}

TEST_CASE("parse_txt ignores text before the first header and trailing whitespace") {
    const std::string body =
        "some attribution line\n"
        "[Only Cheat]\r\n"      // tolerate CRLF
        "  04000000 0000 \n";   // leading/trailing spaces trimmed
    auto cheats = parse_txt(body);
    REQUIRE(cheats.size() == 1);
    CHECK(cheats[0].name == "Only Cheat");
    CHECK(cheats[0].opcode_lines == std::vector<std::string>{"04000000 0000"});
}

TEST_CASE("parse_txt on empty input returns no cheats") {
    CHECK(parse_txt("").empty());
    CHECK(parse_txt("\n\n  \n").empty());
}
```

- [ ] **Step 4: Run — expect FAIL (link/compile error: parse_txt undefined / cheat_txt.cpp missing)**

Run:
```bash
make -C tests test
```
Expected: FAIL (undefined reference to `parse_txt`, since cheat_txt.cpp does not exist yet).

- [ ] **Step 5: Implement the parser.** Create `source/core/cheat_txt.cpp` (serialize_txt is a stub here; implemented & tested in Task 3):

```cpp
#include "core/cheat_txt.hpp"
#include <sstream>

namespace thomaz::core {

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

// If `line` is a header, fill name/is_master and return true.
bool parse_header(const std::string& line, std::string& name, bool& is_master) {
    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
        name = line.substr(1, line.size() - 2);
        is_master = false;
        return true;
    }
    if (line.size() >= 2 && line.front() == '{' && line.back() == '}') {
        name = line.substr(1, line.size() - 2);
        is_master = true;
        return true;
    }
    return false;
}

} // namespace

std::vector<Cheat> parse_txt(const std::string& content) {
    std::vector<Cheat> cheats;
    std::istringstream stream(content);
    std::string raw;
    bool have_current = false;

    while (std::getline(stream, raw)) {
        const std::string line = trim(raw);
        if (line.empty()) continue;

        std::string name;
        bool is_master = false;
        if (parse_header(line, name, is_master)) {
            Cheat c;
            c.name = name;
            c.is_master = is_master;
            cheats.push_back(std::move(c));
            have_current = true;
        } else if (have_current) {
            cheats.back().opcode_lines.push_back(line);
        }
        // lines before the first header are ignored
    }
    return cheats;
}

std::string serialize_txt(const std::vector<Cheat>&, const std::set<std::string>&) {
    return ""; // implemented in Task 3
}

} // namespace thomaz::core
```

- [ ] **Step 6: Run — expect PASS**

Run:
```bash
make -C tests test
```
Expected: all parse_txt tests pass.

- [ ] **Step 7: Commit**

```bash
git add source/core/cheat.hpp source/core/cheat_txt.hpp source/core/cheat_txt.cpp tests/test_cheat_txt.cpp
git commit -m "feat(core): Cheat type + Atmosphere cheat .txt parser"
```

---

## Task 3: `.txt` serializer (master + enabled only)

**Files:**
- Modify: `source/core/cheat_txt.cpp` (replace the serialize_txt stub)
- Modify: `tests/test_cheat_txt.cpp` (add serializer tests)

- [ ] **Step 1: Add failing serializer tests.** Append to `tests/test_cheat_txt.cpp`:

```cpp
#include "core/cheat_txt.hpp"  // already included above; harmless

using thomaz::core::serialize_txt;

TEST_CASE("serialize_txt always includes master and only enabled regulars") {
    std::vector<Cheat> cheats = {
        {"Master", true,  {"580F0000 0149D940"}},
        {"Infinite Health", false, {"01100000 5C3BE7DC 00000006", "20000000"}},
        {"9999 Coins", false, {"02100000 5C27B318 0000270f"}},
    };
    // Only "9999 Coins" enabled; master comes through regardless; "Infinite Health" excluded.
    std::string out = serialize_txt(cheats, {"9999 Coins"});

    const std::string expected =
        "{Master}\n"
        "580F0000 0149D940\n"
        "\n"
        "[9999 Coins]\n"
        "02100000 5C27B318 0000270f\n"
        "\n";
    CHECK(out == expected);
}

TEST_CASE("serialize_txt round-trips through parse_txt") {
    std::vector<Cheat> cheats = {
        {"M", true, {"AAAA"}},
        {"A", false, {"1111", "2222"}},
    };
    std::string out = serialize_txt(cheats, {"A"});
    auto reparsed = parse_txt(out);
    REQUIRE(reparsed.size() == 2);
    CHECK(reparsed[0].is_master == true);
    CHECK(reparsed[0].name == "M");
    CHECK(reparsed[1].name == "A");
    CHECK(reparsed[1].opcode_lines == std::vector<std::string>{"1111", "2222"});
}

TEST_CASE("serialize_txt with nothing enabled still emits the master") {
    std::vector<Cheat> cheats = {
        {"Master", true, {"AAAA"}},
        {"X", false, {"1111"}},
    };
    std::string out = serialize_txt(cheats, {});
    CHECK(out == "{Master}\nAAAA\n\n");
}
```

- [ ] **Step 2: Run — expect FAIL (serializer returns "")**

Run:
```bash
make -C tests test
```
Expected: the three serialize_txt tests FAIL (empty output != expected).

- [ ] **Step 3: Implement serialize_txt.** In `source/core/cheat_txt.cpp`, replace the stub with:

```cpp
std::string serialize_txt(const std::vector<Cheat>& cheats, const std::set<std::string>& enabled) {
    std::string out;
    for (const Cheat& c : cheats) {
        const bool include = c.is_master || enabled.count(c.name) > 0;
        if (!include) continue;
        out += c.is_master ? ("{" + c.name + "}\n") : ("[" + c.name + "]\n");
        for (const std::string& line : c.opcode_lines) {
            out += line;
            out += "\n";
        }
        out += "\n"; // blank separator after each cheat
    }
    return out;
}
```

- [ ] **Step 4: Run — expect PASS**

Run:
```bash
make -C tests test
```
Expected: all cheat_txt tests pass (parser + serializer).

- [ ] **Step 5: Commit**

```bash
git add source/core/cheat_txt.cpp tests/test_cheat_txt.cpp
git commit -m "feat(core): serialize active cheat .txt (master + enabled only)"
```

---

## Task 4: switch-cheats-db cheats JSON adapter

**Files:**
- Create: `source/core/cheat_db.hpp`, `source/core/cheat_db.cpp`
- Create: `tests/test_cheat_db.cpp`

- [ ] **Step 1: Declare the adapter.** Create `source/core/cheat_db.hpp`:

```cpp
#pragma once
#include "core/cheat.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

// Parsed from versions/<TITLE_ID>.json.
struct VersionMap {
    std::map<std::uint32_t, std::string> by_version; // version u32 -> build_id (uppercase hex)
    std::optional<std::uint32_t> latest;
    std::string title;
};

// All build_ids that actually have cheats in cheats/<TITLE_ID>.json (excludes "attribution").
std::vector<std::string> build_ids_with_cheats(const std::string& cheats_json);

// Cheats for one build_id from cheats/<TITLE_ID>.json. Empty if the build_id is absent.
std::vector<Cheat> parse_db_cheats(const std::string& cheats_json, const std::string& build_id);

// Parse versions/<TITLE_ID>.json (keys "latest"/"title" are metadata, not versions).
VersionMap parse_versions(const std::string& versions_json);

} // namespace thomaz::core
```

- [ ] **Step 2: Write failing tests** using the real db shapes from the spike. Create `tests/test_cheat_db.cpp`:

```cpp
#include "doctest.h"
#include "core/cheat_db.hpp"

using namespace thomaz::core;

// Shape of cheats/<TITLE_ID>.json: build_id -> { "[Name]": "<full text>\n" }, plus "attribution".
static const char* CHEATS_JSON = R"({
  "B424BE150A8E7D78": {
    "[Infinite Health]": "[Infinite Health]\n11160000 5C3BE7DC 00000000\n20000000\n",
    "{Master}": "{Master}\n580F0000 0149D940\n"
  },
  "OLDBUILD00000000": {
    "[Old Cheat]": "[Old Cheat]\n04000000 0000\n"
  },
  "attribution": { "[Infinite Health]": "someuser" }
})";

TEST_CASE("build_ids_with_cheats lists build ids and excludes attribution") {
    auto ids = build_ids_with_cheats(CHEATS_JSON);
    REQUIRE(ids.size() == 2);
    // order not guaranteed; check membership
    bool has_new = false, has_old = false, has_attr = false;
    for (auto& id : ids) {
        if (id == "B424BE150A8E7D78") has_new = true;
        if (id == "OLDBUILD00000000") has_old = true;
        if (id == "attribution") has_attr = true;
    }
    CHECK(has_new);
    CHECK(has_old);
    CHECK_FALSE(has_attr);
}

TEST_CASE("parse_db_cheats returns the cheats for one build id") {
    auto cheats = parse_db_cheats(CHEATS_JSON, "B424BE150A8E7D78");
    REQUIRE(cheats.size() == 2);
    // entries parsed via the .txt grammar; find by name
    bool master_ok = false, health_ok = false;
    for (auto& c : cheats) {
        if (c.is_master && c.name == "Master") master_ok = (c.opcode_lines == std::vector<std::string>{"580F0000 0149D940"});
        if (!c.is_master && c.name == "Infinite Health") health_ok = (c.opcode_lines.size() == 2 && c.opcode_lines[1] == "20000000");
    }
    CHECK(master_ok);
    CHECK(health_ok);
}

TEST_CASE("parse_db_cheats on unknown build id is empty") {
    CHECK(parse_db_cheats(CHEATS_JSON, "DOESNOTEXIST0000").empty());
}

TEST_CASE("parse_versions reads version->build_id and metadata") {
    const char* versions = R"({
      "0": "3CA12DFAAF9C82DA",
      "262144": "B424BE150A8E7D78",
      "393216": "B424BE150A8E7D78",
      "latest": 393216,
      "title": "Super Mario Odyssey"
    })";
    VersionMap vm = parse_versions(versions);
    CHECK(vm.by_version.at(0) == "3CA12DFAAF9C82DA");
    CHECK(vm.by_version.at(393216) == "B424BE150A8E7D78");
    CHECK(vm.by_version.count(0) == 1);
    CHECK(vm.by_version.size() == 3); // "latest" and "title" excluded
    REQUIRE(vm.latest.has_value());
    CHECK(vm.latest.value() == 393216);
    CHECK(vm.title == "Super Mario Odyssey");
}
```

- [ ] **Step 3: Run — expect FAIL (cheat_db.cpp missing)**

Run:
```bash
make -C tests test
```
Expected: FAIL (undefined references to the cheat_db functions).

- [ ] **Step 4: Implement the adapter.** Create `source/core/cheat_db.cpp`:

```cpp
#include "core/cheat_db.hpp"
#include "core/cheat_txt.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

std::vector<std::string> build_ids_with_cheats(const std::string& cheats_json) {
    std::vector<std::string> ids;
    json j = json::parse(cheats_json, nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return ids;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.key() == "attribution") continue;
        if (it.value().is_object()) ids.push_back(it.key());
    }
    return ids;
}

std::vector<Cheat> parse_db_cheats(const std::string& cheats_json, const std::string& build_id) {
    std::vector<Cheat> cheats;
    json j = json::parse(cheats_json, nullptr, false);
    if (!j.is_object() || !j.contains(build_id) || !j[build_id].is_object()) return cheats;
    for (auto it = j[build_id].begin(); it != j[build_id].end(); ++it) {
        if (!it.value().is_string()) continue;
        // each value is a full single-cheat .txt snippet (header + opcodes)
        auto parsed = parse_txt(it.value().get<std::string>());
        for (auto& c : parsed) cheats.push_back(std::move(c));
    }
    return cheats;
}

VersionMap parse_versions(const std::string& versions_json) {
    VersionMap vm;
    json j = json::parse(versions_json, nullptr, false);
    if (!j.is_object()) return vm;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& key = it.key();
        if (key == "latest") {
            if (it.value().is_number_unsigned()) vm.latest = it.value().get<std::uint32_t>();
            continue;
        }
        if (key == "title") {
            if (it.value().is_string()) vm.title = it.value().get<std::string>();
            continue;
        }
        // version keys are stringified u32; value is the build_id string
        if (!it.value().is_string()) continue;
        try {
            std::uint32_t version = static_cast<std::uint32_t>(std::stoul(key));
            vm.by_version[version] = it.value().get<std::string>();
        } catch (...) {
            // non-numeric key that is not latest/title: ignore defensively
        }
    }
    return vm;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run — expect PASS**

Run:
```bash
make -C tests test
```
Expected: all cheat_db tests pass.

- [ ] **Step 6: Commit**

```bash
git add source/core/cheat_db.hpp source/core/cheat_db.cpp tests/test_cheat_db.cpp
git commit -m "feat(core): switch-cheats-db JSON adapter (cheats + versions)"
```

---

## Task 5: build_id resolver (version → build_id with fallbacks)

**Files:**
- Create: `source/core/build_id.hpp`, `source/core/build_id.cpp`
- Create: `tests/test_build_id.cpp`

This encodes the spike's GO logic, including fallback (b) verified live against Super Smash Bros. Ultimate.

- [ ] **Step 1: Declare the resolver.** Create `source/core/build_id.hpp`:

```cpp
#pragma once
#include "core/cheat_db.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

struct Resolution {
    enum class Source {
        ExactVersion,        // version maps to a build_id that has cheats
        FallbackOlderBuild,  // exact build_id has no cheats (or version unknown); used newest build_id that does
        NotInDb              // no version-mapped build_id has cheats
    };
    Source source = Source::NotInDb;
    std::string build_id;    // empty iff NotInDb
};

// Resolve which build_id's cheats to use for an installed game version.
// `available` is the set of build_ids that actually have cheats (build_ids_with_cheats()).
Resolution resolve_build_id(std::uint32_t version,
                            const VersionMap& versions,
                            const std::vector<std::string>& available);

} // namespace thomaz::core
```

- [ ] **Step 2: Write failing tests** with the real traced data. Create `tests/test_build_id.cpp`:

```cpp
#include "doctest.h"
#include "core/build_id.hpp"

using namespace thomaz::core;

static VersionMap mario() {
    VersionMap vm;
    vm.by_version = {{262144, "B424BE150A8E7D78"}, {393216, "B424BE150A8E7D78"}};
    vm.latest = 393216;
    return vm;
}

// Smash: latest version 1966080 -> 3EAE..., which has NO cheats (verified in spike).
// Older 1769472 -> B9B1... DOES have cheats.
static VersionMap smash() {
    VersionMap vm;
    vm.by_version = {{1769472, "B9B166DF1DB90BAF"}, {1966080, "3EAE0063B12FD81E"}};
    vm.latest = 1966080;
    return vm;
}

TEST_CASE("exact version maps to a build_id that has cheats") {
    auto r = resolve_build_id(393216, mario(), {"B424BE150A8E7D78"});
    CHECK(r.source == Resolution::Source::ExactVersion);
    CHECK(r.build_id == "B424BE150A8E7D78");
}

TEST_CASE("coverage lag: latest build_id has no cheats -> newest build_id that does") {
    // 3EAE... not in available; B9B1... is.
    auto r = resolve_build_id(1966080, smash(), {"B9B166DF1DB90BAF"});
    CHECK(r.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.build_id == "B9B166DF1DB90BAF");
}

TEST_CASE("unknown version -> newest mapped build_id that has cheats") {
    auto r = resolve_build_id(999999 /*not in map*/, smash(), {"B9B166DF1DB90BAF"});
    CHECK(r.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.build_id == "B9B166DF1DB90BAF");
}

TEST_CASE("nothing mapped has cheats -> NotInDb") {
    auto r = resolve_build_id(1966080, smash(), {/*empty: no cheats anywhere*/});
    CHECK(r.source == Resolution::Source::NotInDb);
    CHECK(r.build_id.empty());
}

TEST_CASE("fallback picks the HIGHEST version whose build_id has cheats") {
    VersionMap vm;
    vm.by_version = {{100, "AAA"}, {300, "CCC"}, {200, "BBB"}};
    // only AAA and BBB have cheats; requested version 999 unknown -> pick BBB (version 200 > 100)
    auto r = resolve_build_id(999, vm, {"AAA", "BBB"});
    CHECK(r.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.build_id == "BBB");
}
```

- [ ] **Step 3: Run — expect FAIL (build_id.cpp missing)**

Run:
```bash
make -C tests test
```
Expected: FAIL (undefined reference to `resolve_build_id`).

- [ ] **Step 4: Implement the resolver.** Create `source/core/build_id.cpp`:

```cpp
#include "core/build_id.hpp"
#include <algorithm>
#include <set>

namespace thomaz::core {

Resolution resolve_build_id(std::uint32_t version,
                            const VersionMap& versions,
                            const std::vector<std::string>& available) {
    const std::set<std::string> have(available.begin(), available.end());

    // 1. Exact version -> build_id that has cheats.
    auto exact = versions.by_version.find(version);
    if (exact != versions.by_version.end() && have.count(exact->second)) {
        return {Resolution::Source::ExactVersion, exact->second};
    }

    // 2. Fallback: highest version whose mapped build_id has cheats.
    //    (by_version is a std::map ordered ascending by version; iterate descending.)
    for (auto it = versions.by_version.rbegin(); it != versions.by_version.rend(); ++it) {
        if (have.count(it->second)) {
            return {Resolution::Source::FallbackOlderBuild, it->second};
        }
    }

    // 3. Nothing mapped has cheats.
    return {Resolution::Source::NotInDb, ""};
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run — expect PASS**

Run:
```bash
make -C tests test
```
Expected: all build_id tests pass.

- [ ] **Step 6: Commit**

```bash
git add source/core/build_id.hpp source/core/build_id.cpp tests/test_build_id.cpp
git commit -m "feat(core): resolve build_id from version with coverage-lag fallback"
```

---

## Phase 2 done = definition

- `make -C tests test` is green and covers: `.txt` parse + serialize (round-trip), db cheats JSON adapter, version-map parser, and build_id resolution including the coverage-lag fallback.
- `source/core/` contains only pure logic (no libnx / Borealis includes), ready to be called by Phase 3 (libnx borders) and Phase 4 (UI).
- The spike's GO logic is now executable and tested with the real Mario Odyssey and Smash Ultimate data.

---

## Self-Review (against the spec + spike)

**Spec/spike coverage:** cheat `.txt` parse rule → Task 2 ✓; "master + enabled only" serialize rule (spec §6 model) → Task 3 ✓; db cheats JSON shape + attribution exclusion → Task 4 ✓; version-map parsing (raw u32 keys) → Task 4 ✓; version→build_id resolution with fallback (a) modeled as "version unknown" and fallback (b) coverage-lag → Task 5 ✓. Fallback (a)'s *local-NSO read* is device-side and belongs to Phase 3; Task 5 models its pure-logic outcome (treated like an unknown version → fallback/NotInDb) — noted, not dropped.

**Placeholder scan:** No TBD/TODO. The serialize_txt stub in Task 2 is explicitly replaced in Task 3 (intentional TDD staging, not a placeholder). All code blocks are complete and compilable.

**Type/name consistency:** `thomaz::core` namespace throughout. `Cheat{name,is_master,opcode_lines}` used identically in cheat_txt, cheat_db, and tests. `VersionMap{by_version,latest,title}` consistent between cheat_db.hpp and build_id usage. `build_ids_with_cheats()` (Task 4) produces the `available` vector consumed by `resolve_build_id()` (Task 5) — names match. Include paths use `core/...` resolved by `-I../source` (set in Task 1's Makefile).
