#include "platform/themes/theme_paths.hpp"
#include <sys/stat.h>

namespace thomaz {

std::string themes_root() {
#ifdef __SWITCH__
    return "/themes";
#else
    return "themes";
#endif
}

namespace {
// Replace filesystem-unsafe characters with '_'. Mirrors how a human-named
// folder must be safe on FAT32.
std::string sanitize_component(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '/': case '\\': case ':': case '*': case '?':
            case '"': case '<': case '>': case '|':
                out.push_back('_'); break;
            default: out.push_back(c);
        }
    }
    return out;
}
} // namespace

std::string theme_folder(const thomaz::core::ThemeEntry& entry) {
    std::string label = sanitize_component(entry.author) + " - " +
                        sanitize_component(entry.name);
    return themes_root() + "/" + label;
}

bool theme_already_downloaded(const thomaz::core::ThemeEntry& entry) {
    struct stat st;
    std::string f = theme_folder(entry);
    return ::stat(f.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

} // namespace thomaz
