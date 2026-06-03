#include "platform/feed/auth_store.hpp"
#include "platform/cheat_store.hpp"      // read_text_file / write_text_file
#include "core/feed/session_codec.hpp"
#include <cstdio>

namespace thomaz {

namespace {
std::string session_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/session.txt";
#else
    return "thomaz-cache/session.txt";
#endif
}
} // namespace

std::optional<feed::Session> load_session()
{
    if (auto raw = read_text_file(session_file()))
        return feed::parse_session(*raw);
    return std::nullopt;
}

void save_session(const feed::Session& s)
{
    write_text_file(session_file(), feed::serialize_session(s));
}

void clear_session()
{
    std::remove(session_file().c_str());
}

} // namespace thomaz
