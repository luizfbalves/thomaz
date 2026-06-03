#include "doctest.h"
#include "core/backup_store.hpp"

using namespace thomaz::core;

TEST_CASE("manifest round-trips through build + parse") {
    ManifestInfo in;
    in.game_name = "Zelda";
    in.title_id  = 0x0100000000010000ULL;
    in.timestamp = "2026-06-03_14-20-00";
    in.profiles  = {"e0e0...aa", "11112222"};

    std::string json = build_manifest(in);
    auto out = parse_manifest(json);

    REQUIRE(out.has_value());
    CHECK(out->game_name == "Zelda");
    CHECK(out->title_id  == 0x0100000000010000ULL);
    CHECK(out->timestamp == "2026-06-03_14-20-00");
    CHECK(out->profiles.size() == 2);
    CHECK(out->profiles[0] == "e0e0...aa");
}

TEST_CASE("parse_manifest returns nullopt on garbage") {
    CHECK_FALSE(parse_manifest("not json").has_value());
    CHECK_FALSE(parse_manifest("{}").has_value());
}
