#include "core/feed/feed_json.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>

namespace thomaz::feed {

using nlohmann::json;

std::string build_credentials_body(const std::string& user, const std::string& pass)
{
    return json{ {"username", user}, {"password", pass} }.dump();
}

std::string build_refresh_body(const std::string& refreshToken)
{
    return json{ {"refreshToken", refreshToken} }.dump();
}

std::string build_like_body(bool liked)
{
    return json{ {"liked", liked} }.dump();
}

std::string build_comment_body(const std::string& text)
{
    return json{ {"text", text} }.dump();
}

namespace {

// Non-throwing parse: returns a discarded json on any error.
json safe_parse(const std::string& body)
{
    return json::parse(body, nullptr, /*allow_exceptions=*/false);
}

std::uint64_t parse_title_id_hex(const json& j, const char* key)
{
    if (!j.contains(key) || !j.at(key).is_string()) return 0;
    const std::string s = j.at(key).get<std::string>();
    if (s.empty()) return 0;
    return std::strtoull(s.c_str(), nullptr, 16);
}

User parse_user(const json& j)
{
    User u;
    if (j.is_object()) {
        u.id       = j.value("id", std::string{});
        u.username = j.value("username", std::string{});
    }
    return u;
}

Post parse_post_obj(const json& j)
{
    Post p;
    p.id           = j.value("id", std::string{});
    if (j.contains("author")) p.author = parse_user(j.at("author"));
    p.imageUrl     = j.value("imageUrl", std::string{});
    p.caption      = j.value("caption", std::string{});
    p.gameTitleId  = parse_title_id_hex(j, "gameTitleId");
    p.gameName     = j.value("gameName", std::string{});
    p.likeCount    = j.value("likeCount", 0);
    p.likedByMe    = j.value("likedByMe", false);
    p.commentCount = j.value("commentCount", 0);
    p.createdAt    = j.value("createdAt", static_cast<std::int64_t>(0));
    return p;
}

Comment parse_comment_obj(const json& j)
{
    Comment c;
    c.id        = j.value("id", std::string{});
    if (j.contains("author")) c.author = parse_user(j.at("author"));
    c.text      = j.value("text", std::string{});
    c.createdAt = j.value("createdAt", static_cast<std::int64_t>(0));
    return c;
}

// Maps a failed/non-2xx auth response to a stable, caller-displayable message.
// The activities show AuthResult.error verbatim when non-empty.
std::string auth_error_message(const json& j, long status)
{
    if (status == 0)   return "Sem conexão com o servidor.";
    if (status == 401) return "Usuário ou senha inválidos.";
    if (status == 409) return "Esse nome de usuário já existe.";
    if (j.is_object() && j.contains("error") && j.at("error").is_string())
        return j.at("error").get<std::string>();
    return "Falha ao autenticar. Tente novamente.";
}

} // namespace

AuthResult parse_auth_response(const std::string& body, long status)
{
    AuthResult r;
    json j = safe_parse(body);
    const bool twoxx = (status >= 200 && status < 300);
    if (twoxx && !j.is_discarded() && j.value("ok", false)) {
        r.ok           = true;
        r.token        = j.value("token", std::string{});
        r.refreshToken = j.value("refreshToken", std::string{});
        if (r.token.empty()) { r.ok = false; r.error = auth_error_message(j, status); }
        return r;
    }
    r.ok = false;
    r.error = auth_error_message(j.is_discarded() ? json::object() : j, status);
    return r;
}

RefreshResult parse_refresh_response(const std::string& body, long status)
{
    RefreshResult r;
    json j = safe_parse(body);
    const bool twoxx = (status >= 200 && status < 300);
    if (twoxx && !j.is_discarded() && j.value("ok", false)) {
        r.token        = j.value("token", std::string{});
        r.refreshToken = j.value("refreshToken", std::string{});
        r.ok           = !r.token.empty() && !r.refreshToken.empty();
    }
    return r;
}

FeedPage parse_feed_page(const std::string& body)
{
    FeedPage page;
    json j = safe_parse(body);
    if (j.is_discarded() || !j.is_object()) return page;
    if (j.contains("posts") && j.at("posts").is_array())
        for (const auto& jp : j.at("posts"))
            if (jp.is_object()) page.posts.push_back(parse_post_obj(jp));
    page.nextCursor = j.value("nextCursor", std::string{});
    page.hasMore    = j.value("hasMore", false);
    return page;
}

std::vector<Comment> parse_comments(const std::string& body)
{
    std::vector<Comment> out;
    json j = safe_parse(body);
    if (j.is_discarded()) return out;
    // Tolerates bare array OR {"comments":[...]}. Keep whichever the API uses.
    const json* arr = nullptr;
    if (j.is_array()) arr = &j;
    else if (j.is_object() && j.contains("comments") && j.at("comments").is_array())
        arr = &j.at("comments");
    if (arr)
        for (const auto& jc : *arr)
            if (jc.is_object()) out.push_back(parse_comment_obj(jc));
    return out;
}

std::optional<Post> parse_post(const std::string& body)
{
    json j = safe_parse(body);
    if (j.is_discarded()) return std::nullopt;
    if (j.is_object() && j.contains("post") && j.at("post").is_object())
        return parse_post_obj(j.at("post"));
    if (j.is_object() && j.contains("id"))
        return parse_post_obj(j);
    return std::nullopt;
}

} // namespace thomaz::feed
