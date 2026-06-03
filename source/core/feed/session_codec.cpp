#include "core/feed/session_codec.hpp"
#include <sstream>

namespace thomaz::feed {

std::string serialize_session(const Session& s)
{
    return s.token + "\n" + s.username + "\n";
}

std::optional<Session> parse_session(const std::string& text)
{
    std::istringstream in(text);
    Session s;
    if (!std::getline(in, s.token))
        return std::nullopt;
    if (!std::getline(in, s.username))
        return std::nullopt;

    auto trim = [](std::string& v) {
        const char* ws = " \t\r\n";
        auto a = v.find_first_not_of(ws);
        if (a == std::string::npos) { v.clear(); return; }
        auto b = v.find_last_not_of(ws);
        v = v.substr(a, b - a + 1);
    };
    trim(s.token);
    trim(s.username);

    if (s.token.empty() || s.username.empty())
        return std::nullopt;
    return s;
}

} // namespace thomaz::feed
