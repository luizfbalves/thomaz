#pragma once

#include "core/games/catalog.hpp"

#include <string>
#include <vector>

namespace thomaz::core {

enum class CatalogSort { NameAsc, NameDesc, SizeAsc, SizeDesc };

enum class CatalogFilter { All, HasUpdate, HasDlc, BaseOnly };

struct CatalogViewQuery {
    CatalogSort   sort   = CatalogSort::NameAsc;
    CatalogFilter filter = CatalogFilter::All;
    std::string   search;
};

std::vector<GroupedTitle> apply_view(const std::vector<GroupedTitle>& in,
                                     const CatalogViewQuery&           q);

} // namespace thomaz::core
