#include "core/themes/themezer_query.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

namespace {
std::string sanitize_hex(const std::string& s) {
    std::string out;
    for (char c : s)
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            out.push_back(c);
    return out;
}
std::string wrap(const std::string& query, const json& variables) {
    json body;
    body["query"]     = query;
    body["variables"] = variables;
    return body.dump();
}
} // namespace

std::string themes_feed_body(const std::string& query, const std::string& target,
                             int page, int limit) {
    static const char* kQuery =
        "query($q:String,$t:Target,$p:PaginationInput){ switch{ themes("
        "query:$q, target:$t, sort:DOWNLOADS, order:DESC, paginationArgs:$p){ "
        "pageInfo{page pageCount} nodes{ hexId name downloadCount downloadUrl "
        "target creator{username} screenshotPreview{jpgThumbUrl} } } } }";
    json v;
    v["q"] = query.empty()  ? json(nullptr) : json(query);
    v["t"] = target.empty() ? json(nullptr) : json(target);
    v["p"] = { {"page", page}, {"limit", limit} };
    return wrap(kQuery, v);
}

std::string packs_feed_body(const std::string& query, int page, int limit) {
    static const char* kQuery =
        "query($q:String,$p:PaginationInput){ switch{ packs("
        "query:$q, sort:DOWNLOADS, order:DESC, paginationArgs:$p){ "
        "pageInfo{page pageCount} nodes{ hexId name downloadCount downloadUrl "
        "creator{username} collagePreview{jpgThumbUrl} } } } }";
    json v;
    v["q"] = query.empty() ? json(nullptr) : json(query);
    v["p"] = { {"page", page}, {"limit", limit} };
    return wrap(kQuery, v);
}

std::string theme_detail_body(const std::string& hex_id) {
    std::string q =
        "{ switch{ theme(hexId:\"" + sanitize_hex(hex_id) + "\"){ hexId name "
        "description downloadUrl target creator{username} "
        "screenshotPreview{jpgThumbUrl} } } }";
    return wrap(q, json::object());
}

std::string pack_detail_body(const std::string& hex_id) {
    std::string q =
        "{ switch{ pack(hexId:\"" + sanitize_hex(hex_id) + "\"){ hexId name "
        "description downloadUrl creator{username} collagePreview{jpgThumbUrl} "
        "themes{ hexId name target downloadUrl } } } }";
    return wrap(q, json::object());
}

} // namespace thomaz::core
