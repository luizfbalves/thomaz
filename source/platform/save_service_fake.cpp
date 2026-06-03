#include "platform/save_service_fake.hpp"

#ifndef __SWITCH__

#include <ctime>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file
#include "core/saves/save_package.hpp"
#include "platform/saves/save_backup_io.hpp"

namespace thomaz {

namespace {
// "YYYY-MM-DD_HH-MM-SS" from local time.
std::string now_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}
} // namespace

std::vector<core::SaveProfile> FakeSaveService::profilesWithSave(std::uint64_t) {
    return {{1, "11111111111111111111111111111111", "Player One"}};
}

bool FakeSaveService::backup(const InstalledTitle& title, std::string* outError) {
    std::string root = core::saves_root();
    std::string ts   = now_timestamp();
    std::string dir  = core::backup_dir(root, title.title_id, ts);

    // One dummy profile folder + a dummy save file.
    std::string profileHex = "11111111111111111111111111111111";
    if (!write_text_file(dir + "/" + profileHex + "/save.dat", "fake save bytes")) {
        if (outError) *outError = "could not write to SD";
        return false;
    }

    core::ManifestInfo m;
    m.game_name = title.name;
    m.title_id  = title.title_id;
    m.timestamp = ts;
    m.profiles  = {profileHex};
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

bool FakeSaveService::restore(const core::BackupEntry& entry, std::uint64_t,
                              std::string* outError) {
    // Nothing real to write back on desktop; succeed if the backup exists.
    if (!read_text_file(entry.path + "/manifest.json")) {
        if (outError) *outError = "backup missing manifest";
        return false;
    }
    return true;
}

std::vector<std::uint8_t> FakeSaveService::packageActiveSave(std::uint64_t,
                                                             std::string* outError) {
    // Desktop stand-in: one dummy profile with one dummy file, matching what
    // backup() writes, so the cloud round-trip is exercisable without a console.
    core::SavePackage pkg;
    std::string dummy = "fake save bytes";
    pkg.files.push_back({ "11111111111111111111111111111111/save.dat",
                          std::vector<std::uint8_t>(dummy.begin(), dummy.end()) });
    (void)outError;
    return core::pack_save_package(pkg);
}

bool FakeSaveService::importPackageAsBackup(std::uint64_t title_id,
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

#endif // !__SWITCH__
