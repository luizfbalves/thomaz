#include "platform/self_update.hpp"
#include "platform/mods/mod_download.hpp"
#include <borealis.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
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
                             std::string* err,
                             std::shared_ptr<std::atomic<bool>> cancelled) {
    const std::string tmp = target + ".tmp";
    brls::Logger::info("thomaz/update: downloading {} -> {}", url, tmp);

    if (!download_file(url, tmp, nullptr, err, cancelled)) {
        // download_file already removed the partial tmp file; log and bail.
        brls::Logger::error("thomaz/update: download failed: {}",
                            err && !err->empty() ? *err : "(no detail)");
        std::remove(tmp.c_str()); // belt-and-suspenders
        return false;
    }

    // Reject a zero-byte "success" (empty 200 body / truncated asset) — renaming
    // it over the running .nro would brick the app.
    struct stat st;
    if (::stat(tmp.c_str(), &st) != 0 || st.st_size == 0) {
        if (err) *err = "downloaded update is empty";
        brls::Logger::error("thomaz/update: {}", *err);
        std::remove(tmp.c_str());
        return false;
    }
    brls::Logger::info("thomaz/update: download OK ({} bytes), installing to {}",
                       (long long)st.st_size, target);

    // Switch FAT (Horizon OS RenameFile) does NOT support renaming over an
    // existing destination — it returns an error when the target already exists.
    // Remove the target first so rename can succeed; there is a brief window
    // where the .nro is absent on the SD card, but the running copy is already
    // in RAM and the window is sub-millisecond in practice.
    if (std::remove(target.c_str()) != 0 && errno != ENOENT) {
        // Only fail if the remove truly failed for a reason other than "not found"
        // (ENOENT means no existing target — fine for a first-time install).
        if (err) *err = "could not remove existing " + target + ": " + std::strerror(errno);
        brls::Logger::error("thomaz/update: {}", *err);
        std::remove(tmp.c_str());
        return false;
    }

    if (std::rename(tmp.c_str(), target.c_str()) != 0) {
        if (err) *err = "could not install " + tmp + " -> " + target + ": " + std::strerror(errno);
        brls::Logger::error("thomaz/update: {}", *err);
        std::remove(tmp.c_str());
        return false;
    }

    brls::Logger::info("thomaz/update: installed successfully");
    return true;
}

} // namespace thomaz
