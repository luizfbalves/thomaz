#include "doctest.h"
#include "core/sysmod/sysmod_paths.hpp"

using namespace thomaz::core;

TEST_CASE("contents root is the Atmosphere contents directory") {
    CHECK(sysmod_contents_root() == "/atmosphere/contents");
}

TEST_CASE("contents dir appends the program id verbatim") {
    CHECK(sysmod_contents_dir("00FF0000636C6BFF")
          == "/atmosphere/contents/00FF0000636C6BFF");
}

TEST_CASE("flags dir and boot2 flag path are nested under the program dir") {
    CHECK(sysmod_flags_dir("00FF0000636C6BFF")
          == "/atmosphere/contents/00FF0000636C6BFF/flags");
    CHECK(sysmod_boot2_flag_path("00FF0000636C6BFF")
          == "/atmosphere/contents/00FF0000636C6BFF/flags/boot2.flag");
}
