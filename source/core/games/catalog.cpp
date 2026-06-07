#include "core/games/catalog.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>

namespace thomaz::core {

namespace {

std::string basename_from_url(const std::string& url) {
    const auto q = url.find('?');
    const auto h = url.find('#');
    std::size_t end = url.size();
    if (q != std::string::npos)
        end = std::min(end, q);
    if (h != std::string::npos)
        end = std::min(end, h);
    const auto slash = url.rfind('/', end);
    if (slash == std::string::npos || slash + 1 >= end)
        return url.substr(0, end);
    return url.substr(slash + 1, end - slash - 1);
}

std::optional<std::uint64_t> parse_hex16(const std::string& s) {
    if (s.size() != 16)
        return std::nullopt;
    std::uint64_t v = 0;
    for (char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return std::nullopt;
        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= static_cast<std::uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f')
            v |= static_cast<std::uint64_t>(c - 'a' + 10);
        else
            v |= static_cast<std::uint64_t>(c - 'A' + 10);
    }
    return v;
}

std::optional<std::uint64_t> title_id_from_url(const std::string& url) {
    for (std::size_t i = 0; i + 18 <= url.size(); ++i) {
        if (url[i] == '[' && url[i + 17] == ']') {
            if (auto id = parse_hex16(url.substr(i + 1, 16)))
                return id;
        }
    }
    return std::nullopt;
}

std::string display_name_for(const IndexFile& f) {
    if (!f.nameOverride.empty())
        return f.nameOverride;
    return basename_from_url(f.url);
}

CatalogRow row_from_file(const IndexFile& f) {
    CatalogRow row;
    row.url         = f.url;
    row.size        = f.size;
    row.nameOverride = f.nameOverride;
    if (auto id = title_id_from_url(f.url)) {
        row.titleId = *id;
        row.kind    = title_kind(*id);
    } else {
        row.kind = TitleKind::Unknown;
    }
    return row;
}

} // namespace

std::vector<GroupedTitle> group_catalog(const ParsedIndex& idx) {
    struct GroupKey {
        std::uint64_t baseId;
        std::string   unknownKey;
        bool          operator<(const GroupKey& o) const {
            if (baseId != o.baseId)
                return baseId < o.baseId;
            return unknownKey < o.unknownKey;
        }
    };

    std::map<GroupKey, GroupedTitle> groups;

    for (const auto& f : idx.files) {
        CatalogRow row = row_from_file(f);
        GroupKey key;
        if (row.titleId != 0) {
            key.baseId = base_title_id(row.titleId);
        } else {
            key.unknownKey = f.url;
        }

        GroupedTitle& g = groups[key];
        if (g.baseId == 0 && row.titleId != 0)
            g.baseId = key.baseId;
        g.rows.push_back(std::move(row));
        g.totalSize += f.size;
    }

    for (auto& [_, g] : groups) {
        g.hasUpdate = false;
        g.hasDlc    = false;
        for (const auto& r : g.rows) {
            if (r.kind == TitleKind::Update)
                g.hasUpdate = true;
            if (r.kind == TitleKind::Dlc)
                g.hasDlc = true;
        }
        if (!g.rows.empty()) {
            for (const auto& r : g.rows) {
                if (r.kind == TitleKind::Base) {
                    g.displayName = display_name_for({r.url, r.size, r.nameOverride});
                    break;
                }
            }
            if (g.displayName.empty())
                g.displayName = display_name_for({g.rows.front().url, g.rows.front().size,
                                                  g.rows.front().nameOverride});
        }
    }

    std::vector<GroupedTitle> out;
    out.reserve(groups.size());
    for (auto& [_, g] : groups)
        out.push_back(std::move(g));
    return out;
}

} // namespace thomaz::core
