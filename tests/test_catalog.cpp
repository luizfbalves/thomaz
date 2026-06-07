#include "doctest.h"
#include "core/games/catalog.hpp"
#include "core/games/index_parse.hpp"
#include <sstream>
#include <string>

using namespace thomaz::core;

static ParsedIndex make_index(std::initializer_list<std::pair<const char*, std::uint64_t>> files) {
    ParsedIndex idx;
    idx.ok = true;
    for (const auto& [url, size] : files) {
        IndexFile f;
        f.url  = url;
        f.size = size;
        idx.files.push_back(std::move(f));
    }
    return idx;
}

TEST_CASE("group_catalog: base, update, and DLC collapse into one group") {
    const std::uint64_t base = 0x0100000000010000ULL;
    std::ostringstream bu, uu, du;
    bu << "https://x/[0100000000010000][v0].nsp";
    uu << "https://x/[0100000000010800][v0].nsp";
    du << "https://x/[0100000000011000][v0].nsp";
    ParsedIndex idx = make_index({{bu.str().c_str(), 100}, {uu.str().c_str(), 200}, {du.str().c_str(), 50}});
    auto groups = group_catalog(idx);
    REQUIRE(groups.size() == 1);
    CHECK(groups[0].baseId == base);
    CHECK(groups[0].hasUpdate);
    CHECK(groups[0].hasDlc);
    CHECK(groups[0].rows.size() == 3);
    CHECK(groups[0].totalSize == 350);
}

TEST_CASE("group_catalog: unrelated bases produce separate groups") {
    ParsedIndex idx = make_index({{"https://x/[0100000000010000][v0].nsp", 1},
                                  {"https://x/[01007EF00011E000][v0].nsp", 2}});
    auto groups = group_catalog(idx);
    CHECK(groups.size() == 2);
}
