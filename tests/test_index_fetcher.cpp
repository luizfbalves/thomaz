#include "doctest.h"
#include "platform/games/index_fetch_util.hpp"

using namespace thomaz;

TEST_CASE("same_host matches case-insensitively") {
    CHECK(same_host("Example.COM", "example.com"));
    CHECK_FALSE(same_host("a.example.com", "b.example.com"));
    CHECK_FALSE(same_host("", "example.com"));
}
