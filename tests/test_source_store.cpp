#include "doctest.h"
#include "platform/games/source_store.hpp"

#include <cstdio>

using namespace thomaz;

TEST_CASE("load_sources returns empty when config file is missing") {
    const std::string path = sources_config_path();
    std::remove(path.c_str());
    CHECK(load_sources().empty());
}
