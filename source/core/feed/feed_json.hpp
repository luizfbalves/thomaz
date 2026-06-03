#pragma once
#include <optional>
#include <string>
#include <vector>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// --- Request body builders (pure) ---
std::string build_credentials_body(const std::string& user, const std::string& pass);
std::string build_refresh_body(const std::string& refreshToken);
std::string build_like_body(bool liked);
std::string build_comment_body(const std::string& text);

// --- Response parsers (implemented in Task 3) ---
struct RefreshResult { bool ok = false; std::string token; std::string refreshToken; };

AuthResult           parse_auth_response(const std::string& body, long status);
RefreshResult        parse_refresh_response(const std::string& body, long status);
FeedPage             parse_feed_page(const std::string& body);
std::vector<Comment> parse_comments(const std::string& body);
std::optional<Post>  parse_post(const std::string& body);

} // namespace thomaz::feed
