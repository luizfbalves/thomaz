#include "core/mods/gamebanana_urls.hpp"

namespace thomaz::core {

namespace {
const char* kBase = "https://gamebanana.com/apiv11";

bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
} // namespace

std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (is_unreserved(c)) {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

std::string gb_search_url(const std::string& query, std::uint64_t game_id, int page) {
    std::string game = game_id == 0 ? std::string() : std::to_string(game_id);
    return std::string(kBase) + "/Util/Search/Results?_sSearchString=" +
           url_encode(query) + "&_sModelName=Mod&_idGameRow=" + game +
           "&_nPage=" + std::to_string(page);
}

std::string gb_mod_files_url(std::uint64_t mod_id) {
    return std::string(kBase) + "/Mod/" + std::to_string(mod_id) +
           "?_csvProperties=_aFiles";
}

std::string gb_game_search_url(const std::string& query, int page) {
    return std::string(kBase) +
           "/Util/Search/Results?_sModelName=Game&_sOrder=best_match&_sSearchString=" +
           url_encode(query) + "&_nPage=" + std::to_string(page);
}

std::string gb_subfeed_url(std::uint64_t game_id, const std::string& query, int page) {
    std::string u = std::string(kBase) + "/Game/" + std::to_string(game_id) +
                    "/Subfeed?_nPage=" + std::to_string(page) +
                    "&_nPerpage=50&_csvModelInclusions=Mod";
    if (!query.empty())
        u += "&_sName=" + url_encode(query);
    return u;
}

} // namespace thomaz::core
