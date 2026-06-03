#include "platform/app_settings.hpp"
#include "platform/cheat_store.hpp"

namespace thomaz {

namespace {

std::string locale_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/locale.txt";
#else
    return "thomaz-cache/locale.txt";
#endif
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

std::string api_url_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/api_url.txt";
#else
    return "thomaz-cache/api_url.txt";
#endif
}

std::string default_api_base_url() {
#ifdef __SWITCH__
    // Production host on Lightsail. Override in Settings if you self-host.
    return "https://api.thomaz.baseup.cc";
#else
    return "http://localhost:3000";
#endif
}

std::string strip_trailing_slash(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

} // namespace

std::string load_locale() {
    if (auto saved = read_text_file(locale_file())) {
        std::string v = trim(*saved);
        if (!v.empty())
            return v;
    }
    return "auto";
}

void save_locale(const std::string& locale) {
    write_text_file(locale_file(), locale);
}

std::string load_api_base_url() {
    if (auto saved = read_text_file(api_url_file())) {
        std::string v = trim(*saved);
        if (!v.empty())
            return strip_trailing_slash(v);
    }
    return default_api_base_url();
}

void save_api_base_url(const std::string& url) {
    std::string v = strip_trailing_slash(trim(url));
    write_text_file(api_url_file(), v);
}

} // namespace thomaz
