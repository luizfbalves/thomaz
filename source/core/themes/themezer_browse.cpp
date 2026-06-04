#include "core/themes/themezer_browse.hpp"
#include "core/themes/themezer_query.hpp"
#include "core/themes/themezer_json.hpp"

namespace thomaz::core::themezer {

BrowseResult browse_themes(const std::string& query, const std::string& target,
                           int page, int limit, const GraphQlFetcher& fetch) {
    BrowseResult r;
    auto body = fetch(themes_feed_body(query, target, page, limit));
    if (!body) return r; // NetworkError
    r.page   = parse_browse_page(*body, ThemeKind::Theme);
    r.status = BrowseStatus::Ok;
    return r;
}

BrowseResult browse_packs(const std::string& query, int page, int limit,
                          const GraphQlFetcher& fetch) {
    BrowseResult r;
    auto body = fetch(packs_feed_body(query, page, limit));
    if (!body) return r;
    r.page   = parse_browse_page(*body, ThemeKind::Pack);
    r.status = BrowseStatus::Ok;
    return r;
}

DetailResult theme_detail(const std::string& hex_id, const GraphQlFetcher& fetch) {
    DetailResult r;
    auto body = fetch(theme_detail_body(hex_id));
    if (!body) return r; // NetworkError
    bool found = false;
    r.detail = parse_theme_detail(*body, &found);
    r.status = found ? DetailStatus::Ok : DetailStatus::NotFound;
    return r;
}

DetailResult pack_detail(const std::string& hex_id, const GraphQlFetcher& fetch) {
    DetailResult r;
    auto body = fetch(pack_detail_body(hex_id));
    if (!body) return r;
    bool found = false;
    r.detail = parse_pack_detail(*body, &found);
    r.status = found ? DetailStatus::Ok : DetailStatus::NotFound;
    return r;
}

} // namespace thomaz::core::themezer
