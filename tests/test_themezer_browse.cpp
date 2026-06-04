#include "doctest.h"
#include "core/themes/themezer_browse.hpp"

using namespace thomaz::core;
using namespace thomaz::core::themezer;

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
