#include "doctest.h"
#include "core/mods/gamebanana_overrides.hpp"

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
