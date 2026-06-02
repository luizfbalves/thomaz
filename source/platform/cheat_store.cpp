#include "platform/cheat_store.hpp"

#include <cstdio>
#include <sys/stat.h>

namespace thomaz {

namespace {

// mkdir -p for the directory portion of `path` (everything before the last '/').
void ensure_parent_dirs(const std::string& path) {
    // Walk each '/' boundary and create the directory up to that point.
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i] != '/')
            continue;
        std::string dir = path.substr(0, i);
        if (!dir.empty())
            ::mkdir(dir.c_str(), 0777); // ignore EEXIST and friends
    }
}

} // namespace

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

} // namespace thomaz
