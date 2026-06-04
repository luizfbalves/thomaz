#include "platform/sysmod/sysmod_store.hpp"
#include "core/sysmod/sysmod_paths.hpp"
#include "core/sysmod/sysmod_scan.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace thomaz {

namespace {

bool path_exists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
}

bool is_dir(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string read_file(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (!f)
        return {};
    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    return out;
}

} // namespace

std::vector<core::RawSysmoduleEntry> sysmod_scan_contents(const std::string& root) {
    std::vector<core::RawSysmoduleEntry> out;
    DIR* d = ::opendir(root.c_str());
    if (!d)
        return out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string dir = root + "/" + name;
        if (!is_dir(dir))
            continue;
        core::RawSysmoduleEntry entry;
        entry.program_id     = name;
        entry.has_exefs      = path_exists(dir + "/exefs.nsp");
        entry.has_boot2_flag = path_exists(dir + "/flags/boot2.flag");
        entry.toolbox_json   = read_file(dir + "/toolbox.json");
        out.push_back(std::move(entry));
    }
    ::closedir(d);
    return out;
}

bool sysmod_set_boot2_flag(const std::string& contents_dir, bool enabled) {
    std::string flags_dir = contents_dir + "/flags";
    std::string flag      = flags_dir + "/boot2.flag";
    if (enabled) {
        ::mkdir(flags_dir.c_str(), 0777); // ignore EEXIST
        if (path_exists(flag))
            return true;
        std::FILE* f = std::fopen(flag.c_str(), "wb");
        if (!f)
            return false;
        return std::fclose(f) == 0;
    }
    ::remove(flag.c_str());
    return !path_exists(flag);
}

SysmoduleStore::SysmoduleStore(std::string contents_root) : root(std::move(contents_root)) {}
SysmoduleStore::SysmoduleStore() : root(core::sysmod_contents_root()) {}

std::vector<core::Sysmodule> SysmoduleStore::list() {
    return core::build_sysmodule_list(sysmod_scan_contents(root));
}

bool SysmoduleStore::setEnabled(const std::string& program_id, bool enabled) {
    return sysmod_set_boot2_flag(root + "/" + program_id, enabled);
}

} // namespace thomaz
