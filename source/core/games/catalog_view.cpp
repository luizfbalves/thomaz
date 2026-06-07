#include "core/games/catalog_view.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace thomaz::core {

namespace {

std::string lower_copy(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool matches_search(const GroupedTitle& g, const std::string& needle) {
    if (needle.empty())
        return true;
    const std::string q = lower_copy(needle);
    if (lower_copy(g.displayName).find(q) != std::string::npos)
        return true;
    std::ostringstream hex;
    hex << std::hex << std::setfill('0') << std::setw(16) << g.baseId;
    const std::string idHex = hex.str();
    if (idHex.find(needle) != std::string::npos)
        return true;
    for (const auto& r : g.rows) {
        std::ostringstream rh;
        rh << std::hex << std::setfill('0') << std::setw(16) << r.titleId;
        if (rh.str().find(needle) != std::string::npos)
            return true;
    }
    return false;
}

bool passes_filter(const GroupedTitle& g, CatalogFilter f) {
    switch (f) {
    case CatalogFilter::All:
        return true;
    case CatalogFilter::HasUpdate:
        return g.hasUpdate;
    case CatalogFilter::HasDlc:
        return g.hasDlc;
    case CatalogFilter::BaseOnly:
        return !g.hasUpdate && !g.hasDlc;
    }
    return true;
}

} // namespace

std::vector<GroupedTitle> apply_view(const std::vector<GroupedTitle>& in,
                                     const CatalogViewQuery&           q) {
    std::vector<GroupedTitle> out;
    out.reserve(in.size());
    for (const auto& g : in) {
        if (!passes_filter(g, q.filter))
            continue;
        if (!matches_search(g, q.search))
            continue;
        out.push_back(g);
    }

    const auto cmp = [&](const GroupedTitle& a, const GroupedTitle& b) {
        switch (q.sort) {
        case CatalogSort::NameAsc:
            return lower_copy(a.displayName) < lower_copy(b.displayName);
        case CatalogSort::NameDesc:
            return lower_copy(a.displayName) > lower_copy(b.displayName);
        case CatalogSort::SizeAsc:
            return a.totalSize < b.totalSize;
        case CatalogSort::SizeDesc:
            return a.totalSize > b.totalSize;
        }
        return false;
    };
    std::sort(out.begin(), out.end(), cmp);
    return out;
}

} // namespace thomaz::core
