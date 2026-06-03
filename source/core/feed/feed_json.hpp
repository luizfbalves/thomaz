#pragma once
#include <string>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// --- Auth request body builder (pure) ---
std::string build_credentials_body(const std::string& user, const std::string& pass);

// --- Auth response parser ---
AuthResult parse_auth_response(const std::string& body, long status);

} // namespace thomaz::feed
