#include "doctest.h"
#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_paths.hpp"
#include <filesystem>

using namespace thomaz;
using thomaz::core::ThemeEntry;

TEST_CASE("active theme round-trips and clears") {
    namespace fs = std::filesystem;
    fs::create_directories(themes_root());
    clear_active_theme();
    CHECK_FALSE(get_active_theme().has_value());

    ActiveTheme t;
    t.hex_id = "A24";
    t.name = "Purple Skies";
    t.author = "Hsushi";
    t.targets = {"ResidentMenu", "Entrance"};
    set_active_theme(t);

    auto got = get_active_theme();
    REQUIRE(got.has_value());
    CHECK(got->hex_id == "A24");
    CHECK(got->name == "Purple Skies");
    CHECK(got->targets.size() == 2);
    CHECK(got->targets[1] == "Entrance");

    clear_active_theme();
    CHECK_FALSE(get_active_theme().has_value());
}

TEST_CASE("is_active_theme matches by hex_id") {
    clear_active_theme();
    ThemeEntry e; e.hex_id = "FF0"; e.name = "X";
    CHECK_FALSE(is_active_theme(e));

    ActiveTheme t; t.hex_id = "FF0"; t.name = "X";
    set_active_theme(t);
    CHECK(is_active_theme(e));

    ThemeEntry other; other.hex_id = "111";
    CHECK_FALSE(is_active_theme(other));

    clear_active_theme();
}
