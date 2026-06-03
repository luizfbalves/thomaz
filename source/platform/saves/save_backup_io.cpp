#include "platform/saves/save_backup_io.hpp"

#include <cstdint>
#include <cstdio>   // ::remove
#include <ctime>
#include <dirent.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file

namespace thomaz {

namespace {
std::string now_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}

// First path segment ("aaaa/save.dat" -> "aaaa"); empty if none.
std::string first_segment(const std::string& path) {
    auto slash = path.find('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

// Reject absolute paths and any ".." traversal segment. Blobs may come from the
// network, so a save file must never escape its backup directory.
bool is_safe_relpath(const std::string& path) {
    if (path.empty() || path.front() == '/') return false;
    size_t start = 0;
    for (;;) {
        size_t slash = path.find('/', start);
        std::string seg = (slash == std::string::npos)
                              ? path.substr(start)
                              : path.substr(start, slash - start);
        if (seg == "..") return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

// Recursively delete a directory tree (POSIX), mirroring the cleanup that
// NsSaveService::backup() does so a failed import never leaves a stale,
// manifest-less directory under saves_root().
void remove_tree(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string p = dir + "/" + name;
        struct stat st;
        if (::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            remove_tree(p);
            ::rmdir(p.c_str());
        } else {
            ::remove(p.c_str());
        }
    }
    ::closedir(d);
}
} // namespace

bool write_package_as_backup(std::uint64_t title_id, const std::string& game_name,
                             const core::SavePackage& pkg, std::string* outError) {
    // Validate all paths BEFORE creating anything — a network blob must not be
    // able to write outside its backup directory.
    for (const auto& f : pkg.files) {
        if (!is_safe_relpath(f.path)) {
            if (outError) *outError = "unsafe save path";
            return false;
        }
    }

    std::string ts  = now_timestamp();
    std::string dir = core::backup_dir(core::saves_root(), title_id, ts);

    std::set<std::string> profileSet;
    for (const auto& f : pkg.files) {
        std::string body(f.bytes.begin(), f.bytes.end());
        if (!write_text_file(dir + "/" + f.path, body)) {
            remove_tree(dir);
            ::rmdir(dir.c_str());
            if (outError) *outError = "could not write save file";
            return false;
        }
        std::string seg = first_segment(f.path);
        if (!seg.empty()) profileSet.insert(seg);
    }

    core::ManifestInfo m;
    m.game_name = game_name;
    m.title_id  = title_id;
    m.timestamp = ts;
    m.profiles.assign(profileSet.begin(), profileSet.end());
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        remove_tree(dir);
        ::rmdir(dir.c_str());
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

} // namespace thomaz
