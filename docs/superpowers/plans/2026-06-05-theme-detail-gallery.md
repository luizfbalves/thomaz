# Theme Detail Gallery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the theme detail screen's single image with a gallery — a large hero image plus a horizontal, D-pad-navigable thumbnail strip (preview, background, icons; pack members for packs) and a fullscreen viewer — gated behind a centered loading state.

**Architecture:** The Themezer GraphQL API already exposes the extra images; we expand the detail queries, parse them into a new `ThemeDetail::gallery` vector in the core (unit-tested), then render that vector in the Borealis UI as a hero + `HScrollingFrame` thumbnail strip. A new `ImageViewerActivity` shows any gallery item fullscreen. The detail screen hides all content until the GraphQL request resolves, showing only a centered spinner.

**Tech Stack:** C++17, Borealis (brls) UI toolkit, nlohmann/json, doctest (core unit tests). Build: core tests via `tests/Makefile`; desktop app via `scripts/build-desktop.sh` (SDL2, WSLg).

---

## File Structure

**Core (unit-tested):**
- `source/core/themes/themezer_types.hpp` — add `GalleryImage` struct + `ThemeDetail::gallery`.
- `source/core/themes/themezer_query.cpp` — expand `theme_detail_body` / `pack_detail_body`.
- `source/core/themes/themezer_json.cpp` — build `gallery` from the responses.
- `tests/test_themezer_query.cpp`, `tests/test_themezer_json.cpp` — new assertions.

**UI (manual verification):**
- `resources/i18n/{en-US,pt-BR}/themes.json` — `view_fullscreen` hint string.
- `source/app/image_viewer_activity.{hpp,cpp}` — new fullscreen viewer (created).
- `resources/xml/activity/image_viewer.xml` — viewer layout (created).
- `resources/xml/activity/theme_detail.xml` — loading wrapper + hero + thumbnail strip.
- `source/app/theme_detail_activity.{hpp,cpp}` — loading state, gallery build, hero swap, viewer wiring, empty-gallery fallback.

**Build registration:** `CMakeLists.txt` uses `file(GLOB_RECURSE MAIN_SRC source/*.cpp)` — new `.cpp` files are picked up automatically on the next configure (the build scripts re-run `cmake -B`). No CMake edits needed.

---

## Task 1: Data model — `GalleryImage` + `ThemeDetail::gallery`

**Files:**
- Modify: `source/core/themes/themezer_types.hpp`

- [ ] **Step 1: Add the struct and field**

In `source/core/themes/themezer_types.hpp`, add the `GalleryImage` struct immediately **before** the `ThemeDetail` struct:

```cpp
// One image shown in the theme detail gallery. `url` is the full-resolution
// image (hero + fullscreen); `thumb_url` is the small image for the strip
// (same as `url` for single-size assets like icons/backgrounds).
struct GalleryImage {
    std::string url;
    std::string thumb_url;
    std::string label;   // "Preview", "Background", an icon name, or a pack member name
};
```

Then add this line inside the `ThemeDetail` struct, after `std::vector<ThemePart> parts;`:

```cpp
    std::vector<GalleryImage> gallery;
```

- [ ] **Step 2: Verify the core still compiles and tests pass**

Run: `cd tests && make test`
Expected: builds with no errors; all existing test cases pass (`[doctest] Status: SUCCESS!`).

- [ ] **Step 3: Commit**

```bash
git add source/core/themes/themezer_types.hpp
git commit -m "feat(themes): add GalleryImage type and ThemeDetail.gallery"
```

---

## Task 2: Expand the detail GraphQL queries

**Files:**
- Modify: `source/core/themes/themezer_query.cpp`
- Test: `tests/test_themezer_query.cpp`

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_themezer_query.cpp`:

```cpp
TEST_CASE("theme_detail_body requests preview sizes + asset images") {
    json t = json::parse(theme_detail_body("A24"));
    std::string q = t["query"].get<std::string>();
    CHECK(q.find("screenshotPreview{") != std::string::npos);
    CHECK(q.find("hdUrl") != std::string::npos);
    CHECK(q.find("thumbUrl") != std::string::npos);
    CHECK(q.find("assets{") != std::string::npos);
    CHECK(q.find("backgroundImageUrl") != std::string::npos);
    CHECK(q.find("homeIconUrl") != std::string::npos);
    CHECK(q.find("shareIconUrl") != std::string::npos);
}

TEST_CASE("pack_detail_body requests member theme previews") {
    json p = json::parse(pack_detail_body("16D"));
    std::string q = p["query"].get<std::string>();
    CHECK(q.find("themes{") != std::string::npos);
    CHECK(q.find("screenshotPreview{") != std::string::npos);
    CHECK(q.find("hdUrl") != std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd tests && make test`
Expected: FAIL — the two new cases report missing substrings (e.g. `assets{`, `hdUrl`).

- [ ] **Step 3: Update the query builders**

In `source/core/themes/themezer_query.cpp`, replace `theme_detail_body` with:

```cpp
std::string theme_detail_body(const std::string& hex_id) {
    std::string q =
        "{ switch{ theme(hexId:\"" + sanitize_hex(hex_id) + "\"){ hexId name "
        "description downloadUrl target creator{username} "
        "screenshotPreview{jpgThumbUrl hdUrl thumbUrl} "
        "assets{ backgroundImageUrl albumIconUrl homeIconUrl newsIconUrl "
        "shopIconUrl controllerIconUrl settingsIconUrl powerIconUrl "
        "nsoIconUrl cardIconUrl shareIconUrl } } } }";
    return wrap(q, json::object());
}
```

And replace `pack_detail_body` with (adds `screenshotPreview` to the `themes{}` selection; keeps everything else):

```cpp
std::string pack_detail_body(const std::string& hex_id) {
    std::string q =
        "{ switch{ pack(hexId:\"" + sanitize_hex(hex_id) + "\"){ hexId name "
        "description downloadUrl creator{username} collagePreview{jpgThumbUrl} "
        "themes{ hexId name target downloadUrl "
        "screenshotPreview{jpgThumbUrl hdUrl thumbUrl} } } } }";
    return wrap(q, json::object());
}
```

Note: `jpgThumbUrl` stays in `screenshotPreview` so the existing `preview_of()` parser (which reads `jpgThumbUrl`) keeps populating `ThemeEntry::preview_url` unchanged.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd tests && make test`
Expected: PASS — all query test cases green, no regressions.

- [ ] **Step 5: Commit**

```bash
git add source/core/themes/themezer_query.cpp tests/test_themezer_query.cpp
git commit -m "feat(themes): request gallery images in detail queries"
```

---

## Task 3: Parser — build the gallery for a single theme

**Files:**
- Modify: `source/core/themes/themezer_json.cpp`
- Test: `tests/test_themezer_json.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_themezer_json.cpp`:

```cpp
TEST_CASE("parse_theme_detail builds gallery: preview + background + non-null icons") {
    const char* TH = R"json({"data":{"switch":{"theme":{
      "hexId":"A24","name":"Purple","description":"d","downloadUrl":"u",
      "target":"ResidentMenu","creator":{"username":"Hsushi"},
      "screenshotPreview":{"jpgThumbUrl":"j.jpg","hdUrl":"hd.png","thumbUrl":"th.png"},
      "assets":{"backgroundImageUrl":"bg.png","homeIconUrl":"home.png",
                "albumIconUrl":null,"newsIconUrl":""}}}}})json";
    bool found = false;
    ThemeDetail d = parse_theme_detail(TH, &found);
    REQUIRE(found);
    REQUIRE(d.gallery.size() == 3);          // preview, background, home (album/news skipped)
    CHECK(d.gallery[0].label == "Preview");
    CHECK(d.gallery[0].url == "hd.png");
    CHECK(d.gallery[0].thumb_url == "th.png");
    CHECK(d.gallery[1].label == "Background");
    CHECK(d.gallery[1].url == "bg.png");
    CHECK(d.gallery[1].thumb_url == "bg.png");
    CHECK(d.gallery[2].label == "Home");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — `d.gallery.size()` is 0 (gallery not built yet).

- [ ] **Step 3: Add parser helpers and call them**

In `source/core/themes/themezer_json.cpp`, inside the anonymous `namespace {`, add these helpers **after** the existing `preview_of` function:

```cpp
// nlohmann's value() throws if a present field is null; the Themezer API returns
// null for icons a theme doesn't override, so read strings defensively.
std::string str_field(const json& o, const char* key) {
    if (!o.is_object() || !o.contains(key)) return std::string();
    const json& v = o[key];
    return v.is_string() ? v.get<std::string>() : std::string();
}

struct ImgUrls { std::string hd; std::string thumb; };

// Reads an ImageSizes node; falls back across fields so we always get a usable
// url pair (hd for hero/fullscreen, thumb for the strip).
ImgUrls image_sizes_of(const json& node) {
    ImgUrls u;
    u.hd    = str_field(node, "hdUrl");
    u.thumb = str_field(node, "thumbUrl");
    if (u.hd.empty())    u.hd    = str_field(node, "jpgThumbUrl");
    if (u.thumb.empty()) u.thumb = u.hd;
    if (u.hd.empty())    u.hd    = u.thumb;
    return u;
}

// (display label, assets-node json key) in strip order. Background first.
const std::pair<const char*, const char*> kAssetImages[] = {
    {"Background",  "backgroundImageUrl"},
    {"Album",       "albumIconUrl"},
    {"Home",        "homeIconUrl"},
    {"News",        "newsIconUrl"},
    {"Shop",        "shopIconUrl"},
    {"Controllers", "controllerIconUrl"},
    {"Settings",    "settingsIconUrl"},
    {"Power",       "powerIconUrl"},
    {"Online",      "nsoIconUrl"},
    {"Game Card",   "cardIconUrl"},
    {"Share",       "shareIconUrl"},
};

// Theme gallery = rendered preview, then each non-null asset image.
void build_theme_gallery(const json& node, ThemeDetail& d) {
    if (node.contains("screenshotPreview")) {
        ImgUrls p = image_sizes_of(node["screenshotPreview"]);
        if (!p.hd.empty()) d.gallery.push_back({ p.hd, p.thumb, "Preview" });
    }
    if (node.contains("assets") && node["assets"].is_object()) {
        const json& a = node["assets"];
        for (const auto& pair : kAssetImages) {
            std::string url = str_field(a, pair.second);
            if (!url.empty()) d.gallery.push_back({ url, url, pair.first });
        }
    }
}
```

Then in `parse_theme_detail`, add the call immediately **before** `if (found) *found = true;`:

```cpp
    build_theme_gallery(node, d);
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd tests && make test`
Expected: PASS — new theme-gallery case green, all prior cases still pass.

- [ ] **Step 5: Commit**

```bash
git add source/core/themes/themezer_json.cpp tests/test_themezer_json.cpp
git commit -m "feat(themes): parse theme gallery (preview + assets)"
```

---

## Task 4: Parser — build the gallery for a pack (member previews)

**Files:**
- Modify: `source/core/themes/themezer_json.cpp`
- Test: `tests/test_themezer_json.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/test_themezer_json.cpp`:

```cpp
TEST_CASE("parse_pack_detail builds gallery: one item per member preview") {
    const char* P = R"json({"data":{"switch":{"pack":{
      "hexId":"16D","name":"Clean","description":"c","downloadUrl":"u",
      "creator":{"username":"x"},"collagePreview":{"jpgThumbUrl":"c.jpg"},
      "themes":[
        {"hexId":"9A6","name":"Home","target":"ResidentMenu","downloadUrl":"u1",
         "screenshotPreview":{"hdUrl":"h1.png","thumbUrl":"t1.png"}},
        {"hexId":"9A7","name":"Lock","target":"Entrance","downloadUrl":"u2",
         "screenshotPreview":{"hdUrl":"h2.png","thumbUrl":"t2.png"}}
      ]}}}})json";
    bool found = false;
    ThemeDetail d = parse_pack_detail(P, &found);
    REQUIRE(found);
    REQUIRE(d.gallery.size() == 2);
    CHECK(d.gallery[0].label == "Home");
    CHECK(d.gallery[0].url == "h1.png");
    CHECK(d.gallery[1].label == "Lock");
    CHECK(d.gallery[1].thumb_url == "t2.png");
    // members are still expanded into parts (download path unchanged)
    CHECK(d.parts.size() == 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — `d.gallery.size()` is 0 for packs.

- [ ] **Step 3: Build the pack gallery in the parser**

In `source/core/themes/themezer_json.cpp`, inside `parse_pack_detail`, locate the member loop:

```cpp
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
```

Add the gallery push **inside the loop, after `d.parts.push_back(p);`**:

```cpp
            if (t.contains("screenshotPreview")) {
                ImgUrls img = image_sizes_of(t["screenshotPreview"]);
                if (!img.hd.empty()) {
                    std::string label = p.name.empty() ? p.target : p.name;
                    d.gallery.push_back({ img.hd, img.thumb, label });
                }
            }
```

(`image_sizes_of` and `ImgUrls` were added in Task 3 and are visible here in the same translation unit.)

- [ ] **Step 4: Run test to verify it passes**

Run: `cd tests && make test`
Expected: PASS — pack-gallery case green, all prior cases still pass.

- [ ] **Step 5: Commit**

```bash
git add source/core/themes/themezer_json.cpp tests/test_themezer_json.cpp
git commit -m "feat(themes): parse pack gallery (one item per member)"
```

---

## Task 5: Add the fullscreen-hint i18n string

**Files:**
- Modify: `resources/i18n/en-US/themes.json`
- Modify: `resources/i18n/pt-BR/themes.json`

- [ ] **Step 1: Add the key to en-US**

In `resources/i18n/en-US/themes.json`, add this entry after the `"by": "by",` line:

```json
    "view_fullscreen": "View fullscreen",
```

- [ ] **Step 2: Add the key to pt-BR**

In `resources/i18n/pt-BR/themes.json`, add this entry after the `"by": "por",` line:

```json
    "view_fullscreen": "Ver em tela cheia",
```

- [ ] **Step 3: Verify both files are valid JSON**

Run: `python3 -c "import json; json.load(open('resources/i18n/en-US/themes.json')); json.load(open('resources/i18n/pt-BR/themes.json')); print('ok')"`
Expected: prints `ok` (no JSON parse error from a stray/missing comma).

- [ ] **Step 4: Commit**

```bash
git add resources/i18n/en-US/themes.json resources/i18n/pt-BR/themes.json
git commit -m "i18n(themes): add view_fullscreen hint"
```

---

## Task 6: Fullscreen image viewer activity

**Files:**
- Create: `source/app/image_viewer_activity.hpp`
- Create: `source/app/image_viewer_activity.cpp`
- Create: `resources/xml/activity/image_viewer.xml`

- [ ] **Step 1: Create the XML layout**

Create `resources/xml/activity/image_viewer.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!--
  Fullscreen gallery image viewer. A dim full-bleed backdrop with the image
  centered (aspect preserved) and the item label as a caption. B pops the
  activity (Borealis default back). IDs (viewerImage/viewerCaption/viewerSpinner)
  are wired by ImageViewerActivity.
-->
<brls:Box axis="column" grow="1.0" justifyContent="center" alignItems="center"
          backgroundColor="#000000E6" paddingTop="40" paddingBottom="40"
          paddingLeft="60" paddingRight="60">
    <brls:ProgressSpinner id="viewerSpinner" width="48" height="48"/>
    <brls:Image id="viewerImage" width="1000" height="563" visibility="gone"/>
    <brls:Label id="viewerCaption" fontSize="16" textColor="#FFFFFF"
                marginTop="16" horizontalAlign="center"/>
</brls:Box>
```

- [ ] **Step 2: Create the header**

Create `source/app/image_viewer_activity.hpp`:

```cpp
#pragma once
#include <atomic>
#include <memory>

#include <borealis.hpp>

#include "core/themes/themezer_types.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Fullscreen viewer for a single gallery image. Fetches the full-resolution
// `url` async, showing a spinner until it arrives. B closes (Borealis default).
class ImageViewerActivity : public brls::Activity {
  public:
    ImageViewerActivity(thomaz::core::GalleryImage image, IHttpClient* http);
    ~ImageViewerActivity() override;

    CONTENT_FROM_XML_RES("activity/image_viewer.xml");
    void onContentAvailable() override;

  private:
    thomaz::core::GalleryImage image;
    IHttpClient*               http;
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Create the implementation**

Create `source/app/image_viewer_activity.cpp`:

```cpp
#include "app/image_viewer_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>

namespace thomaz {

ImageViewerActivity::ImageViewerActivity(core::GalleryImage image, IHttpClient* http)
    : image(std::move(image)), http(http) {}

ImageViewerActivity::~ImageViewerActivity() { *this->alive = false; }

void ImageViewerActivity::onContentAvailable() {
    if (auto* caption = (brls::Label*)this->getView("viewerCaption"))
        caption->setText(this->image.label);

    std::string url = this->image.url;
    if (url.empty()) {
        if (auto* spinner = this->getView("viewerSpinner"))
            spinner->setVisibility(brls::Visibility::GONE);
        return;
    }

    IHttpClient* client = this->http;
    auto alive = this->alive;
    brls::async([this, client, url, alive]() {
        HttpResponse r = client->get(url);
        if (!r.ok()) return;
        std::string body = r.body;
        brls::sync([this, alive, body]() {
            if (!alive->load()) return;
            if (auto* spinner = this->getView("viewerSpinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            if (auto* img = (brls::Image*)this->getView("viewerImage")) {
                img->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
                img->setVisibility(brls::Visibility::VISIBLE);
            }
        });
    });
}

} // namespace thomaz
```

- [ ] **Step 4: Verify the desktop build compiles the new files**

Run: `./scripts/build-desktop.sh`
Expected: configures (re-globs and finds `image_viewer_activity.cpp`) and builds to completion — final line `Done. Run it with:  ./build_desktop/thomaz`, no compile/link errors.

- [ ] **Step 5: Commit**

```bash
git add source/app/image_viewer_activity.hpp source/app/image_viewer_activity.cpp resources/xml/activity/image_viewer.xml
git commit -m "feat(themes): add fullscreen ImageViewerActivity"
```

---

## Task 7: Theme detail layout — loading wrapper + thumbnail strip

**Files:**
- Modify: `resources/xml/activity/theme_detail.xml`

- [ ] **Step 1: Replace the layout**

Replace the entire contents of `resources/xml/activity/theme_detail.xml` with:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!--
  Theme/pack detail. Frame-level: centered spinner + error label shown while the
  detail request is in flight; the two-column content (id="detailContent") stays
  hidden until it resolves.
  Left: hero preview + horizontal thumbnail strip + name/author + (pack) parts.
  Right: description (wraps) + download button + note.
  IDs (spinner/detailError/detailContent/detailPreview/thumbStrip/thumbStripRow/
  detailName/detailAuthor/detailDesc/partsLabel/partsBox/downloadButton/
  downloadButtonLabel/downloadNote) are wired by ThemeDetailActivity.
-->
<brls:AppletFrame id="themeDetailFrame" title="@i18n/themes/title" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0">

        <brls:ProgressSpinner id="spinner" width="48" height="48"
                              alignSelf="center" marginTop="180" visibility="gone"/>
        <brls:Label id="detailError" alignSelf="center" marginTop="180" fontSize="18"
                    textColor="@theme/thomaz/text_dim" visibility="gone"/>

        <brls:Box id="detailContent" axis="row" grow="1.0" visibility="gone"
                  paddingTop="28" paddingBottom="24" paddingLeft="40" paddingRight="40">

            <!-- ===== LEFT: hero + strip + identity + parts ===== -->
            <brls:Box axis="column" width="460" marginRight="28">
                <brls:Image id="detailPreview" width="460" height="259" cornerRadius="12" marginBottom="10"/>
                <brls:HScrollingFrame id="thumbStrip" width="460" height="52" marginBottom="14">
                    <brls:Box id="thumbStripRow" axis="row" height="48"/>
                </brls:HScrollingFrame>
                <brls:Label id="detailName" fontSize="24" lineHeight="1.2" textColor="#FFFFFF" marginBottom="4"/>
                <brls:Label id="detailAuthor" fontSize="15" textColor="@theme/thomaz/text_dim" marginBottom="14"/>
                <brls:Label id="partsLabel" fontSize="15" textColor="@theme/thomaz/text_dim"
                            visibility="gone" marginBottom="6"/>
                <brls:Box id="partsBox" axis="column"/>
            </brls:Box>

            <!-- ===== RIGHT: description + download ===== -->
            <brls:Box axis="column" grow="1.0">
                <brls:Label id="detailDesc" width="690" fontSize="15" lineHeight="1.45"
                            verticalAlign="top" textColor="@theme/thomaz/text_dim"/>
                <brls:Box grow="1.0"/>
                <brls:Box id="downloadButton" axis="row" height="56" cornerRadius="10"
                          justifyContent="center" alignItems="center" focusable="true"
                          hideHighlightBackground="true" backgroundColor="@theme/thomaz/accent_bright">
                    <brls:Label id="downloadButtonLabel" text="@i18n/themes/download" fontSize="18" textColor="#FFFFFF"/>
                </brls:Box>
                <brls:Label id="downloadNote" fontSize="13" textColor="@theme/thomaz/text_dim" marginTop="10"/>
            </brls:Box>
        </brls:Box>
    </brls:Box>
</brls:AppletFrame>
```

Key changes vs. the old file: the spinner moved to frame level (centered) and an error label was added beside it; the two-column row is wrapped in `detailContent` (starts `visibility="gone"`); the left column gained an `HScrollingFrame` (`thumbStrip` → `thumbStripRow`) under the hero.

- [ ] **Step 2: Verify the desktop build still configures/compiles**

Run: `./scripts/build-desktop.sh`
Expected: builds to completion (XML is validated at runtime, but the build must stay green). No errors.

- [ ] **Step 3: Commit**

```bash
git add resources/xml/activity/theme_detail.xml
git commit -m "feat(themes): detail layout with loading gate + thumbnail strip"
```

---

## Task 8: Detail activity — loading state, gallery, hero swap, viewer wiring

**Files:**
- Modify: `source/app/theme_detail_activity.hpp`
- Modify: `source/app/theme_detail_activity.cpp`

- [ ] **Step 1: Update the header**

In `source/app/theme_detail_activity.hpp`, add `#include <string>` and `#include <unordered_map>` near the top includes. Add these three method declarations in the `private:` section, after `void onResolved(...)`:

```cpp
    void loadThumb(const std::string& url, brls::Image* into);
    void buildGallery();
    void showGalleryImage(const thomaz::core::GalleryImage& img);
```

Add this member in the `private:` section, after the `alive` member:

```cpp
    std::unordered_map<std::string, std::string> heroCache; // url -> raw image bytes
```

- [ ] **Step 2: Add the include and the new methods in the .cpp**

In `source/app/theme_detail_activity.cpp`, add the new activity include after the existing `#include "app/app_header.hpp"`:

```cpp
#include "app/image_viewer_activity.hpp"
```

Add these three method definitions inside `namespace thomaz {`, immediately **before** `void ThemeDetailActivity::onResolved(...)`:

```cpp
// Fetches `url` into `into`, guarded by the activity's alive flag (same pattern
// as the browse grid).
void ThemeDetailActivity::loadThumb(const std::string& url, brls::Image* into) {
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

// Loads the HD image into the hero, caching bytes by url so revisits are instant.
void ThemeDetailActivity::showGalleryImage(const core::GalleryImage& img) {
    auto* hero = (brls::Image*)this->getView("detailPreview");
    if (!hero || img.url.empty()) return;

    auto it = this->heroCache.find(img.url);
    if (it != this->heroCache.end()) {
        hero->setImageFromMem((const unsigned char*)it->second.data(), (int)it->second.size());
        return;
    }
    IHttpClient* client = this->http;
    auto alive = this->alive;
    std::string url = img.url;
    brls::async([this, client, alive, url]() {
        HttpResponse r = client->get(url);
        if (!r.ok()) return;
        std::string body = r.body;
        brls::sync([this, alive, url, body]() {
            if (!alive->load()) return;
            this->heroCache[url] = body;
            if (auto* hero = (brls::Image*)this->getView("detailPreview"))
                hero->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
        });
    });
}

// Builds the thumbnail strip from detail.gallery. Empty gallery => hide the
// strip and fall back to the browse preview in the hero.
void ThemeDetailActivity::buildGallery() {
    auto* strip = this->getView("thumbStrip");
    auto* row   = (brls::Box*)this->getView("thumbStripRow");
    const auto& g = this->detail.gallery;

    if (g.empty()) {
        if (strip) strip->setVisibility(brls::Visibility::GONE);
        if (!this->entry.preview_url.empty())
            this->loadThumb(this->entry.preview_url, (brls::Image*)this->getView("detailPreview"));
        return;
    }

    if (strip) strip->setVisibility(brls::Visibility::VISIBLE);
    if (row) row->clearViews();

    for (const auto& item : g) {
        core::GalleryImage gi = item;
        auto* thumb = new brls::Image();
        thumb->setWidth(80.0f);
        thumb->setHeight(45.0f);
        thumb->setCornerRadius(6.0f);
        thumb->setMarginRight(8.0f);
        thumb->setFocusable(true);
        this->loadThumb(gi.thumb_url, thumb);

        thumb->getFocusEvent()->subscribe([this, gi](brls::View*) {
            this->showGalleryImage(gi);
        });
        thumb->registerAction("themes/view_fullscreen"_i18n, brls::BUTTON_A,
            [this, gi](brls::View*) {
                brls::Application::pushActivity(new ImageViewerActivity(gi, this->http));
                return true;
            });
        thumb->addGestureRecognizer(new brls::TapGestureRecognizer(thumb, [this, gi]() {
            brls::Application::pushActivity(new ImageViewerActivity(gi, this->http));
        }));

        if (row) row->addView(thumb);
    }

    this->showGalleryImage(g.front());
}
```

- [ ] **Step 3: Show the loading state in `onContentAvailable` (remove the eager preview fetch)**

In `source/app/theme_detail_activity.cpp`, in `onContentAvailable`, **delete** this whole block (the eager single-image fetch):

```cpp
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
```

The line immediately after it already shows the spinner:

```cpp
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::VISIBLE);
```

Leave that line. The name/author/note set above it populate views inside the hidden `detailContent`, so nothing is shown until resolve — that is the desired behavior.

- [ ] **Step 4: Reveal content (or error) in `onResolved`**

In `onResolved`, replace the existing top of the method:

```cpp
void ThemeDetailActivity::onResolved(const core::ThemeDetail& d, bool ok) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
    if (!ok) {
        brls::Application::notify("themes/error_network"_i18n);
        return;
    }
    this->detail   = d;
    this->resolved = true;
```

with:

```cpp
void ThemeDetailActivity::onResolved(const core::ThemeDetail& d, bool ok) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
    if (!ok) {
        if (auto* err = (brls::Label*)this->getView("detailError")) {
            err->setText("themes/error_network"_i18n);
            err->setVisibility(brls::Visibility::VISIBLE);
        }
        brls::Application::notify("themes/error_network"_i18n);
        return;
    }
    this->detail   = d;
    this->resolved = true;

    if (auto* content = this->getView("detailContent"))
        content->setVisibility(brls::Visibility::VISIBLE);
    this->buildGallery();
```

(The rest of `onResolved` — setting `detailDesc`, the pack parts loop, `downloaded`/`applied` state, `refreshActionButton()` — stays unchanged.)

- [ ] **Step 5: Build and verify it compiles**

Run: `./scripts/build-desktop.sh`
Expected: builds to completion, no compile/link errors.

- [ ] **Step 6: Smoke-run the app**

Run: `timeout 8 ./build_desktop/thomaz; echo "exit=$?"`
Expected: the app launches without crashing; the timeout kills it after 8s → `exit=124` (a clean run reaching the timeout is the healthy outcome). Any other non-zero exit before 8s indicates a startup crash to investigate.

- [ ] **Step 7: Commit**

```bash
git add source/app/theme_detail_activity.hpp source/app/theme_detail_activity.cpp
git commit -m "feat(themes): gallery UI — loading gate, hero swap, fullscreen"
```

- [ ] **Step 8: On-device / interactive verification (manual)**

Build for hardware with `./scripts/build-switch.sh` (or run the desktop binary interactively via `./scripts/run-desktop.sh`) and confirm:
- Opening a theme shows **only the centered spinner**, then reveals the full screen at once.
- The hero shows the preview; the thumbnail strip lists preview + background + icons.
- D-pad left/right along the strip **swaps the hero**; the strip scrolls to keep the focused thumb visible on a many-icon theme.
- Pressing **A** on a thumbnail opens it fullscreen with its caption; **B** returns.
- Opening a **pack** shows one thumbnail per member theme, each swappable.
- With the network disconnected, the detail screen shows the **centered error message** (no blank content).

---

## Self-Review

**Spec coverage:**
- Data model `GalleryImage` + `ThemeDetail::gallery` → Task 1. ✓
- API query expansion (theme assets, pack member previews) → Task 2. ✓
- Parser: theme `[Preview, Background, icons…]` skipping nulls → Task 3. ✓
- Parser: pack one-item-per-member with member-name labels → Task 4. ✓
- Layout A (hero + horizontal thumbnail strip) → Task 7 (`HScrollingFrame`). ✓
- Hero swap on focus → Task 8 (`getFocusEvent()->subscribe`). ✓
- Fullscreen viewer (A opens, B closes) → Task 6 + Task 8 wiring. ✓
- Loading state (centered spinner, reveal on resolve, centered error) → Task 7 (layout) + Task 8 (logic). ✓
- Empty-gallery fallback to `preview_url` → Task 8 `buildGallery`. ✓
- Failed image load leaves placeholder (alive-guarded async, no crash) → Tasks 6 & 8. ✓
- Unit tests for parser + query → Tasks 2–4. ✓

**Placeholder scan:** No TBD/TODO/"handle edge cases"; every code step shows full code. ✓

**Type consistency:** `GalleryImage{url, thumb_url, label}` aggregate-initialized consistently as `{url, thumb, label}` across parser (Tasks 3–4) and UI (Task 8). `ThemeDetail::gallery`, `image_sizes_of`, `str_field`, `build_theme_gallery`, `buildGallery`, `showGalleryImage`, `loadThumb`, and view IDs (`spinner`, `detailError`, `detailContent`, `thumbStrip`, `thumbStripRow`, `viewerImage`, `viewerCaption`, `viewerSpinner`) match between the XML, header, and implementation. ✓

## Out of Scope
- Disk caching of gallery images across sessions.
- Transitions/animations beyond Borealis defaults.
- Downloading individual assets (icons/background) separately.
