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

} // namespace thomaz
