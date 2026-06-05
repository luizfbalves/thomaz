#include "platform/themes/qlaunch_patches.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace thomaz {

namespace {

// Bundled patches ship in the app romfs (resources/theme-patches at build time).
std::string patch_src_dir() {
#ifdef __SWITCH__
    return "romfs:/theme-patches";
#else
    return "resources/theme-patches";
#endif
}

// Atmosphère scans every subfolder of exefs_patches; the subfolder name is
// arbitrary and just groups a patch set.
std::string exefs_patches_dir() {
#ifdef __SWITCH__
    return "/atmosphere/exefs_patches/thomaz_themes";
#else
    return "themes-out/exefs_patches/thomaz_themes";
#endif
}

// mkdir -p for a directory path (POSIX, FAT-safe).
void ensure_dirs(const std::string& dir) {
    std::string acc;
    for (size_t i = 0; i < dir.size(); ++i) {
        acc.push_back(dir[i]);
        if (dir[i] == '/' && acc.size() > 1) ::mkdir(acc.c_str(), 0777);
    }
    ::mkdir(dir.c_str(), 0777);
}

bool copy_file(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return false;
    std::vector<char> buf((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!buf.empty()) out.write(buf.data(), (std::streamsize)buf.size());
    return (bool)out;
}

bool ends_with_ips(const std::string& n) {
    return n.size() > 4 && n.compare(n.size() - 4, 4, ".ips") == 0;
}

} // namespace

int install_qlaunch_patches() {
    const std::string src = patch_src_dir();
    DIR* d = ::opendir(src.c_str());
    if (!d) return 0;

    const std::string dst = exefs_patches_dir();
    ensure_dirs(dst);

    int written = 0;
    while (dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (!ends_with_ips(name)) continue;
        if (copy_file(src + "/" + name, dst + "/" + name)) ++written;
    }
    ::closedir(d);
    return written;
}

} // namespace thomaz
