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
#include "core/saves/save_package.hpp"
#include "platform/saves/save_backup_io.hpp"
#include "platform/fs_util.hpp"

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

// Recursively read every file under `src` into the package, prefixing each
// path with `prefix` (the profile's uid_hex). Returns false on any read error.
bool read_tree(const std::string& src, const std::string& prefix,
               core::SavePackage& pkg) {
    DIR* d = ::opendir(src.c_str());
    if (!d) return false;
    bool ok = true;
    struct dirent* e;
    while (ok && (e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string s = src + "/" + name;
        std::string rel = prefix.empty() ? name : prefix + "/" + name;
        struct stat st;
        if (::stat(s.c_str(), &st) != 0) { ok = false; break; }
        if (S_ISDIR(st.st_mode)) {
            ok = read_tree(s, rel, pkg);
        } else {
            FILE* in = std::fopen(s.c_str(), "rb");
            if (!in) { ok = false; break; }
            std::vector<std::uint8_t> bytes;
            char buf[8192];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0)
                bytes.insert(bytes.end(), buf, buf + n);
            bool readErr = std::ferror(in) != 0;
            std::fclose(in);
            if (readErr) { ok = false; break; }
            pkg.files.push_back({ rel, std::move(bytes) });
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
        std::string mountRoot = std::string(kMount) + ":/";
        if (copy_tree(mountRoot, dst, nullptr)) // copy_tree creates dst if missing
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
        clear_tree(dir); ::rmdir(dir.c_str()); // don't leave a manifest-less backup on SD
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

bool NsSaveService::restore(const core::BackupEntry& entry, std::uint64_t title_id,
                            std::string* outError) {
    accountInitialize(AccountServiceType_System);

    std::vector<std::string> done;
    std::vector<std::string> skipped;

    for (const std::string& profileHex : entry.profiles) {
        AccountUid uid;
        std::sscanf(profileHex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);

        // Mounting only succeeds for a profile that still exists and has a save
        // slot for this title. If not, skip it and report.
        if (R_FAILED(fsdevMountSaveData(kMount, title_id, uid))) {
            skipped.push_back(profileHex);
            continue;
        }

        std::string mountRoot = std::string(kMount) + ":/";
        std::string src       = entry.path + "/" + profileHex;

        clear_tree(mountRoot);                      // wipe current save contents
        bool ok = copy_tree(src, mountRoot, nullptr); // write backup files back in
        if (ok && R_SUCCEEDED(fsdevCommitDevice(kMount))) // commit, or it is discarded
            done.push_back(profileHex);
        else
            skipped.push_back(profileHex);

        fsdevUnmountDevice(kMount);
    }
    accountExit();

    if (done.empty()) {
        if (outError) *outError = "restore failed (no profiles restored)";
        return false;
    }
    if (!skipped.empty() && outError)
        *outError = "restored " + std::to_string(done.size()) +
                    " profile(s); skipped " + std::to_string(skipped.size());
    return true;
}

std::vector<std::uint8_t> NsSaveService::packageActiveSave(std::uint64_t title_id,
                                                           std::string* outError) {
    accountInitialize(AccountServiceType_System);
    core::SavePackage pkg;
    bool any        = false;
    bool readFailed = false;
    for (auto& p : all_profiles()) {
        AccountUid uid;
        std::sscanf(p.uid_hex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);
        if (R_FAILED(fsdevMountSaveData(kMount, title_id, uid)))
            continue; // no save for this profile
        std::string mountRoot = std::string(kMount) + ":/";
        core::SavePackage tmp;
        bool ok = read_tree(mountRoot, p.uid_hex, tmp);
        fsdevUnmountDevice(kMount);
        if (!ok) { readFailed = true; break; } // never upload a partially-read save
        for (auto& f : tmp.files)
            pkg.files.push_back(std::move(f));
        any = true;
    }
    accountExit();
    if (readFailed) {
        if (outError) *outError = "failed to read save";
        return {};
    }
    if (!any) {
        if (outError) *outError = "no save data";
        return {};
    }
    return core::pack_save_package(pkg);
}

bool NsSaveService::importPackageAsBackup(std::uint64_t title_id,
                                          const std::vector<std::uint8_t>& blob,
                                          std::string* outError) {
    auto pkg = core::unpack_save_package(blob);
    if (!pkg) {
        if (outError) *outError = "corrupted cloud save";
        return false;
    }
    return write_package_as_backup(title_id, "", *pkg, outError);
}

} // namespace thomaz

#endif // __SWITCH__
