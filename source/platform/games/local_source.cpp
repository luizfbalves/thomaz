#include "platform/games/local_source.hpp"

#include "core/games/recurse_plan.hpp"
#include "platform/fs_util.hpp"

#include <cstring>

#include <dirent.h>
#include <sys/stat.h>

namespace thomaz {

namespace {

bool ends_with_ci(const std::string& name, const char* ext) {
    const std::size_t elen = std::strlen(ext);
    if (name.size() < elen)
        return false;
    return name.compare(name.size() - elen, elen, ext) == 0;
}

bool is_game_archive(const std::string& name) {
    return ends_with_ci(name, ".nsp") || ends_with_ci(name, ".nsz");
}

bool scan_dir(const std::string& dir, int depth, const thomaz::core::RecurseBounds& bounds,
              std::vector<thomaz::core::IndexFile>& out, bool* truncated) {
    if (depth > bounds.maxDepth)
        return true;
    if (out.size() >= bounds.maxEntries) {
        if (truncated)
            *truncated = true;
        return false;
    }

    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return true;

    while (struct dirent* entry = ::readdir(d)) {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string full = dir + "/" + name;
        struct stat st;
        if (::stat(full.c_str(), &st) != 0)
            continue;

        if (S_ISREG(st.st_mode) && is_game_archive(name)) {
            thomaz::core::IndexFile f;
            f.url          = full;
            f.size         = static_cast<std::uint64_t>(st.st_size);
            f.nameOverride = name;
            out.push_back(std::move(f));
            if (out.size() >= bounds.maxEntries) {
                if (truncated)
                    *truncated = true;
                ::closedir(d);
                return false;
            }
        } else if (S_ISDIR(st.st_mode)) {
            if (!scan_dir(full, depth + 1, bounds, out, truncated)) {
                ::closedir(d);
                return false;
            }
        }
    }

    ::closedir(d);
    return true;
}

} // namespace

std::string local_source_dir() {
#ifdef __SWITCH__
    return "/switch/thomaz/games";
#else
    return "thomaz-cache/games";
#endif
}

bool is_local_source(const thomaz::core::SourceConfig& cfg) {
    return cfg.url.rfind("local:", 0) == 0;
}

thomaz::core::SourceConfig make_local_peer_config() {
    thomaz::core::SourceConfig cfg;
    cfg.url      = kLocalSourceUrl;
    cfg.authType = thomaz::core::SourceAuthType::None;
    return cfg;
}

thomaz::core::ParsedIndex scan_local_files(bool* truncated) {
    thomaz::core::ParsedIndex out;
    if (truncated)
        *truncated = false;

    const thomaz::core::RecurseBounds bounds;
    const std::string root = local_source_dir();
    ensure_parent_dirs(root + "/.keep");

    scan_dir(root, 0, bounds, out.files, truncated);
    out.ok = true;
    return out;
}

} // namespace thomaz
