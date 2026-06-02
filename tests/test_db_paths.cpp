#include "doctest.h"
#include "core/db_paths.hpp"

using namespace thomaz::core;

TEST_CASE("title_id_hex pads to 16 chars, both cases") {
    // Super Mario Odyssey
    CHECK(title_id_hex(0x0100000000010000ULL, false) == "0100000000010000");
    CHECK(title_id_hex(0x0100000000010000ULL, true)  == "0100000000010000");
    // A value with hex letters exercises upper/lower
    CHECK(title_id_hex(0x01006A800016E000ULL, false) == "01006a800016e000");
    CHECK(title_id_hex(0x01006A800016E000ULL, true)  == "01006A800016E000");
    // Small value still pads to 16
    CHECK(title_id_hex(0x1ULL, false) == "0000000000000001");
}

TEST_CASE("db URLs use the uppercase title id") {
    CHECK(cheats_url(0x01006A800016E000ULL) ==
        "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/cheats/01006A800016E000.json");
    CHECK(versions_url(0x0100000000010000ULL) ==
        "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/versions/0100000000010000.json");
}

TEST_CASE("sd_cheat_path uses lowercase title id and verbatim build id") {
    CHECK(sd_cheat_path(0x0100000000010000ULL, "B424BE150A8E7D78") ==
        "/atmosphere/contents/0100000000010000/cheats/B424BE150A8E7D78.txt");
}
