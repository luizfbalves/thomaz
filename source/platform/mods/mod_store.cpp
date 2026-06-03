#include "platform/mods/mod_store.hpp"

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

bool copy_file(const std::string& src, const std::string& dst) {
    std::FILE* in = std::fopen(src.c_str(), "rb");
    if (!in)
        return false;
    std::FILE* out = std::fopen(dst.c_str(), "wb");
    if (!out) {
        std::fclose(in);
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

bool remove_tree(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d) {
        // Not a directory (or missing): try removing as a file; success if gone.
        ::remove(dir.c_str());
        struct stat st;
        return ::stat(dir.c_str(), &st) != 0;
    }

    bool ok = true;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string child = dir + "/" + name;
        if (is_dir(child)) {
            if (!remove_tree(child))
                ok = false; // best-effort: keep going, report failure
        } else if (::remove(child.c_str()) != 0) {
            ok = false;
        }
    }
    ::closedir(d);
    ::rmdir(dir.c_str());

    struct stat st;
    bool gone = ::stat(dir.c_str(), &st) != 0;
    return ok && gone;
}

std::vector<std::string> list_subdirs(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        if (is_dir(dir + "/" + name))
            out.push_back(name);
    }
    ::closedir(d);
    return out;
}

std::optional<std::string> read_marker(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return std::nullopt;
    std::string out;
    char buf[256];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    // Trim a single trailing newline if present.
    if (!out.empty() && out.back() == '\n')
        out.pop_back();
    return out;
}

bool write_marker(const std::string& path, const std::string& value) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    std::size_t w = std::fwrite(value.data(), 1, value.size(), f);
    bool ok = (w == value.size());
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

bool clear_marker(const std::string& path) {
    ::remove(path.c_str());
    struct stat st;
    return ::stat(path.c_str(), &st) != 0;
}

} // namespace thomaz
