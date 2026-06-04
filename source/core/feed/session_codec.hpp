#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace thomaz::feed {

struct Session {
    std::string token;        // short-lived access token
    std::string refreshToken; // long-lived, rotates on each /auth/refresh
    std::string username;
};

// Auth call result. Lives here so callers that depend only on auth/session
// do not need the now-deleted community-feed type header.
struct AuthResult {
    bool        ok = false;
    std::string token;        // access token when ok
    std::string error;        // human/i18n message when !ok
    std::string refreshToken; // refresh token when ok (appended last on purpose)
};

// Formato em disco: "<token>\n<username>\n". Funções puras (sem IO) para
// serem testáveis; o auth_store faz o read/write do arquivo.
std::string serialize_session(const Session& s);
std::optional<Session> parse_session(const std::string& text);

} // namespace thomaz::feed
