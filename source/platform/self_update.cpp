#include "platform/self_update.hpp"
#include "platform/mods/mod_download.hpp"
#include <cstdio>
#include <sys/stat.h>

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

bool apply_downloaded_update(const std::string& url, const std::string& target,
                             std::string* err) {
    const std::string tmp = target + ".tmp";
    if (!download_file(url, tmp, nullptr, err)) {
        std::remove(tmp.c_str()); // download_file already cleans up; belt-and-suspenders
        return false;
    }
    // Reject a zero-byte "success" (empty 200 body / truncated asset) — renaming
    // it over the running .nro would brick the app.
    struct stat st;
    if (::stat(tmp.c_str(), &st) != 0 || st.st_size == 0) {
        if (err) *err = "downloaded update is empty";
        std::remove(tmp.c_str());
        return false;
    }
    // Atomic swap: tmp and target are in the same directory (same filesystem),
    // so rename replaces the running .nro in one step — no partial-write window.
    if (std::rename(tmp.c_str(), target.c_str()) != 0) {
        if (err) *err = "could not replace " + target;
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

} // namespace thomaz
