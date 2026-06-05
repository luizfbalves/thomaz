#include "platform/mods/mod_store.hpp"
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

} // namespace

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
    // WR-04: cap the read — a marker is one short line, but it lives on an
    // SD-card path a user/malformed download could replace with a huge file,
    // and the result is later used as a path component.
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < 4096)
        out.append(buf, n);
    // WR-05: distinguish EOF from a mid-read I/O error (flaky SD card). A
    // truncated-but-successful-looking read would silently select the wrong
    // active mod — mirror save_service_switch's ferror precedent and fail.
    bool readErr = std::ferror(f) != 0;
    std::fclose(f);
    if (readErr)
        return std::nullopt;
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
