#include "core/feed/session_codec.hpp"
#include <sstream>

namespace thomaz::feed {

std::string serialize_session(const Session& s)
{
    return s.token + "\n" + s.refreshToken + "\n" + s.username + "\n";
}

std::optional<Session> parse_session(const std::string& text)
{
    std::istringstream in(text);
    std::string l1, l2, l3;
    if (!std::getline(in, l1))
        return std::nullopt;
    bool hasL2 = static_cast<bool>(std::getline(in, l2));
    bool hasL3 = static_cast<bool>(std::getline(in, l3));

    auto trim = [](std::string& v) {
        const char* ws = " \t\r\n";
        auto a = v.find_first_not_of(ws);
        if (a == std::string::npos) { v.clear(); return; }
        auto b = v.find_last_not_of(ws);
        v = v.substr(a, b - a + 1);
    };

    Session s;
    if (hasL2 && hasL3) {
        // New 3-line format: token / refreshToken / username
        s.token = l1; s.refreshToken = l2; s.username = l3;
    } else if (hasL2) {
        // Legacy 2-line format: token / username (no refresh token)
        s.token = l1; s.refreshToken = ""; s.username = l2;
    } else {
        return std::nullopt;
    }
    trim(s.token); trim(s.refreshToken); trim(s.username);

    if (s.token.empty() || s.username.empty())
        return std::nullopt;
    return s;
}

} // namespace thomaz::feed
