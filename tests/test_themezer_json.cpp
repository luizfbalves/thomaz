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
    CHECK_FALSE(p.is_complete);
}

TEST_CASE("parse_browse_page reads packs + collage preview, last page complete") {
    BrowsePage p = parse_browse_page(PACKS_JSON, ThemeKind::Pack);
    REQUIRE(p.entries.size() == 1);
    CHECK(p.entries[0].kind == ThemeKind::Pack);
    CHECK(p.entries[0].target.empty());
    CHECK(p.entries[0].preview_url == "https://img/c.jpg");
    CHECK(p.is_complete);
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

TEST_CASE("parse_pack_detail builds gallery: one item per member preview") {
    const char* P = R"json({"data":{"switch":{"pack":{
      "hexId":"16D","name":"Clean","description":"c","downloadUrl":"u",
      "creator":{"username":"x"},"collagePreview":{"jpgThumbUrl":"c.jpg"},
      "themes":[
        {"hexId":"9A6","name":"Home","target":"ResidentMenu","downloadUrl":"u1",
         "screenshotPreview":{"hdUrl":"h1.png","thumbUrl":"t1.png"}},
        {"hexId":"9A7","name":"Lock","target":"Entrance","downloadUrl":"u2",
         "screenshotPreview":{"hdUrl":"h2.png","thumbUrl":"t2.png"}},
        {"hexId":"9A8","name":"","target":"News","downloadUrl":"u3",
         "screenshotPreview":{"jpgThumbUrl":"j3.jpg"}}
      ]}}}})json";
    bool found = false;
    ThemeDetail d = parse_pack_detail(P, &found);
    REQUIRE(found);
    REQUIRE(d.gallery.size() == 3);
    CHECK(d.gallery[0].label == "Home");
    CHECK(d.gallery[0].url == "h1.png");
    CHECK(d.gallery[1].label == "Lock");
    CHECK(d.gallery[1].thumb_url == "t2.png");
    // empty name falls back to target; jpgThumbUrl-only fills both url and thumb
    CHECK(d.gallery[2].label == "News");
    CHECK(d.gallery[2].url == "j3.jpg");
    CHECK(d.gallery[2].thumb_url == "j3.jpg");
    // members are still expanded into parts (download path unchanged)
    CHECK(d.parts.size() == 3);
}
