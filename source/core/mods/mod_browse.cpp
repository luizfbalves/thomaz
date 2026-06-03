#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_json.hpp"
#include "core/mods/gamebanana_urls.hpp"

namespace thomaz::core {

BrowseResult search_mods(const std::string& query, std::uint64_t game_id, int page,
                         const UrlFetcher& fetch) {
    BrowseResult result;
    std::optional<std::string> body = fetch(gb_search_url(query, game_id, page));
    if (!body) {
        result.status = BrowseStatus::NetworkError;
        return result;
    }
    result.page   = parse_search_page(*body);
    result.status = BrowseStatus::Ok;
    return result;
}

ResolveResult resolve_mod_files(std::uint64_t mod_id, const UrlFetcher& fetch) {
    ResolveResult result;
    std::optional<std::string> body = fetch(gb_mod_files_url(mod_id));
    if (!body) {
        result.status = ResolveStatus::NetworkError;
        return result;
    }
    ModFilesResult parsed = parse_mod_files(*body);
    if (!parsed.ok) {
        result.status = ResolveStatus::NotFound;
        result.error  = parsed.error;
        return result;
    }
    result.status = ResolveStatus::Ok;
    result.files  = std::move(parsed.files);
    return result;
}

} // namespace thomaz::core
