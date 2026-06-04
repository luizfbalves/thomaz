#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_json.hpp"
#include "core/mods/gamebanana_urls.hpp"
#include "core/mods/gamebanana_overrides.hpp"

#include <cctype>

namespace thomaz::core {

namespace {

// Lowercase and keep only [a-z0-9] (drops spaces, punctuation, parentheses, and
// — because we strip the literal "switch" token first — the "(Switch)" suffix).
std::string normalize_name(const std::string& s) {
    std::string low;
    low.reserve(s.size());
    for (char c : s)
        low.push_back((char)std::tolower((unsigned char)c));
    // remove the word "switch" so "Splatoon (Switch)" and "Splatoon" compare equal
    for (std::string::size_type p; (p = low.find("switch")) != std::string::npos; )
        low.erase(p, 6);
    std::string out;
    out.reserve(low.size());
    for (unsigned char c : low)
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            out.push_back((char)c);
    return out;
}

// True only if the two normalized names are exactly equal.
//
// Substring matching is deliberately NOT used: "resident evil 4" is a substring
// of "resident evil 4 remake", which would silently resolve one game to another
// game's GameBanana page — and downloads stage under the *current* title, so the
// wrong game's mods end up in this game's folder. When the name isn't an exact
// match we return false, letting resolve_game fall back to manual search rather
// than guess wrong.
bool names_match(const std::string& a, const std::string& b) {
    return !a.empty() && !b.empty() && a == b;
}

} // anonymous namespace

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
    constexpr std::uint64_t kSwitchHubGameId = 6384;
    const std::string q = normalize_name(name);
    const ModRecord* chosen = nullptr;
    // Prefer a "(Switch)" page whose name matches the query.
    for (const ModRecord& rec : page.records) {
        if (rec.id == kSwitchHubGameId) continue;
        if (rec.name.find("(Switch)") != std::string::npos &&
            names_match(q, normalize_name(rec.name))) {
            chosen = &rec;
            break;
        }
    }
    // Otherwise any non-hub record whose name matches the query.
    if (!chosen) {
        for (const ModRecord& rec : page.records) {
            if (rec.id == kSwitchHubGameId) continue;
            if (names_match(q, normalize_name(rec.name))) {
                chosen = &rec;
                break;
            }
        }
    }
    if (!chosen) {            // no confident match -> let the UI fall back to manual search
        r.status = GameResolveStatus::NotFound;
        return r;
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
