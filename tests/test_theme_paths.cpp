#include "doctest.h"
#include "platform/themes/theme_paths.hpp"
#include <filesystem>

using namespace thomaz;
using thomaz::core::ThemeEntry;

TEST_CASE("theme_folder composes root + sanitized 'author - name'") {
    ThemeEntry e;
    e.author = "Hsushi";
    e.name   = "Purple/Skies: Home?";   // unsafe path chars
    std::string f = theme_folder(e);
    CHECK(f == themes_root() + "/Hsushi - Purple_Skies_ Home_");
}

TEST_CASE("theme_already_downloaded reflects folder existence") {
    namespace fs = std::filesystem;
    ThemeEntry e; e.author = "T"; e.name = "Exists";
    fs::remove_all(theme_folder(e));
    CHECK_FALSE(theme_already_downloaded(e));
    fs::create_directories(theme_folder(e));
    CHECK(theme_already_downloaded(e));
    fs::remove_all(theme_folder(e));
}
