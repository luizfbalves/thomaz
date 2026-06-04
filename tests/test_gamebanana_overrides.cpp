#include "doctest.h"
#include "core/mods/gamebanana_overrides.hpp"
#include "core/mods/mod_paths.hpp"

#include <filesystem>
#include <fstream>

using namespace thomaz::core;

TEST_CASE("override returns the curated game_id for a seeded title") {
    // The Legend of Zelda: Breath of the Wild (Switch) -> GameBanana game 6386
    auto id = gamebanana_game_override(0x01007EF00011E000ULL);
    REQUIRE(id.has_value());
    CHECK(*id == 6386);
}

TEST_CASE("override returns nullopt for an unmapped title") {
    CHECK_FALSE(gamebanana_game_override(0x0123456789ABCDEFULL).has_value());
}

TEST_CASE("override reads user-supplied pairs from the SD overrides file") {
    namespace fs = std::filesystem;
    fs::path root = mod_staging_root();          // "thomaz-mods" on desktop
    fs::create_directories(root);
    fs::path file = root / "overrides.json";
    {
        std::ofstream out(file);
        out << R"({ "overrides": [
            { "title_id": "0100AABBCCDD0000", "game_id": 4242 },
            { "title_id": "0100EEFF00110000", "game_id": "777" }
        ] })";
    }

    auto a = gamebanana_game_override(0x0100AABBCCDD0000ULL);
    REQUIRE(a.has_value());
    CHECK(*a == 4242);

    auto b = gamebanana_game_override(0x0100EEFF00110000ULL); // game_id as string
    REQUIRE(b.has_value());
    CHECK(*b == 777);

    CHECK_FALSE(gamebanana_game_override(0x0100000000990000ULL).has_value());

    // Compiled entries still take precedence and don't need the file.
    auto botw = gamebanana_game_override(0x01007EF00011E000ULL);
    REQUIRE(botw.has_value());
    CHECK(*botw == 6386);

    fs::remove(file); // keep other tests' environment clean
}
