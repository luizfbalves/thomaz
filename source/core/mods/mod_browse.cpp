#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_json.hpp"
#include "core/mods/gamebanana_urls.hpp"
#include "core/mods/gamebanana_overrides.hpp"

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

GameResolve resolve_game(std::uint64_t title_id, const std::string& name,
                         const UrlFetcher& fetch) {
    GameResolve r;
    if (auto ov = gamebanana_game_override(title_id)) {
        r.status = GameResolveStatus::Ok;
        r.game_id = *ov;
        r.matched_name = name;
        r.source = GameResolveSource::Override;
        return r;
    }
    std::optional<std::string> body = fetch(gb_game_search_url(name + " (Switch)", 1));
    if (!body) {
        r.status = GameResolveStatus::NetworkError;
        return r;
    }
    SearchPage page = parse_search_page(*body);
    if (page.records.empty()) {
        r.status = GameResolveStatus::NotFound;
        return r;
    }
    const ModRecord* chosen = &page.records[0];
    for (const ModRecord& rec : page.records) {
        if (rec.name.find("Switch") != std::string::npos) {
            chosen = &rec;
            break;
        }
    }
    r.status = GameResolveStatus::Ok;
    r.game_id = chosen->id;
    r.matched_name = chosen->name;
    r.source = GameResolveSource::NameMatch;
    return r;
}

BrowseResult list_game_mods(std::uint64_t game_id, const std::string& query,
                            int page, const UrlFetcher& fetch) {
    BrowseResult result;
    std::optional<std::string> body = fetch(gb_subfeed_url(game_id, query, page));
    if (!body) {
        result.status = BrowseStatus::NetworkError;
        return result;
    }
    result.page = parse_search_page(*body);
    result.status = BrowseStatus::Ok;
    return result;
}

} // namespace thomaz::core
