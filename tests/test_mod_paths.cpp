#include "doctest.h"
#include "core/mods/mod_paths.hpp"

using namespace thomaz::core;

static constexpr std::uint64_t SMO = 0x0100000000010000ULL;

TEST_CASE("staging dir is per-title, lowercase hex, under the staging root") {
    CHECK(mod_staging_dir(SMO, "Cool Skin")
          == mod_staging_root() + "/0100000000010000/Cool Skin");
}

TEST_CASE("staging title dir has no mod name") {
    CHECK(mod_staging_title_dir(SMO)
          == mod_staging_root() + "/0100000000010000");
}

TEST_CASE("contents romfs dir is the Atmosphere LayeredFS target") {
    CHECK(sd_romfs_dir(SMO) == "/atmosphere/contents/0100000000010000/romfs");
}

TEST_CASE("active marker lives in the title staging dir") {
    CHECK(active_marker_path(SMO)
          == mod_staging_root() + "/0100000000010000/.active");
}
