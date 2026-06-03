#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::feed {

struct Session {
    std::string token;        // short-lived access token
    std::string refreshToken; // long-lived, rotates on each /auth/refresh
    std::string username;
};

// Auth call result (moved here from feed_client.hpp so pure core code can
// produce it without depending on the platform layer).
struct AuthResult {
    bool        ok = false;
    std::string token;        // access token when ok
    std::string error;        // human/i18n message when !ok
    std::string refreshToken; // refresh token when ok (appended last on purpose)
};

} // namespace thomaz::feed
