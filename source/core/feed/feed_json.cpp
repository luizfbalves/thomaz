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

// Parsers added in Task 3.

} // namespace thomaz::feed
