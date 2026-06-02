#include "platform/self_update.hpp"

namespace thomaz {

namespace {
std::string g_self_path;

bool ends_with_nro(const std::string& s) {
    return s.size() >= 4 && s.compare(s.size() - 4, 4, ".nro") == 0;
}
} // namespace

void set_self_path(const char* argv0) {
    g_self_path = argv0 ? argv0 : "";
}

std::string update_target_path() {
    // hbmenu passes the launch path as argv[0]; prefer it so we replace the
    // actual running file wherever it lives. Fall back to the canonical path.
    if (ends_with_nro(g_self_path))
        return g_self_path;
    return "/switch/thomaz.nro";
}

} // namespace thomaz
