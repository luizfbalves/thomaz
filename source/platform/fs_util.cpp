#include "platform/fs_util.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace thomaz {

namespace {

bool is_dir(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// Copy a single file from src to dst. On open failure for dst, removes any
// ghost file so the destination is left clean (behaviour from save_service_switch).
bool copy_file(const std::string& src, const std::string& dst) {
    std::FILE* in = std::fopen(src.c_str(), "rb");
    if (!in)
        return false;
    std::FILE* out = std::fopen(dst.c_str(), "wb");
    if (!out) {
        std::fclose(in);
        ::remove(dst.c_str()); // no ghost file (save_service_switch nicety)
        return false;
    }
    char buf[8192];
    std::size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
        if (std::fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    ok = (std::fclose(out) == 0) && ok;
    std::fclose(in);
    return ok;
}

} // namespace

// CANONICAL ensure_parent_dirs — substring-at-slash form from cheat_store.cpp:11-21.
// Walks each '/' boundary (i=1..size()) and creates the directory up to that point.
// Handles trailing slashes: "x/y/" creates x and x/y.
void ensure_parent_dirs(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i] != '/')
            continue;
        std::string dir = path.substr(0, i);
        if (!dir.empty())
            ::mkdir(dir.c_str(), 0777); // ignore EEXIST and friends
    }
}

bool copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err) {
    ::mkdir(dst_dir.c_str(), 0777); // ignore EEXIST

    DIR* d = ::opendir(src_dir.c_str());
    if (!d) {
        if (err)
            *err = "cannot open " + src_dir;
        return false;
    }

    bool ok = true;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string src = src_dir + "/" + name;
        std::string dst = dst_dir + "/" + name;
        if (is_dir(src)) {
            if (!copy_tree(src, dst, err)) {
                ok = false;
                break;
            }
        } else if (!copy_file(src, dst)) {
            if (err)
                *err = "cannot copy " + src;
            ok = false;
            break;
        }
    }

    ::closedir(d);
    return ok;
}

} // namespace thomaz
