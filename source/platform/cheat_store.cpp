#include "platform/cheat_store.hpp"
#include "platform/fs_util.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

namespace thomaz {

std::optional<std::string> read_text_file(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return std::nullopt;

    std::string out;
    char buf[4096];
    std::size_t n;
    // WR-04: cap the read — cheat/version-cache files are small, but they live
    // on SD-card paths that a malformed download could replace with an
    // arbitrarily large file (unbounded allocation on a memory-constrained
    // console). 1 MiB is far beyond any legitimate cheat .txt or version cache.
    constexpr std::size_t kMaxBytes = 1u << 20;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < kMaxBytes)
        out.append(buf, n);

    // WR-05: distinguish EOF from a mid-read I/O error. fread()==0 is true for
    // both; without ferror a flaky-SD truncation yields a half-parsed cache that
    // looks like valid content. Mirror save_service_switch's precedent.
    bool readErr = std::ferror(f) != 0;
    std::fclose(f);
    if (readErr)
        return std::nullopt;
    return out;
}

bool write_text_file(const std::string& path, const std::string& body) {
    ensure_parent_dirs(path);

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;

    std::size_t written = std::fwrite(body.data(), 1, body.size(), f);
    bool ok = (written == body.size());
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

std::string index_cache_path() {
#ifdef __SWITCH__
    return "/switch/thomaz/cache/versions.json";
#else
    return "thomaz-cache/versions.json";
#endif
}

bool dir_has_nonempty_txt(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return false;

    bool found = false;
    while (struct dirent* entry = ::readdir(d)) {
        std::string name = entry->d_name;
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".txt") != 0)
            continue;

        std::string full = dir + "/" + name;
        struct stat st;
        if (::stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
            found = true;
            break;
        }
    }

    ::closedir(d);
    return found;
}

int clear_cheat_files(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return 0;

    int removed = 0;
    while (struct dirent* entry = ::readdir(d)) {
        std::string name = entry->d_name;
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".txt") != 0)
            continue; // only ever delete cheat .txt files

        std::string full = dir + "/" + name;
        if (::remove(full.c_str()) == 0)
            ++removed;
    }

    ::closedir(d);
    return removed;
}

} // namespace thomaz
