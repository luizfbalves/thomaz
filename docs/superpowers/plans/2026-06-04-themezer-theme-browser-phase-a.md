# Themezer Theme Browser (Phase A) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Temas" module that browses Themezer (feed + search + section filter) as a thumbnail grid and downloads `.nxtheme` files to `sd:/themes/`.

**Architecture:** Mirror the mods subsystem. A pure, unit-tested `core/themes/` GraphQL client (body builders + JSON parsers + a browse layer that takes an injected fetcher), a `platform/themes/` download layer reusing `download_file`, and two Borealis activities (grid browser + detail). Native install (`.nxtheme → szs`) is Phase B and out of scope.

**Tech Stack:** C++17, libnx/Borealis, nlohmann/json, doctest, CMake, Themezer GraphQL API (`POST https://api.themezer.net/graphql`).

---

## Verified API facts (from live introspection 2026-06-04)

- Endpoint: `POST https://api.themezer.net/graphql`, `Content-Type: application/json`, body `{"query":"...","variables":{...}}`.
- Themes feed/search:
  ```graphql
  query($q:String,$t:Target,$p:PaginationInput){ switch{ themes(query:$q, target:$t, sort:DOWNLOADS, order:DESC, paginationArgs:$p){ pageInfo{page pageCount} nodes{ hexId name downloadCount downloadUrl target creator{username} screenshotPreview{jpgThumbUrl} } } } }
  ```
- Packs feed/search (no `target` arg):
  ```graphql
  query($q:String,$p:PaginationInput){ switch{ packs(query:$q, sort:DOWNLOADS, order:DESC, paginationArgs:$p){ pageInfo{page pageCount} nodes{ hexId name downloadCount downloadUrl creator{username} collagePreview{jpgThumbUrl} } } } }
  ```
- Theme detail (hexId inlined, sanitized to hex chars): `{ switch{ theme(hexId:"A24"){ hexId name description downloadUrl target creator{username} screenshotPreview{jpgThumbUrl} } } }`
- Pack detail: `{ switch{ pack(hexId:"16D"){ hexId name description downloadUrl creator{username} collagePreview{jpgThumbUrl} themes{ hexId name target downloadUrl } } } }`
- Enums: `ItemSort` = CREATED|DOWNLOADS|RISING|SAVES|TRENDING|UPDATED. `SortOrder` = ASC|DESC. `Target` = Entrance|Flaunch|MyPage|Notification|Psl|ResidentMenu|Set.
- `PaginationInput` = `{ page:PositiveInt, limit:PositiveInt }`. Pagination: more pages exist when `pageInfo.page < pageInfo.pageCount`.
- A theme's `downloadUrl` (e.g. `https://api.themezer.net/switch/themes/A24/download`) returns the `.nxtheme` directly. A pack download = download each member theme's `downloadUrl` individually (yields one `.nxtheme` per section).
- Preview thumbnails are JPEGs (`screenshotPreview.jpgThumbUrl` / `collagePreview.jpgThumbUrl`) — renderable via `brls::Image::setImageFromMem`.

## UI refinement locked here

The feed cannot cheaply merge two paginated sources, so the browser header has a **content toggle**: **[Packs | Temas]**. Default = **Packs** (complete looks, what most users want). The **[Parte ▾]** section filter is enabled only in **Temas** mode (packs have no target). This satisfies the spec's "packs + themes" coverage without merging feeds.

## File structure

- `source/core/themes/themezer_types.hpp` — data structs/enums (no logic).
- `source/core/themes/themezer_query.{hpp,cpp}` — build GraphQL POST bodies.
- `source/core/themes/themezer_json.{hpp,cpp}` — parse responses into structs.
- `source/core/themes/themezer_browse.{hpp,cpp}` — orchestration over an injected `GraphQlFetcher`.
- `source/platform/themes/theme_paths.{hpp,cpp}` — `themes_root()`, folder naming.
- `source/platform/themes/theme_download.{hpp,cpp}` — download a theme/pack to SD.
- `source/app/theme_browser_activity.{hpp,cpp}` + `resources/xml/activity/theme_browser.xml`.
- `source/app/theme_detail_activity.{hpp,cpp}` + `resources/xml/activity/theme_detail.xml`.
- `resources/i18n/{en-US,pt-BR}/themes.json` — `themes/` namespace.
- `resources/xml/activity/home.xml` + `source/app/home_activity.cpp` — "Temas" rail card.
- `tests/test_themezer_query.cpp`, `tests/test_themezer_json.cpp`, `tests/test_theme_paths.cpp`, `tests/Makefile`.

---

## Task 1: Wire new core/themes + platform/themes into the test build

**Files:**
- Modify: `tests/Makefile:3`

- [ ] **Step 1: Add the new core/themes glob and platform/themes file to test SRCS**

In `tests/Makefile`, change the `SRCS :=` line to add `$(wildcard ../source/core/themes/*.cpp)` and `../source/platform/themes/theme_paths.cpp`. The full line becomes:

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/mods/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) $(wildcard ../source/core/themes/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp ../source/platform/saves/fake_cloud_save_client.cpp ../source/platform/saves/http_cloud_save_client.cpp ../source/platform/saves/save_backup_io.cpp ../source/platform/mods/mod_store.cpp ../source/platform/themes/theme_paths.cpp
```

- [ ] **Step 2: Verify the suite still builds and passes (no new files yet)**

Run: `cd tests && make clean >/dev/null && make test 2>&1 | tail -3`
Expected: `Status: SUCCESS!` (the globs match nothing new yet, so this is a no-op safety check).

- [ ] **Step 3: Commit**

```bash
git add tests/Makefile
git commit -m "test(themes): include core/themes + theme_paths in the test build"
```

---

## Task 2: Theme data types

**Files:**
- Create: `source/core/themes/themezer_types.hpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

enum class ThemeKind { Theme, Pack };

// One downloadable .nxtheme. A standalone theme has exactly one; a pack has one
// per section it themes.
struct ThemePart {
    std::string hex_id;
    std::string target;        // e.g. "ResidentMenu" (may be empty)
    std::string name;
    std::string download_url;  // direct .nxtheme download
};

// One card in the browse grid.
struct ThemeEntry {
    ThemeKind     kind = ThemeKind::Theme;
    std::string   hex_id;        // Themezer hexId ("A24")
    std::string   name;
    std::string   author;        // creator.username
    std::string   target;        // theme section; empty for packs
    std::string   preview_url;   // jpgThumbUrl / collagePreview.jpgThumbUrl
    std::string   download_url;  // direct download (theme: .nxtheme; pack: archive)
    std::uint64_t downloads = 0; // downloadCount
};

// One page of browse results.
struct BrowsePage {
    std::vector<ThemeEntry> entries;
    int  page = 1;
    int  page_count = 1;
    bool is_complete = true;     // page >= page_count => no more pages
};

// Detail resolved from theme(hexId)/pack(hexId). `parts` unifies download: a
// standalone theme yields one part (itself); a pack yields its members.
struct ThemeDetail {
    ThemeEntry              entry;
    std::string             description;
    std::vector<ThemePart>  parts;
};

} // namespace thomaz::core
```

- [ ] **Step 2: Commit**

```bash
git add source/core/themes/themezer_types.hpp
git commit -m "feat(themes): core data types for Themezer entries"
```

---

## Task 3: GraphQL body builders

**Files:**
- Create: `source/core/themes/themezer_query.hpp`, `source/core/themes/themezer_query.cpp`
- Test: `tests/test_themezer_query.cpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include <string>

namespace thomaz::core {

// All builders return a ready-to-POST JSON body: {"query":...,"variables":...}.

// Themes feed/search. `query` empty => no text filter. `target` empty => no
// section filter (otherwise a Target enum value like "ResidentMenu").
std::string themes_feed_body(const std::string& query, const std::string& target,
                             int page, int limit);

// Packs feed/search. `query` empty => no text filter.
std::string packs_feed_body(const std::string& query, int page, int limit);

// Detail bodies. `hex_id` is sanitized to [0-9A-Fa-f] before being inlined.
std::string theme_detail_body(const std::string& hex_id);
std::string pack_detail_body(const std::string& hex_id);

} // namespace thomaz::core
```

- [ ] **Step 2: Write the failing test**

```cpp
#include "doctest.h"
#include "core/themes/themezer_query.hpp"
#include <nlohmann/json.hpp>

using namespace thomaz::core;
using nlohmann::json;

TEST_CASE("themes_feed_body: full filter set goes into variables") {
    json b = json::parse(themes_feed_body("zelda", "ResidentMenu", 2, 30));
    CHECK(b["query"].get<std::string>().find("themes(") != std::string::npos);
    CHECK(b["variables"]["q"] == "zelda");
    CHECK(b["variables"]["t"] == "ResidentMenu");
    CHECK(b["variables"]["p"]["page"] == 2);
    CHECK(b["variables"]["p"]["limit"] == 30);
}

TEST_CASE("themes_feed_body: empty query/target serialize as null (no filter)") {
    json b = json::parse(themes_feed_body("", "", 1, 30));
    CHECK(b["variables"]["q"].is_null());
    CHECK(b["variables"]["t"].is_null());
}

TEST_CASE("packs_feed_body has no target variable") {
    json b = json::parse(packs_feed_body("clean", 1, 30));
    CHECK(b["query"].get<std::string>().find("packs(") != std::string::npos);
    CHECK(b["variables"]["q"] == "clean");
    CHECK_FALSE(b["variables"].contains("t"));
}

TEST_CASE("detail bodies inline a sanitized hexId") {
    json t = json::parse(theme_detail_body("A2\"4 evil"));
    CHECK(t["query"].get<std::string>().find("theme(hexId:\"A24\")") != std::string::npos);
    json p = json::parse(pack_detail_body("16D"));
    CHECK(p["query"].get<std::string>().find("pack(hexId:\"16D\")") != std::string::npos);
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd tests && make test 2>&1 | tail -5`
Expected: FAIL — link error / `themes_feed_body` not defined.

- [ ] **Step 4: Write the implementation**

```cpp
#include "core/themes/themezer_query.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

namespace {
std::string sanitize_hex(const std::string& s) {
    std::string out;
    for (char c : s)
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            out.push_back(c);
    return out;
}
std::string wrap(const std::string& query, const json& variables) {
    json body;
    body["query"]     = query;
    body["variables"] = variables;
    return body.dump();
}
} // namespace

std::string themes_feed_body(const std::string& query, const std::string& target,
                             int page, int limit) {
    static const char* kQuery =
        "query($q:String,$t:Target,$p:PaginationInput){ switch{ themes("
        "query:$q, target:$t, sort:DOWNLOADS, order:DESC, paginationArgs:$p){ "
        "pageInfo{page pageCount} nodes{ hexId name downloadCount downloadUrl "
        "target creator{username} screenshotPreview{jpgThumbUrl} } } } }";
    json v;
    v["q"] = query.empty()  ? json(nullptr) : json(query);
    v["t"] = target.empty() ? json(nullptr) : json(target);
    v["p"] = { {"page", page}, {"limit", limit} };
    return wrap(kQuery, v);
}

std::string packs_feed_body(const std::string& query, int page, int limit) {
    static const char* kQuery =
        "query($q:String,$p:PaginationInput){ switch{ packs("
        "query:$q, sort:DOWNLOADS, order:DESC, paginationArgs:$p){ "
        "pageInfo{page pageCount} nodes{ hexId name downloadCount downloadUrl "
        "creator{username} collagePreview{jpgThumbUrl} } } } }";
    json v;
    v["q"] = query.empty() ? json(nullptr) : json(query);
    v["p"] = { {"page", page}, {"limit", limit} };
    return wrap(kQuery, v);
}

std::string theme_detail_body(const std::string& hex_id) {
    std::string q =
        "{ switch{ theme(hexId:\"" + sanitize_hex(hex_id) + "\"){ hexId name "
        "description downloadUrl target creator{username} "
        "screenshotPreview{jpgThumbUrl} } } }";
    return wrap(q, json::object());
}

std::string pack_detail_body(const std::string& hex_id) {
    std::string q =
        "{ switch{ pack(hexId:\"" + sanitize_hex(hex_id) + "\"){ hexId name "
        "description downloadUrl creator{username} collagePreview{jpgThumbUrl} "
        "themes{ hexId name target downloadUrl } } } }";
    return wrap(q, json::object());
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd tests && make test 2>&1 | tail -3`
Expected: `Status: SUCCESS!`

- [ ] **Step 6: Commit**

```bash
git add source/core/themes/themezer_query.hpp source/core/themes/themezer_query.cpp tests/test_themezer_query.cpp
git commit -m "feat(themes): GraphQL body builders + tests"
```

---

## Task 4: Response parsers

**Files:**
- Create: `source/core/themes/themezer_json.hpp`, `source/core/themes/themezer_json.cpp`
- Test: `tests/test_themezer_json.cpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "core/themes/themezer_types.hpp"
#include <string>

namespace thomaz::core {

// Parse a themes/packs feed response. `kind` selects which list to read
// (data.switch.themes vs data.switch.packs) and stamps each entry's kind.
// Returns an empty page (page_count 1, is_complete true) on malformed input.
BrowsePage parse_browse_page(const std::string& body, ThemeKind kind);

// Parse a theme(hexId) detail response. `found` is set false when the node is
// null/absent (theme doesn't exist). A standalone theme yields one part.
ThemeDetail parse_theme_detail(const std::string& body, bool* found);

// Parse a pack(hexId) detail response. parts = the pack's member themes.
ThemeDetail parse_pack_detail(const std::string& body, bool* found);

} // namespace thomaz::core
```

- [ ] **Step 2: Write the failing test (fixtures trimmed from real API responses)**

```cpp
#include "doctest.h"
#include "core/themes/themezer_json.hpp"

using namespace thomaz::core;

static const char* THEMES_JSON = R"json({"data":{"switch":{"themes":{
  "pageInfo":{"page":1,"pageCount":3},
  "nodes":[
    {"hexId":"A24","name":"Purple Skies","downloadCount":105079,
     "downloadUrl":"https://api.themezer.net/switch/themes/A24/download",
     "target":"ResidentMenu","creator":{"username":"Hsushi"},
     "screenshotPreview":{"jpgThumbUrl":"https://img/x.jpg"}}
  ]}}}})json";

static const char* PACKS_JSON = R"json({"data":{"switch":{"packs":{
  "pageInfo":{"page":2,"pageCount":2},
  "nodes":[
    {"hexId":"16D","name":"Project Clean","downloadCount":57870,
     "downloadUrl":"https://api.themezer.net/switch/packs/16D/download",
     "creator":{"username":"usiruktv"},
     "collagePreview":{"jpgThumbUrl":"https://img/c.jpg"}}
  ]}}}})json";

static const char* PACK_DETAIL_JSON = R"json({"data":{"switch":{"pack":{
  "hexId":"16D","name":"Project Clean","description":"clean",
  "downloadUrl":"https://api.themezer.net/switch/packs/16D/download",
  "creator":{"username":"usiruktv"},
  "collagePreview":{"jpgThumbUrl":"https://img/c.jpg"},
  "themes":[
    {"hexId":"9A6","name":"Home","target":"ResidentMenu",
     "downloadUrl":"https://api.themezer.net/switch/themes/9A6/download"},
    {"hexId":"9A7","name":"Lock","target":"Entrance",
     "downloadUrl":"https://api.themezer.net/switch/themes/9A7/download"}
  ]}}}})json";

TEST_CASE("parse_browse_page reads themes nodes + pagination") {
    BrowsePage p = parse_browse_page(THEMES_JSON, ThemeKind::Theme);
    REQUIRE(p.entries.size() == 1);
    CHECK(p.entries[0].kind == ThemeKind::Theme);
    CHECK(p.entries[0].hex_id == "A24");
    CHECK(p.entries[0].name == "Purple Skies");
    CHECK(p.entries[0].author == "Hsushi");
    CHECK(p.entries[0].target == "ResidentMenu");
    CHECK(p.entries[0].downloads == 105079);
    CHECK(p.entries[0].preview_url == "https://img/x.jpg");
    CHECK(p.entries[0].download_url == "https://api.themezer.net/switch/themes/A24/download");
    CHECK(p.page == 1);
    CHECK(p.page_count == 3);
    CHECK_FALSE(p.is_complete);          // 1 < 3
}

TEST_CASE("parse_browse_page reads packs + collage preview, last page complete") {
    BrowsePage p = parse_browse_page(PACKS_JSON, ThemeKind::Pack);
    REQUIRE(p.entries.size() == 1);
    CHECK(p.entries[0].kind == ThemeKind::Pack);
    CHECK(p.entries[0].target.empty());
    CHECK(p.entries[0].preview_url == "https://img/c.jpg");
    CHECK(p.is_complete);                // 2 >= 2
}

TEST_CASE("parse_browse_page returns empty page on garbage") {
    BrowsePage p = parse_browse_page("not json", ThemeKind::Theme);
    CHECK(p.entries.empty());
    CHECK(p.is_complete);
}

TEST_CASE("parse_pack_detail expands member themes into parts") {
    bool found = false;
    ThemeDetail d = parse_pack_detail(PACK_DETAIL_JSON, &found);
    REQUIRE(found);
    CHECK(d.entry.kind == ThemeKind::Pack);
    CHECK(d.entry.name == "Project Clean");
    REQUIRE(d.parts.size() == 2);
    CHECK(d.parts[0].target == "ResidentMenu");
    CHECK(d.parts[1].download_url == "https://api.themezer.net/switch/themes/9A7/download");
}

TEST_CASE("parse_theme_detail yields a single self part; missing node => not found") {
    const char* TH = R"json({"data":{"switch":{"theme":{
      "hexId":"A24","name":"Purple","description":"d",
      "downloadUrl":"https://api.themezer.net/switch/themes/A24/download",
      "target":"ResidentMenu","creator":{"username":"Hsushi"},
      "screenshotPreview":{"jpgThumbUrl":"https://img/x.jpg"}}}}})json";
    bool found = false;
    ThemeDetail d = parse_theme_detail(TH, &found);
    REQUIRE(found);
    CHECK(d.entry.kind == ThemeKind::Theme);
    REQUIRE(d.parts.size() == 1);
    CHECK(d.parts[0].hex_id == "A24");
    CHECK(d.parts[0].download_url == "https://api.themezer.net/switch/themes/A24/download");

    bool found2 = true;
    ThemeDetail miss = parse_theme_detail(R"json({"data":{"switch":{"theme":null}}})json", &found2);
    CHECK_FALSE(found2);
    CHECK(miss.parts.empty());
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd tests && make test 2>&1 | tail -5`
Expected: FAIL — `parse_browse_page` not defined.

- [ ] **Step 4: Write the implementation**

```cpp
#include "core/themes/themezer_json.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

namespace {
// Walk data.switch.<key>; returns a null json if any hop is missing.
const json& switch_node(const json& doc, const char* key) {
    static const json kNull;
    if (!doc.is_object() || !doc.contains("data")) return kNull;
    const json& data = doc["data"];
    if (!data.is_object() || !data.contains("switch")) return kNull;
    const json& sw = data["switch"];
    if (!sw.is_object() || !sw.contains(key)) return kNull;
    return sw[key];
}

std::string author_of(const json& node) {
    if (node.contains("creator") && node["creator"].is_object())
        return node["creator"].value("username", std::string());
    return std::string();
}

std::string preview_of(const json& node) {
    for (const char* k : {"screenshotPreview", "collagePreview"}) {
        if (node.contains(k) && node[k].is_object())
            return node[k].value("jpgThumbUrl", std::string());
    }
    return std::string();
}

ThemeEntry entry_of(const json& node, ThemeKind kind) {
    ThemeEntry e;
    e.kind         = kind;
    e.hex_id       = node.value("hexId", std::string());
    e.name         = node.value("name", std::string());
    e.author       = author_of(node);
    e.target       = node.value("target", std::string());
    e.preview_url  = preview_of(node);
    e.download_url = node.value("downloadUrl", std::string());
    e.downloads    = node.value("downloadCount", (std::uint64_t)0);
    return e;
}
} // namespace

BrowsePage parse_browse_page(const std::string& body, ThemeKind kind) {
    BrowsePage page;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) return page;

    const json& list = switch_node(doc, kind == ThemeKind::Theme ? "themes" : "packs");
    if (!list.is_object()) return page;

    if (list.contains("pageInfo") && list["pageInfo"].is_object()) {
        page.page       = list["pageInfo"].value("page", 1);
        page.page_count = list["pageInfo"].value("pageCount", 1);
    }
    page.is_complete = page.page >= page.page_count;

    if (list.contains("nodes") && list["nodes"].is_array()) {
        for (const json& n : list["nodes"]) {
            if (!n.is_object()) continue;
            try { page.entries.push_back(entry_of(n, kind)); }
            catch (const json::exception&) { continue; }
        }
    }
    return page;
}

ThemeDetail parse_theme_detail(const std::string& body, bool* found) {
    if (found) *found = false;
    ThemeDetail d;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) return d;

    const json& node = switch_node(doc, "theme");
    if (!node.is_object()) return d;

    d.entry       = entry_of(node, ThemeKind::Theme);
    d.description = node.value("description", std::string());
    ThemePart self;
    self.hex_id       = d.entry.hex_id;
    self.target       = d.entry.target;
    self.name         = d.entry.name;
    self.download_url = d.entry.download_url;
    d.parts.push_back(self);
    if (found) *found = true;
    return d;
}

ThemeDetail parse_pack_detail(const std::string& body, bool* found) {
    if (found) *found = false;
    ThemeDetail d;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) return d;

    const json& node = switch_node(doc, "pack");
    if (!node.is_object()) return d;

    d.entry       = entry_of(node, ThemeKind::Pack);
    d.description = node.value("description", std::string());
    if (node.contains("themes") && node["themes"].is_array()) {
        for (const json& t : node["themes"]) {
            if (!t.is_object()) continue;
            ThemePart p;
            p.hex_id       = t.value("hexId", std::string());
            p.target       = t.value("target", std::string());
            p.name         = t.value("name", std::string());
            p.download_url = t.value("downloadUrl", std::string());
            d.parts.push_back(p);
        }
    }
    if (found) *found = true;
    return d;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd tests && make test 2>&1 | tail -3`
Expected: `Status: SUCCESS!`

- [ ] **Step 6: Commit**

```bash
git add source/core/themes/themezer_json.hpp source/core/themes/themezer_json.cpp tests/test_themezer_json.cpp
git commit -m "feat(themes): response parsers + tests"
```

---

## Task 5: Browse orchestration layer

**Files:**
- Create: `source/core/themes/themezer_browse.hpp`, `source/core/themes/themezer_browse.cpp`
- Test: `tests/test_themezer_browse.cpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "core/themes/themezer_types.hpp"
#include <functional>
#include <optional>
#include <string>

namespace thomaz::core {

// Performs the GraphQL POST: takes the request body, returns the response body,
// or nullopt on transport failure. Injected so the core stays testable.
using GraphQlFetcher = std::function<std::optional<std::string>(const std::string& body)>;

enum class BrowseStatus { Ok, NetworkError };
struct BrowseResult {
    BrowseStatus status = BrowseStatus::NetworkError;
    BrowsePage   page;
};

// Browse themes (section filter via `target`, empty = all) or packs.
BrowseResult browse_themes(const std::string& query, const std::string& target,
                           int page, int limit, const GraphQlFetcher& fetch);
BrowseResult browse_packs(const std::string& query, int page, int limit,
                          const GraphQlFetcher& fetch);

enum class DetailStatus { Ok, NotFound, NetworkError };
struct DetailResult {
    DetailStatus status = DetailStatus::NetworkError;
    ThemeDetail  detail;
};

DetailResult theme_detail(const std::string& hex_id, const GraphQlFetcher& fetch);
DetailResult pack_detail(const std::string& hex_id, const GraphQlFetcher& fetch);

} // namespace thomaz::core
```

- [ ] **Step 2: Write the failing test**

```cpp
#include "doctest.h"
#include "core/themes/themezer_browse.hpp"

using namespace thomaz::core;

static GraphQlFetcher constFetcher(std::string resp) {
    return [resp](const std::string&) -> std::optional<std::string> { return resp; };
}
static GraphQlFetcher failFetcher() {
    return [](const std::string&) -> std::optional<std::string> { return std::nullopt; };
}

TEST_CASE("browse_themes maps a good response to Ok + entries") {
    auto f = constFetcher(R"json({"data":{"switch":{"themes":{
      "pageInfo":{"page":1,"pageCount":1},
      "nodes":[{"hexId":"A24","name":"X","downloadUrl":"u","target":"Set",
                "creator":{"username":"a"},"screenshotPreview":{"jpgThumbUrl":"p"}}]}}}})json");
    BrowseResult r = browse_themes("", "", 1, 30, f);
    REQUIRE(r.status == BrowseStatus::Ok);
    REQUIRE(r.page.entries.size() == 1);
    CHECK(r.page.entries[0].hex_id == "A24");
}

TEST_CASE("browse_packs reports NetworkError on transport failure") {
    BrowseResult r = browse_packs("", 1, 30, failFetcher());
    CHECK(r.status == BrowseStatus::NetworkError);
    CHECK(r.page.entries.empty());
}

TEST_CASE("pack_detail maps a missing node to NotFound") {
    auto f = constFetcher(R"json({"data":{"switch":{"pack":null}}})json");
    DetailResult r = pack_detail("16D", f);
    CHECK(r.status == DetailStatus::NotFound);
}

TEST_CASE("theme_detail Ok yields one part") {
    auto f = constFetcher(R"json({"data":{"switch":{"theme":{
      "hexId":"A24","name":"X","downloadUrl":"u","target":"Set",
      "creator":{"username":"a"},"screenshotPreview":{"jpgThumbUrl":"p"}}}}})json");
    DetailResult r = theme_detail("A24", f);
    REQUIRE(r.status == DetailStatus::Ok);
    REQUIRE(r.detail.parts.size() == 1);
    CHECK(r.detail.parts[0].download_url == "u");
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd tests && make test 2>&1 | tail -5`
Expected: FAIL — `browse_themes` not defined.

- [ ] **Step 4: Write the implementation**

```cpp
#include "core/themes/themezer_browse.hpp"
#include "core/themes/themezer_query.hpp"
#include "core/themes/themezer_json.hpp"

namespace thomaz::core {

BrowseResult browse_themes(const std::string& query, const std::string& target,
                           int page, int limit, const GraphQlFetcher& fetch) {
    BrowseResult r;
    auto body = fetch(themes_feed_body(query, target, page, limit));
    if (!body) return r; // NetworkError
    r.page   = parse_browse_page(*body, ThemeKind::Theme);
    r.status = BrowseStatus::Ok;
    return r;
}

BrowseResult browse_packs(const std::string& query, int page, int limit,
                          const GraphQlFetcher& fetch) {
    BrowseResult r;
    auto body = fetch(packs_feed_body(query, page, limit));
    if (!body) return r;
    r.page   = parse_browse_page(*body, ThemeKind::Pack);
    r.status = BrowseStatus::Ok;
    return r;
}

DetailResult theme_detail(const std::string& hex_id, const GraphQlFetcher& fetch) {
    DetailResult r;
    auto body = fetch(theme_detail_body(hex_id));
    if (!body) return r; // NetworkError
    bool found = false;
    r.detail = parse_theme_detail(*body, &found);
    r.status = found ? DetailStatus::Ok : DetailStatus::NotFound;
    return r;
}

DetailResult pack_detail(const std::string& hex_id, const GraphQlFetcher& fetch) {
    DetailResult r;
    auto body = fetch(pack_detail_body(hex_id));
    if (!body) return r;
    bool found = false;
    r.detail = parse_pack_detail(*body, &found);
    r.status = found ? DetailStatus::Ok : DetailStatus::NotFound;
    return r;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd tests && make test 2>&1 | tail -3`
Expected: `Status: SUCCESS!`

- [ ] **Step 6: Commit**

```bash
git add source/core/themes/themezer_browse.hpp source/core/themes/themezer_browse.cpp tests/test_themezer_browse.cpp
git commit -m "feat(themes): browse/detail orchestration over injected fetcher + tests"
```

---

## Task 6: Theme paths + download destination

**Files:**
- Create: `source/platform/themes/theme_paths.hpp`, `source/platform/themes/theme_paths.cpp`
- Test: `tests/test_theme_paths.cpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "core/themes/themezer_types.hpp"
#include <string>

namespace thomaz {

// Root holding downloaded themes. Switch -> "/themes" (NXThemes Installer reads
// sd:/themes/); desktop -> "themes" (working dir).
std::string themes_root();

// <root>/<sanitized "Author - Name">  — one folder per theme/pack download.
std::string theme_folder(const thomaz::core::ThemeEntry& entry);

// True if theme_folder(entry) already exists (drives the "downloaded" badge).
bool theme_already_downloaded(const thomaz::core::ThemeEntry& entry);

} // namespace thomaz
```

- [ ] **Step 2: Write the failing test**

```cpp
#include "doctest.h"
#include "platform/themes/theme_paths.hpp"
#include <filesystem>

using namespace thomaz;
using thomaz::core::ThemeEntry;

TEST_CASE("theme_folder composes root + sanitized 'author - name'") {
    ThemeEntry e;
    e.author = "Hsushi";
    e.name   = "Purple/Skies: Home?";   // unsafe path chars
    std::string f = theme_folder(e);
    CHECK(f == themes_root() + "/Hsushi - Purple_Skies_ Home_");
}

TEST_CASE("theme_already_downloaded reflects folder existence") {
    namespace fs = std::filesystem;
    ThemeEntry e; e.author = "T"; e.name = "Exists";
    fs::remove_all(theme_folder(e));
    CHECK_FALSE(theme_already_downloaded(e));
    fs::create_directories(theme_folder(e));
    CHECK(theme_already_downloaded(e));
    fs::remove_all(theme_folder(e));
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd tests && make test 2>&1 | tail -5`
Expected: FAIL — `themes_root` not defined.

- [ ] **Step 4: Write the implementation**

```cpp
#include "platform/themes/theme_paths.hpp"
#include <sys/stat.h>

namespace thomaz {

std::string themes_root() {
#ifdef __SWITCH__
    return "/themes";
#else
    return "themes";
#endif
}

namespace {
// Replace filesystem-unsafe characters with '_'. Mirrors how a human-named
// folder must be safe on FAT32.
std::string sanitize_component(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '/': case '\\': case ':': case '*': case '?':
            case '"': case '<': case '>': case '|':
                out.push_back('_'); break;
            default: out.push_back(c);
        }
    }
    return out;
}
} // namespace

std::string theme_folder(const thomaz::core::ThemeEntry& entry) {
    std::string label = sanitize_component(entry.author) + " - " +
                        sanitize_component(entry.name);
    return themes_root() + "/" + label;
}

bool theme_already_downloaded(const thomaz::core::ThemeEntry& entry) {
    struct stat st;
    std::string f = theme_folder(entry);
    return ::stat(f.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace thomaz
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd tests && make test 2>&1 | tail -3`
Expected: `Status: SUCCESS!`

- [ ] **Step 6: Commit**

```bash
git add source/platform/themes/theme_paths.hpp source/platform/themes/theme_paths.cpp tests/test_theme_paths.cpp
git commit -m "feat(themes): SD theme paths + downloaded check + tests"
```

---

## Task 7: Theme download (IO; no unit test — IO + network)

**Files:**
- Create: `source/platform/themes/theme_download.hpp`, `source/platform/themes/theme_download.cpp`

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "core/themes/themezer_types.hpp"
#include <string>

namespace thomaz {

struct ThemeDownloadResult {
    bool ok = false;
    std::string error;
};

// Download every part of `detail` into theme_folder(detail.entry) as
// "<target or index>.nxtheme". A standalone theme writes one file; a pack writes
// one per section. On any failure the partial folder is removed.
ThemeDownloadResult download_theme(const thomaz::core::ThemeDetail& detail);

} // namespace thomaz
```

- [ ] **Step 2: Write the implementation**

```cpp
#include "platform/themes/theme_download.hpp"
#include "platform/themes/theme_paths.hpp"
#include "platform/mods/mod_store.hpp"     // remove_tree
#include "platform/mods/mod_download.hpp"  // download_file

#include <sys/stat.h>

namespace thomaz {

namespace {
std::string part_filename(const thomaz::core::ThemePart& p, int index) {
    std::string base = p.target.empty() ? ("theme" + std::to_string(index)) : p.target;
    // targets are enum-safe ASCII already; keep simple.
    return base + ".nxtheme";
}
} // namespace

ThemeDownloadResult download_theme(const thomaz::core::ThemeDetail& detail) {
    ThemeDownloadResult res;
    if (detail.parts.empty()) {
        res.error = "nothing to download";
        return res;
    }

    std::string folder = theme_folder(detail.entry);
    ::mkdir(themes_root().c_str(), 0777); // ensure root exists (best-effort)
    ::mkdir(folder.c_str(), 0777);

    int index = 0;
    for (const auto& part : detail.parts) {
        std::string dest = folder + "/" + part_filename(part, index++);
        std::string err;
        if (!download_file(part.download_url, dest, nullptr, &err)) {
            remove_tree(folder); // no half-written theme left behind
            res.error = err.empty() ? "download failed" : err;
            return res;
        }
    }
    res.ok = true;
    return res;
}

} // namespace thomaz
```

- [ ] **Step 3: Verify it compiles in the desktop build (no unit test for IO)**

Run: `cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build_desktop -j"$(nproc)" 2>&1 | tail -3`
Expected: `Built target thomaz` (the new files are picked up by GLOB_RECURSE after the reconfigure).

- [ ] **Step 4: Commit**

```bash
git add source/platform/themes/theme_download.hpp source/platform/themes/theme_download.cpp
git commit -m "feat(themes): download a theme/pack to sd:/themes/"
```

---

## Task 8: i18n strings (themes namespace)

**Files:**
- Create: `resources/i18n/en-US/themes.json`, `resources/i18n/pt-BR/themes.json`

- [ ] **Step 1: Write en-US/themes.json**

```json
{
    "title": "Themes",
    "home": "Themes",
    "tab_packs": "Packs",
    "tab_themes": "Themes",
    "filter_part": "Section",
    "part_all": "All",
    "part_ResidentMenu": "Home",
    "part_Entrance": "Lock screen",
    "part_Flaunch": "All apps",
    "part_Set": "Settings",
    "part_Psl": "Player select",
    "part_MyPage": "My Page",
    "part_News": "News",
    "part_Notification": "Notifications",
    "search": "Search themes",
    "search_hint": "Type a theme name",
    "loading": "Loading…",
    "no_results": "No themes found.",
    "load_more": "Load more",
    "downloads": "downloads",
    "downloaded": "Downloaded",
    "by": "by",
    "pack_parts": "This pack includes:",
    "download": "Download",
    "downloading": "Downloading…",
    "download_ok": "Saved to sd:/themes/. Install it with NXThemes Installer.",
    "download_fail": "Download failed",
    "error_network": "Couldn't reach Themezer. Check your connection."
}
```

- [ ] **Step 2: Write pt-BR/themes.json**

```json
{
    "title": "Temas",
    "home": "Temas",
    "tab_packs": "Packs",
    "tab_themes": "Temas",
    "filter_part": "Parte",
    "part_all": "Todas",
    "part_ResidentMenu": "Início",
    "part_Entrance": "Tela de bloqueio",
    "part_Flaunch": "Todos os apps",
    "part_Set": "Configurações",
    "part_Psl": "Seleção de jogador",
    "part_MyPage": "Minha Página",
    "part_News": "Novidades",
    "part_Notification": "Notificações",
    "search": "Buscar temas",
    "search_hint": "Digite o nome do tema",
    "loading": "Carregando…",
    "no_results": "Nenhum tema encontrado.",
    "load_more": "Carregar mais",
    "downloads": "downloads",
    "downloaded": "Baixado",
    "by": "por",
    "pack_parts": "Este pack inclui:",
    "download": "Baixar",
    "downloading": "Baixando…",
    "download_ok": "Salvo em sd:/themes/. Instale com o NXThemes Installer.",
    "download_fail": "Falha no download",
    "error_network": "Não consegui acessar o Themezer. Verifique a conexão."
}
```

- [ ] **Step 3: Validate JSON**

Run: `python3 -c "import json;json.load(open('resources/i18n/en-US/themes.json'));json.load(open('resources/i18n/pt-BR/themes.json'));print('JSON OK')"`
Expected: `JSON OK`

- [ ] **Step 4: Commit**

```bash
git add resources/i18n/en-US/themes.json resources/i18n/pt-BR/themes.json
git commit -m "feat(themes): i18n strings (en-US + pt-BR)"
```

---

## Task 9: Theme detail activity + XML

**Files:**
- Create: `resources/xml/activity/theme_detail.xml`, `source/app/theme_detail_activity.hpp`, `source/app/theme_detail_activity.cpp`

> Build this before the browser so the browser can push it.

- [ ] **Step 1: Write the XML**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<brls:AppletFrame id="themeDetailFrame" title="@i18n/themes/title" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0" paddingTop="28" paddingBottom="24" paddingLeft="40" paddingRight="40">
        <brls:Image id="detailPreview" width="420" height="236" cornerRadius="12" marginBottom="16"/>
        <brls:Label id="detailName" fontSize="24" textColor="#FFFFFF"/>
        <brls:Label id="detailAuthor" fontSize="16" textColor="@theme/thomaz/text_dim" marginBottom="8"/>
        <brls:Label id="detailDesc" fontSize="15" textColor="@theme/thomaz/text_dim" marginBottom="16"/>
        <brls:ProgressSpinner id="spinner" width="40" height="40" visibility="gone"/>
        <brls:Label id="partsLabel" fontSize="15" textColor="@theme/thomaz/text_dim" visibility="gone" marginBottom="6"/>
        <brls:Box id="partsBox" axis="column" marginBottom="16"/>
        <brls:Box id="downloadButton" axis="row" height="56" cornerRadius="10"
                  justifyContent="center" alignItems="center" focusable="true"
                  hideHighlightBackground="true" backgroundColor="@theme/thomaz/accent_bright">
            <brls:Label text="@i18n/themes/download" fontSize="18" textColor="#FFFFFF"/>
        </brls:Box>
        <brls:Label id="downloadNote" fontSize="13" textColor="@theme/thomaz/text_dim" marginTop="10"/>
    </brls:Box>
</brls:AppletFrame>
```

- [ ] **Step 2: Write the header**

```cpp
#pragma once
#include <atomic>
#include <memory>

#include <borealis.hpp>

#include "core/themes/themezer_types.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Resolves a theme/pack's detail (download URLs + preview), then downloads it
// to sd:/themes/ on demand. Phase A stops at downloading the .nxtheme files.
class ThemeDetailActivity : public brls::Activity {
  public:
    ThemeDetailActivity(thomaz::core::ThemeEntry entry, IHttpClient* http);
    ~ThemeDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/theme_detail.xml");
    void onContentAvailable() override;

  private:
    void onResolved(const thomaz::core::ThemeDetail& detail, bool ok);
    void startDownload();

    thomaz::core::ThemeEntry  entry;
    thomaz::core::ThemeDetail detail;
    bool                      resolved = false;
    IHttpClient*              http;
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Write the implementation**

```cpp
#include "app/theme_detail_activity.hpp"
#include "app/app_header.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>
#include <optional>
#include <string>

#include "core/themes/themezer_browse.hpp"
#include "platform/themes/theme_download.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {
// POST GraphQL via the app's http client; returns the response body or nullopt.
core::GraphQlFetcher makeFetcher(IHttpClient* http) {
    return [http](const std::string& body) -> std::optional<std::string> {
        HttpRequest req;
        req.method = HttpMethod::Post;
        req.url    = "https://api.themezer.net/graphql";
        req.headers.push_back({ "Content-Type", "application/json" });
        req.body = body;
        HttpResponse r = http->request(req);
        return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
    };
}
} // namespace

ThemeDetailActivity::ThemeDetailActivity(core::ThemeEntry entry, IHttpClient* http)
    : entry(std::move(entry)), http(http) {}

ThemeDetailActivity::~ThemeDetailActivity() { *this->alive = false; }

void ThemeDetailActivity::onContentAvailable() {
    install_header_username(this);

    // Header from what we already have.
    if (auto* name = (brls::Label*)this->getView("detailName")) name->setText(this->entry.name);
    if (auto* author = (brls::Label*)this->getView("detailAuthor"))
        author->setText("themes/by"_i18n + std::string(" ") + this->entry.author);
    if (auto* note = (brls::Label*)this->getView("downloadNote"))
        note->setText("themes/download_ok"_i18n);

    // Preview (async image).
    if (!this->entry.preview_url.empty()) {
        std::string url = this->entry.preview_url;
        IHttpClient* client = this->http;
        auto alive = this->alive;
        brls::async([this, client, url, alive]() {
            HttpResponse r = client->get(url);
            if (!r.ok()) return;
            std::string body = r.body;
            brls::sync([this, alive, body]() {
                if (!alive->load()) return;
                if (auto* img = (brls::Image*)this->getView("detailPreview"))
                    img->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
            });
        });
    }

    // Spinner while resolving the detail (download URLs / pack parts).
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::VISIBLE);

    core::ThemeEntry e = this->entry;
    IHttpClient* client = this->http;
    auto alive = this->alive;
    brls::async([this, e, client, alive]() {
        core::GraphQlFetcher fetch = makeFetcher(client);
        core::DetailResult res = (e.kind == core::ThemeKind::Pack)
            ? core::pack_detail(e.hex_id, fetch)
            : core::theme_detail(e.hex_id, fetch);
        bool ok = (res.status == core::DetailStatus::Ok);
        core::ThemeDetail d = res.detail;
        brls::sync([this, alive, d, ok]() {
            if (!alive->load()) return;
            this->onResolved(d, ok);
        });
    });

    if (auto* btn = this->getView("downloadButton")) {
        btn->registerClickAction([this](brls::View*) {
            brls::sync([this]() { this->startDownload(); });
            return true;
        });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }
}

void ThemeDetailActivity::onResolved(const core::ThemeDetail& d, bool ok) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
    if (!ok) {
        brls::Application::notify("themes/error_network"_i18n);
        return;
    }
    this->detail   = d;
    this->resolved = true;

    if (auto* desc = (brls::Label*)this->getView("detailDesc"))
        desc->setText(d.description);

    // Pack: list the parts that will be saved.
    if (d.entry.kind == core::ThemeKind::Pack) {
        if (auto* pl = (brls::Label*)this->getView("partsLabel")) {
            pl->setText("themes/pack_parts"_i18n);
            pl->setVisibility(brls::Visibility::VISIBLE);
        }
        if (auto* box = (brls::Box*)this->getView("partsBox")) {
            for (const auto& p : d.parts) {
                auto* row = new brls::Label();
                row->setText(std::string("• ") + (p.name.empty() ? p.target : p.name));
                row->setFontSize(14.0f);
                row->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
                box->addView(row);
            }
        }
    }
}

void ThemeDetailActivity::startDownload() {
    if (!this->resolved) return;
    brls::Application::notify("themes/downloading"_i18n);

    core::ThemeDetail d = this->detail;
    auto alive = this->alive;
    brls::async([alive, d]() {
        ThemeDownloadResult r = download_theme(d);
        std::string msg = r.ok ? "themes/download_ok"_i18n
                               : ("themes/download_fail"_i18n + std::string(": ") + r.error);
        brls::sync([alive, msg, ok = r.ok]() {
            if (!alive->load()) return;
            brls::Application::notify(msg);
            if (ok) brls::Application::popActivity();
        });
    });
}

} // namespace thomaz
```

- [ ] **Step 4: Reconfigure + build desktop**

Run: `cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build_desktop -j"$(nproc)" 2>&1 | tail -3`
Expected: `Built target thomaz`

- [ ] **Step 5: Commit**

```bash
git add resources/xml/activity/theme_detail.xml source/app/theme_detail_activity.hpp source/app/theme_detail_activity.cpp
git commit -m "feat(themes): theme/pack detail activity with download"
```

---

## Task 10: Theme browser activity + XML

**Files:**
- Create: `resources/xml/activity/theme_browser.xml`, `source/app/theme_browser_activity.hpp`, `source/app/theme_browser_activity.cpp`

- [ ] **Step 1: Write the XML**

```xml
<?xml version="1.0" encoding="UTF-8"?>
<brls:AppletFrame id="themeBrowserFrame" title="@i18n/themes/title" iconInterpolation="linear">
    <brls:ScrollingFrame width="auto" height="auto" grow="1.0">
        <brls:Box width="auto" height="auto" axis="column" paddingTop="28" paddingLeft="40" paddingRight="40" paddingBottom="24">
            <brls:Box axis="row" alignItems="center" marginBottom="14">
                <brls:Box id="tabPacks" axis="row" height="40" paddingLeft="16" paddingRight="16" cornerRadius="8"
                          alignItems="center" focusable="true" hideHighlightBackground="true" marginRight="8"
                          backgroundColor="@theme/thomaz/accent_bright">
                    <brls:Label text="@i18n/themes/tab_packs" fontSize="15" textColor="#FFFFFF"/>
                </brls:Box>
                <brls:Box id="tabThemes" axis="row" height="40" paddingLeft="16" paddingRight="16" cornerRadius="8"
                          alignItems="center" focusable="true" hideHighlightBackground="true" marginRight="8"
                          backgroundColor="#22242D">
                    <brls:Label text="@i18n/themes/tab_themes" fontSize="15" textColor="#FFFFFF"/>
                </brls:Box>
                <brls:Box id="searchButton" axis="row" height="40" paddingLeft="16" paddingRight="16" cornerRadius="8"
                          alignItems="center" focusable="true" hideHighlightBackground="true" marginRight="8"
                          backgroundColor="#22242D">
                    <brls:Label text="@i18n/themes/search" fontSize="15" textColor="#FFFFFF"/>
                </brls:Box>
                <brls:Box id="partButton" axis="row" height="40" paddingLeft="16" paddingRight="16" cornerRadius="8"
                          alignItems="center" focusable="true" hideHighlightBackground="true" visibility="gone"
                          backgroundColor="#22242D">
                    <brls:Label id="partButtonLabel" text="@i18n/themes/filter_part" fontSize="15" textColor="#FFFFFF"/>
                </brls:Box>
            </brls:Box>
            <brls:ProgressSpinner id="spinner" width="44" height="44" marginTop="24" alignSelf="center" visibility="gone"/>
            <brls:Label id="emptyLabel" visibility="gone" text="@i18n/themes/no_results" fontSize="18" horizontalAlign="center"/>
            <AnimatedBox id="resultsBox" width="auto" height="auto" entranceDelay="60" axis="column"/>
        </brls:Box>
    </brls:ScrollingFrame>
</brls:AppletFrame>
```

- [ ] **Step 2: Write the header**

```cpp
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "core/themes/themezer_browse.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Browses Themezer as a grid: a Packs/Themes toggle, free-text search, a section
// filter (Themes mode only), and a load-more row. Thumbnails load async.
class ThemeBrowserActivity : public brls::Activity {
  public:
    explicit ThemeBrowserActivity(IHttpClient* http);
    ~ThemeBrowserActivity() override;

    CONTENT_FROM_XML_RES("activity/theme_browser.xml");
    void onContentAvailable() override;

  private:
    void reload();                                   // re-query page 1 with current mode/query/target
    void runQuery(int page, bool append);
    void populate(const thomaz::core::BrowsePage& page, bool append);
    void loadThumb(const std::string& url, brls::Image* into);
    void openSearch();
    void cyclePart();                                // advance the section filter

    IHttpClient* http;
    bool         packsMode = true;                   // start on Packs
    std::string  query;
    std::string  target;                             // "" = all (Themes mode only)
    int          page = 1;
    bool         isComplete = true;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Write the implementation**

```cpp
#include "app/theme_browser_activity.hpp"
#include "app/app_header.hpp"
#include "app/theme_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>
#include <optional>
#include <string>

#include "core/themes/themezer_browse.hpp"
#include "platform/themes/theme_paths.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {
const char* kTargets[] = { "", "ResidentMenu", "Entrance", "Flaunch",
                           "Set", "Psl", "MyPage", "Notification" };

core::GraphQlFetcher makeFetcher(IHttpClient* http) {
    return [http](const std::string& body) -> std::optional<std::string> {
        HttpRequest req;
        req.method = HttpMethod::Post;
        req.url    = "https://api.themezer.net/graphql";
        req.headers.push_back({ "Content-Type", "application/json" });
        req.body   = body;
        HttpResponse r = http->request(req);
        return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
    };
}

std::string partLabel(const std::string& target) {
    if (target.empty()) return "themes/part_all"_i18n;
    return brls::getStr("themes/part_" + target);
}
} // namespace

ThemeBrowserActivity::ThemeBrowserActivity(IHttpClient* http) : http(http) {}
ThemeBrowserActivity::~ThemeBrowserActivity() { *this->alive = false; }

void ThemeBrowserActivity::onContentAvailable() {
    install_header_username(this);

    if (auto* tp = this->getView("tabPacks")) {
        tp->registerClickAction([this](brls::View*) {
            this->packsMode = true; this->target = "";
            if (auto* pb = this->getView("partButton")) pb->setVisibility(brls::Visibility::GONE);
            brls::sync([this]() { this->reload(); });
            return true;
        });
        tp->addGestureRecognizer(new brls::TapGestureRecognizer(tp));
    }
    if (auto* tt = this->getView("tabThemes")) {
        tt->registerClickAction([this](brls::View*) {
            this->packsMode = false;
            if (auto* pb = this->getView("partButton")) pb->setVisibility(brls::Visibility::VISIBLE);
            brls::sync([this]() { this->reload(); });
            return true;
        });
        tt->addGestureRecognizer(new brls::TapGestureRecognizer(tt));
    }
    if (auto* sb = this->getView("searchButton")) {
        sb->registerClickAction([this](brls::View*) { this->openSearch(); return true; });
        sb->addGestureRecognizer(new brls::TapGestureRecognizer(sb));
    }
    if (auto* pb = this->getView("partButton")) {
        pb->registerClickAction([this](brls::View*) { this->cyclePart(); return true; });
        pb->addGestureRecognizer(new brls::TapGestureRecognizer(pb));
    }

    this->reload();
}

void ThemeBrowserActivity::reload() {
    this->page = 1;
    this->runQuery(1, /*append=*/false);
}

void ThemeBrowserActivity::runQuery(int page, bool append) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::VISIBLE);

    IHttpClient* client = this->http;
    auto alive = this->alive;
    bool packs = this->packsMode;
    std::string q = this->query;
    std::string t = this->target;

    brls::async([this, client, alive, packs, q, t, page, append]() {
        core::GraphQlFetcher fetch = makeFetcher(client);
        core::BrowseResult res = packs
            ? core::browse_packs(q, page, 30, fetch)
            : core::browse_themes(q, t, page, 30, fetch);
        bool ok = (res.status == core::BrowseStatus::Ok);
        core::BrowsePage pg = res.page;
        brls::sync([this, alive, ok, pg, append]() {
            if (!alive->load()) return;
            if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
            if (!ok) { brls::Application::notify("themes/error_network"_i18n); return; }
            this->populate(pg, append);
        });
    });
}

void ThemeBrowserActivity::loadThumb(const std::string& url, brls::Image* into) {
    if (url.empty() || !into) return;
    IHttpClient* client = this->http;
    auto alive = this->alive;
    std::string u = url;
    brls::async([client, alive, u, into]() {
        HttpResponse r = client->get(u);
        if (!r.ok()) return;
        std::string body = r.body;
        brls::sync([alive, body, into]() {
            if (!alive->load()) return;
            into->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
        });
    });
}

void ThemeBrowserActivity::populate(const core::BrowsePage& pg, bool append) {
    auto* box = (brls::Box*)this->getView("resultsBox");
    auto* empty = (brls::Label*)this->getView("emptyLabel");
    if (!box) return;

    this->page       = pg.page;
    this->isComplete = pg.is_complete;

    if (!append) box->clearViews();

    if (!append && pg.entries.empty()) {
        if (empty) { empty->setText("themes/no_results"_i18n); empty->setVisibility(brls::Visibility::VISIBLE); }
        return;
    }
    if (empty) empty->setVisibility(brls::Visibility::GONE);

    // 3-per-row grid. Build rows of up to 3 cards.
    brls::Box* rowBox = nullptr;
    int col = 0;
    for (const auto& entry : pg.entries) {
        if (col == 0) {
            rowBox = new brls::Box(brls::Axis::ROW);
            rowBox->setMarginBottom(12.0f);
            box->addView(rowBox);
        }
        core::ThemeEntry e = entry;

        auto* card = new brls::Box(brls::Axis::COLUMN);
        card->setWidth(230.0f);
        card->setMarginRight(12.0f);
        card->setPadding(8.0f, 8.0f, 10.0f, 8.0f);
        card->setCornerRadius(12.0f);
        card->setFocusable(true);
        card->setHideHighlightBackground(true);
        card->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));

        auto* img = new brls::Image();
        img->setWidth(214.0f);
        img->setHeight(120.0f);
        img->setCornerRadius(8.0f);
        card->addView(img);
        this->loadThumb(e.preview_url, img);

        auto* name = new brls::Label();
        name->setText(e.name);
        name->setFontSize(15.0f);
        name->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        name->setMarginTop(6.0f);
        card->addView(name);

        auto* meta = new brls::Label();
        std::string m = "@" + e.author + "  ⬇ " + std::to_string(e.downloads);
        if (theme_already_downloaded(e)) m += "  ✓";
        meta->setText(m);
        meta->setFontSize(12.0f);
        meta->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
        card->addView(meta);

        card->registerClickAction([this, e](brls::View*) {
            brls::Application::pushActivity(new ThemeDetailActivity(e, this->http));
            return true;
        });
        card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
        rowBox->addView(card);

        col = (col + 1) % 3;
    }

    // Load-more row (full width) when more pages exist.
    if (!this->isComplete) {
        auto* more = new brls::Box(brls::Axis::ROW);
        more->setHeight(48.0f);
        more->setFocusable(true);
        more->setMarginTop(4.0f);
        more->setCornerRadius(10.0f);
        more->setJustifyContent(brls::JustifyContent::CENTER);
        more->setAlignItems(brls::AlignItems::CENTER);
        more->setHideHighlightBackground(true);
        more->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
        auto* lbl = new brls::Label();
        lbl->setText("themes/load_more"_i18n);
        lbl->setFontSize(16.0f);
        more->addView(lbl);
        more->registerClickAction([this](brls::View*) {
            int next = this->page + 1;
            brls::sync([this, next]() { this->runQuery(next, /*append=*/true); });
            return true;
        });
        more->addGestureRecognizer(new brls::TapGestureRecognizer(more));
        box->addView(more);
    }
}

void ThemeBrowserActivity::openSearch() {
    brls::Application::getPlatform()->getImeManager()->openForText(
        [this, alive = this->alive](std::string q) {
            if (!alive->load()) return;
            this->query = q;
            brls::sync([this]() { this->reload(); });
        },
        "themes/search"_i18n, "themes/search_hint"_i18n, 64);
}

void ThemeBrowserActivity::cyclePart() {
    const int n = (int)(sizeof(kTargets) / sizeof(kTargets[0]));
    int cur = 0;
    for (int i = 0; i < n; i++) if (this->target == kTargets[i]) { cur = i; break; }
    this->target = kTargets[(cur + 1) % n];
    if (auto* lbl = (brls::Label*)this->getView("partButtonLabel"))
        lbl->setText("themes/filter_part"_i18n + std::string(": ") + partLabel(this->target));
    this->reload();
}

} // namespace thomaz
```

- [ ] **Step 4: Reconfigure + build desktop**

Run: `cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build_desktop -j"$(nproc)" 2>&1 | tail -3`
Expected: `Built target thomaz`

- [ ] **Step 5: Commit**

```bash
git add resources/xml/activity/theme_browser.xml source/app/theme_browser_activity.hpp source/app/theme_browser_activity.cpp
git commit -m "feat(themes): Themezer browser grid (packs/themes toggle, search, section filter, load-more)"
```

---

## Task 11: Home rail "Temas" card + wiring

**Files:**
- Modify: `resources/xml/activity/home.xml` (the rail Box, after the Mods card)
- Modify: `source/app/home_activity.cpp` (wire the click)

- [ ] **Step 1: Add the Temas card to the rail in home.xml**

Insert this block immediately after the `modsCard` `</AnimatedBox>` and before the rail-closing `</brls:Box>`:

```xml
                <!-- Themes (active) -->
                <AnimatedBox id="themesCard" entranceDelay="390"
                          axis="row" alignItems="center" grow="1.0" marginTop="14"
                          paddingLeft="22" paddingRight="22" cornerRadius="16"
                          backgroundColor="@theme/thomaz/tile_mods"
                          focusable="true" highlightCornerRadius="16"
                          hideHighlightBackground="true">
                    <brls:Label text="@i18n/themes/home"
                                fontSize="17" textColor="#FFFFFF" grow="1.0"/>
                </AnimatedBox>
```

- [ ] **Step 2: Add the include + wiring in home_activity.cpp**

Add the include near the other activity includes at the top of `source/app/home_activity.cpp`:

```cpp
#include "app/theme_browser_activity.hpp"
```

Add this block in `onContentAvailable` next to the other card wirings (e.g. right after the mods card block):

```cpp
    if (brls::View* themes = this->getView("themesCard")) {
        themes->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new ThemeBrowserActivity(this->http),
                                            brls::TransitionAnimation::NONE);
            return true;
        });
        themes->addGestureRecognizer(new brls::TapGestureRecognizer(themes));
    }
```

> The repo uses `brls::TransitionAnimation::NONE` for all rail pushes (entrance animations were intentionally removed). This block matches that.

- [ ] **Step 3: Reconfigure + build desktop + boot smoke**

Run:
```bash
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build_desktop -j"$(nproc)" 2>&1 | tail -3
timeout 4 ./build_desktop/thomaz >/tmp/themes_smoke.log 2>&1; echo "exit=$? (124=healthy)"
```
Expected: `Built target thomaz`, then `exit=124 (124=healthy)`.

- [ ] **Step 4: Commit**

```bash
git add resources/xml/activity/home.xml source/app/home_activity.cpp
git commit -m "feat(themes): add Temas card to the home rail"
```

---

## Task 12: Full test run, Switch CI validation, push

**Files:** none (verification)

- [ ] **Step 1: Run the full unit suite**

Run: `cd tests && make clean >/dev/null && make test 2>&1 | tail -3`
Expected: `Status: SUCCESS!` (all prior tests + the new themezer query/json/browse + theme_paths tests).

- [ ] **Step 2: Push the branch and let Switch CI build the .nro**

```bash
git push -u origin feat/themes-browser
```
Then watch the build workflow:
```bash
RUN=$(gh run list --branch feat/themes-browser --event push --limit 1 --json databaseId -q '.[0].databaseId')
gh run watch "$RUN" --exit-status >/dev/null 2>&1; echo "Switch CI exit=$?"
```
Expected: `Switch CI exit=0`. If it fails, read the log (`gh run view "$RUN" --log-failed`), fix the libnx-specific issue, commit, and re-run this step.

- [ ] **Step 3: Open the PR**

```bash
gh pr create --title "feat(themes): Themezer theme browser (Phase A — browse + download)" \
  --body "Phase A of the themes milestone: a 'Temas' module that browses Themezer (packs/themes toggle, search, section filter) as a thumbnail grid and downloads .nxtheme files to sd:/themes/. Native install is Phase B. Spec: docs/superpowers/specs/2026-06-04-themezer-theme-browser-design.md"
```

---

## Self-review notes (for the implementer)

- **Commit identity:** this repo commits as `luizfbalves <luizzbanndera@gmail.com>`. Prefix each commit command with `git -c user.name="luizfbalves" -c user.email="luizzbanndera@gmail.com"` (the ambient git identity is different).
- **CMake glob:** `source/*.cpp` is globbed with `GLOB_RECURSE`, so new files need a `cmake -B build_desktop ...` reconfigure (not just `--build`) to be picked up — already included in the build steps.
- **i18n auto-load:** Borealis loads every `resources/i18n/<locale>/<file>.json` as namespace `<file>`, so `themes.json` keys are reachable as `themes/...` with no registration.
- **Switch-only validation:** `setImageFromMem`, IME, and libnx HTTP only fully validate in the Switch CI; desktop build + boot-smoke covers layout/logic.
- **Deviation from spec — preview cache:** the spec mentioned an in-memory per-session preview cache. This plan re-fetches thumbnails when the grid is rebuilt (on tab/search/filter change); load-more only fetches the new cards. This keeps Phase A lean and is correctness-neutral. If thumbnail traffic proves heavy on device, add a `std::unordered_map<std::string, std::vector<std::uint8_t>>` cache keyed by URL in `ThemeBrowserActivity` and consult it in `loadThumb` before fetching — a small, isolated follow-up.
