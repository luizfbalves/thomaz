#include "doctest.h"
#include "core/games/catalog.hpp"
#include "core/games/catalog_view.hpp"

using namespace thomaz::core;

static GroupedTitle make_group(const std::string& name, std::uint64_t size, bool upd, bool dlc,
                               std::uint64_t baseId = 0) {
    GroupedTitle g;
    g.displayName = name;
    g.totalSize   = size;
    g.hasUpdate   = upd;
    g.hasDlc      = dlc;
    g.baseId      = baseId;
    return g;
}

TEST_CASE("apply_view: NameAsc and SizeDesc ordering") {
    std::vector<GroupedTitle> in = {make_group("zelda", 300, false, false),
                                    make_group("animal", 100, false, false)};
    CatalogViewQuery q;
    q.sort = CatalogSort::NameAsc;
    auto asc = apply_view(in, q);
    REQUIRE(asc.size() == 2);
    CHECK(asc[0].displayName == "animal");
    q.sort = CatalogSort::SizeDesc;
    auto desc = apply_view(in, q);
    CHECK(desc[0].displayName == "zelda");
}

TEST_CASE("apply_view: BaseOnly excludes groups with update") {
    std::vector<GroupedTitle> in = {make_group("pure", 1, false, false),
                                    make_group("patched", 1, true, false)};
    CatalogViewQuery q;
    q.filter = CatalogFilter::BaseOnly;
    auto out = apply_view(in, q);
    REQUIRE(out.size() == 1);
    CHECK(out[0].displayName == "pure");
}

TEST_CASE("apply_view: search by 16-hex title ID substring") {
    GroupedTitle g = make_group("misc", 1, false, false, 0x0100000000010000ULL);
    std::vector<GroupedTitle> in = {g};
    CatalogViewQuery q;
    q.search = "0100000000010000";
    auto out = apply_view(in, q);
    CHECK(out.size() == 1);
}

TEST_CASE("apply_view: case-insensitive name search") {
    std::vector<GroupedTitle> in = {make_group("The Legend of Zelda", 1, false, false)};
    CatalogViewQuery q;
    q.search = "zelda";
    CHECK(apply_view(in, q).size() == 1);
}
