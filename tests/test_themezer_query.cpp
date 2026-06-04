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
    json t = json::parse(theme_detail_body("A2\"4 wxyz"));
    CHECK(t["query"].get<std::string>().find("theme(hexId:\"A24\")") != std::string::npos);
    json p = json::parse(pack_detail_body("16D"));
    CHECK(p["query"].get<std::string>().find("pack(hexId:\"16D\")") != std::string::npos);
}
