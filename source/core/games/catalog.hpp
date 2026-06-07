#pragma once

#include "core/games/index_parse.hpp"
#include "core/games/title_id.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

struct CatalogRow {
    std::uint64_t titleId     = 0;
    TitleKind     kind        = TitleKind::Unknown;
    std::uint64_t size        = 0;
    std::string   url;
    std::string   nameOverride;
};

struct GroupedTitle {
    std::uint64_t            baseId = 0;
    std::string              displayName;
    std::vector<CatalogRow>  rows;
    bool                     hasUpdate = false;
    bool                     hasDlc    = false;
    std::uint64_t            totalSize = 0;
};

std::vector<GroupedTitle> group_catalog(const ParsedIndex& idx);

} // namespace thomaz::core
