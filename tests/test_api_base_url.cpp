#include "doctest.h"
#include <cstdio>
#include "platform/app_settings.hpp"

using namespace thomaz;

TEST_CASE("api base url falls back to a non-empty default when none saved") {
    std::remove("thomaz-cache/api_url.txt");
    std::string def = load_api_base_url();
    CHECK_FALSE(def.empty());
    CHECK(def.find("://") != std::string::npos); // looks like a URL
}

TEST_CASE("api base url save trims and strips trailing slash, then loads back") {
    save_api_base_url("  http://localhost:3000/  ");
    CHECK(load_api_base_url() == "http://localhost:3000");
    std::remove("thomaz-cache/api_url.txt"); // cleanup
}
