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
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);

    std::fclose(f);
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
