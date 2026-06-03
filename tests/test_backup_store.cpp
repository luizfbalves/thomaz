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
    CHECK(out->profiles[1] == "11112222");
}

TEST_CASE("parse_manifest: title_id as a string is rejected; missing game_name is allowed") {
    // title_id stored as a string -> rejected (would otherwise silently become 0)
    CHECK_FALSE(parse_manifest(R"({"title_id":"0100","timestamp":"2026-01-01_00-00-00"})").has_value());
    // game_name optional: a manifest without it still parses, name defaults to ""
    auto ok = parse_manifest(R"({"title_id":4096,"timestamp":"2026-01-01_00-00-00"})");
    REQUIRE(ok.has_value());
    CHECK(ok->game_name == "");
    CHECK(ok->title_id == 4096);
}

TEST_CASE("parse_manifest returns nullopt on garbage") {
    CHECK_FALSE(parse_manifest("not json").has_value());
    CHECK_FALSE(parse_manifest("{}").has_value());
}

TEST_CASE("path builders compose root + lowercase title id + timestamp") {
    std::uint64_t tid = 0x0100000000010000ULL;
    CHECK(title_backups_dir("/sd/saves", tid) == "/sd/saves/0100000000010000");
    CHECK(backup_dir("/sd/saves", tid, "2026-06-03_14-20-00")
          == "/sd/saves/0100000000010000/2026-06-03_14-20-00");
}

TEST_CASE("format_timestamp_label renders dd/mm hh:mm") {
    CHECK(format_timestamp_label("2026-06-03_14-20-00") == "03/06 14:20");
    CHECK(format_timestamp_label("garbage") == "garbage");
}
