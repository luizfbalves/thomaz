#include "platform/saves/save_backup_io.hpp"

#include <ctime>
#include <set>
#include <vector>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file

namespace thomaz {

namespace {
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

// First path segment ("aaaa/save.dat" -> "aaaa"); empty if none.
std::string first_segment(const std::string& path) {
    auto slash = path.find('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}
} // namespace

bool write_package_as_backup(std::uint64_t title_id, const std::string& game_name,
                             const core::SavePackage& pkg, std::string* outError) {
    std::string ts  = now_timestamp();
    std::string dir = core::backup_dir(core::saves_root(), title_id, ts);

    std::set<std::string> profileSet;
    for (const auto& f : pkg.files) {
        std::string body(f.bytes.begin(), f.bytes.end());
        if (!write_text_file(dir + "/" + f.path, body)) {
            if (outError) *outError = "could not write save file";
            return false;
        }
        std::string seg = first_segment(f.path);
        if (!seg.empty()) profileSet.insert(seg);
    }

    core::ManifestInfo m;
    m.game_name = game_name;
    m.title_id  = title_id;
    m.timestamp = ts;
    m.profiles.assign(profileSet.begin(), profileSet.end());
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

} // namespace thomaz
