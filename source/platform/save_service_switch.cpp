#include "platform/save_service_switch.hpp"

#ifdef __SWITCH__

#include <switch.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <vector>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file

namespace thomaz {

namespace {

constexpr const char* kMount = "thomaz_save"; // mount name -> "thomaz_save:/"

std::string now_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}

// 32-hex string for an AccountUid (high then low), used as the SD folder name.
std::string uid_hex(AccountUid uid) {
    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016lx%016lx",
                  (unsigned long)uid.uid[0], (unsigned long)uid.uid[1]);
    return buf;
}

// mkdir -p
void make_dirs(const std::string& path) {
    std::string acc;
    for (size_t i = 0; i < path.size(); ++i) {
        acc += path[i];
        if (path[i] == '/' && acc.size() > 1)
            ::mkdir(acc.c_str(), 0777);
    }
    ::mkdir(path.c_str(), 0777);
}

// Recursively copy everything under src dir into dst dir (both already exist).
bool copy_tree(const std::string& src, const std::string& dst) {
    DIR* d = ::opendir(src.c_str());
    if (!d)
        return false;
    bool ok = true;
    struct dirent* e;
    while (ok && (e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string s = src + "/" + name;
        std::string t = dst + "/" + name;
        struct stat st;
        if (::stat(s.c_str(), &st) != 0) { ok = false; break; }
        if (S_ISDIR(st.st_mode)) {
            ::mkdir(t.c_str(), 0777);
            ok = copy_tree(s, t);
        } else {
            FILE* in = std::fopen(s.c_str(), "rb");
            FILE* out = std::fopen(t.c_str(), "wb");
            if (!in || !out) { ok = false; }
            char buf[8192];
            size_t n;
            while (ok && (n = std::fread(buf, 1, sizeof(buf), in)) > 0)
                if (std::fwrite(buf, 1, n, out) != n) ok = false;
            if (in) std::fclose(in);
            if (out) std::fclose(out);
        }
    }
    ::closedir(d);
    return ok;
}

// Recursively delete the contents of a directory (leaves the dir itself).
void clear_tree(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string p = dir + "/" + name;
        struct stat st;
        if (::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            clear_tree(p);
            ::rmdir(p.c_str());
        } else {
            ::remove(p.c_str());
        }
    }
    ::closedir(d);
}

std::vector<core::SaveProfile> all_profiles() {
    std::vector<core::SaveProfile> out;
    AccountUid uids[ACC_USER_LIST_SIZE];
    s32 count = 0;
    if (R_FAILED(accountListAllUsers(uids, ACC_USER_LIST_SIZE, &count)))
        return out;
    for (s32 i = 0; i < count; ++i) {
        core::SaveProfile p;
        p.uid_hex = uid_hex(uids[i]);
        AccountProfile profile;
        if (R_SUCCEEDED(accountGetProfile(&profile, uids[i]))) {
            AccountProfileBase base;
            if (R_SUCCEEDED(accountProfileGet(&profile, nullptr, &base)))
                p.name = base.nickname;
            accountProfileClose(&profile);
        }
        out.push_back(std::move(p));
    }
    return out;
}

} // namespace

std::vector<core::SaveProfile> NsSaveService::profilesWithSave(std::uint64_t title_id) {
    std::vector<core::SaveProfile> out;
    accountInitialize(AccountServiceType_System);
    for (auto& p : all_profiles()) {
        AccountUid uid;
        std::sscanf(p.uid_hex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);
        if (R_SUCCEEDED(fsdevMountSaveData(kMount, title_id, uid))) {
            out.push_back(p);
            fsdevUnmountDevice(kMount);
        }
    }
    accountExit();
    return out;
}

bool NsSaveService::backup(const InstalledTitle& title, std::string* outError) {
    accountInitialize(AccountServiceType_System);

    std::string ts  = now_timestamp();
    std::string dir = core::backup_dir(core::saves_root(), title.title_id, ts);

    std::vector<std::string> savedProfiles;
    bool anyFailure = false;

    for (auto& p : all_profiles()) {
        AccountUid uid;
        std::sscanf(p.uid_hex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);
        if (R_FAILED(fsdevMountSaveData(kMount, title.title_id, uid)))
            continue; // this profile has no save for this title

        std::string dst = dir + "/" + p.uid_hex;
        make_dirs(dst);
        std::string mountRoot = std::string(kMount) + ":/";
        if (copy_tree(mountRoot, dst))
            savedProfiles.push_back(p.uid_hex);
        else
            anyFailure = true;
        fsdevUnmountDevice(kMount);
    }
    accountExit();

    if (savedProfiles.empty()) {
        clear_tree(dir); ::rmdir(dir.c_str()); // remove partial/empty
        if (outError) *outError = anyFailure ? "copy failed" : "no save data";
        return false;
    }

    core::ManifestInfo m;
    m.game_name = title.name;
    m.title_id  = title.title_id;
    m.timestamp = ts;
    m.profiles  = savedProfiles;
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

bool NsSaveService::restore(const core::BackupEntry&, std::uint64_t, std::string* outError) {
    if (outError) *outError = "not implemented";
    return false;
}

} // namespace thomaz

#endif // __SWITCH__
