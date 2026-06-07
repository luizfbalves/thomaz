#include "doctest.h"
#include "core/games/title_id.hpp"

using namespace thomaz::core;

TEST_CASE("title_kind: base, update, DLC, and unknown") {
    const std::uint64_t base = 0x0100000000010000ULL;
    CHECK(title_kind(base) == TitleKind::Base);
    CHECK(title_kind(base | 0x800ULL) == TitleKind::Update);
    CHECK(title_kind(base | 0x1000ULL) == TitleKind::Dlc);
    CHECK(title_kind(base | 0x400ULL) == TitleKind::Unknown);
}

TEST_CASE("base_title_id: update and DLC mask to the same base") {
    const std::uint64_t base = 0x0100000000010000ULL;
    CHECK(base_title_id(base) == base);
    CHECK(base_title_id(base | 0x800ULL) == base);
    CHECK(base_title_id(base | 0x1000ULL) == base);
}

TEST_CASE("dlc_content_index extracts slot from DLC id") {
    const std::uint64_t base = 0x0100000000010000ULL;
    CHECK(dlc_content_index(base | 0x1000ULL) == 0);
    CHECK(dlc_content_index(base | 0x1001ULL) == 1);
}
