#include "core/backup_store.hpp"
#include "core/db_paths.hpp"
#include "platform/cheat_store.hpp"   // read_text_file
#include "platform/mods/mod_store.hpp" // remove_tree

#include <nlohmann/json.hpp>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

using nlohmann::json;

namespace thomaz::core {

std::string build_manifest(const ManifestInfo& info) {
    json j;
    j["game_name"] = info.game_name;
    j["title_id"]  = info.title_id;
    j["timestamp"] = info.timestamp;
    j["profiles"]  = info.profiles;
    return j.dump(2);
}

std::optional<ManifestInfo> parse_manifest(const std::string& body) {
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object())
        return std::nullopt;
    // Required: title_id (unsigned) + timestamp (string). game_name/profiles are optional.
    if (!j.contains("timestamp") || !j["timestamp"].is_string())
        return std::nullopt;
    if (!j.contains("title_id") || !j["title_id"].is_number_unsigned())
        return std::nullopt;

    ManifestInfo info;
    info.game_name = j.value("game_name", std::string{});
    info.title_id  = j.value("title_id", std::uint64_t{0});
    info.timestamp = j.value("timestamp", std::string{});
    if (j.contains("profiles") && j["profiles"].is_array())
        for (const auto& p : j["profiles"])
            if (p.is_string())
                info.profiles.push_back(p.get<std::string>());
    return info;
}

std::string saves_root() {
#ifdef __SWITCH__
    return "/switch/thomaz/saves";
#else
    return "thomaz-saves";
#endif
}

std::string title_backups_dir(const std::string& root, std::uint64_t title_id) {
    return root + "/" + title_id_hex(title_id, /*upper=*/false);
}

std::string backup_dir(const std::string& root, std::uint64_t title_id,
                       const std::string& timestamp) {
    return title_backups_dir(root, title_id) + "/" + timestamp;
}

std::string format_timestamp_label(const std::string& ts) {
    // Expect "YYYY-MM-DD_HH-MM-SS" (19 chars).
    if (ts.size() != 19 || ts[4] != '-' || ts[7] != '-' || ts[10] != '_' ||
        ts[13] != '-' || ts[16] != '-')
        return ts;
    // "DD/MM HH:MM"
    return ts.substr(8, 2) + "/" + ts.substr(5, 2) + " " +
           ts.substr(11, 2) + ":" + ts.substr(14, 2);
}

std::vector<BackupEntry> list_backups(const std::string& root, std::uint64_t title_id) {
    std::vector<BackupEntry> out;
    std::string dir = title_backups_dir(root, title_id);

    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return out;

    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string sub = dir + "/" + name;
        struct stat st;
        if (::stat(sub.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        auto body = thomaz::read_text_file(sub + "/manifest.json");
        if (!body)
            continue;
        auto info = parse_manifest(*body);
        if (!info)
            continue;
        BackupEntry entry;
        entry.path      = sub;
        entry.timestamp = info->timestamp;
        entry.profiles  = info->profiles;
        out.push_back(std::move(entry));
    }
    ::closedir(d);

    // Newest first. Timestamps are zero-padded "YYYY-MM-DD_HH-MM-SS",
    // so lexical descending == chronological descending.
    std::sort(out.begin(), out.end(),
              [](const BackupEntry& a, const BackupEntry& b) {
                  return a.timestamp > b.timestamp;
              });
    return out;
}

std::optional<std::string> last_backup_timestamp(const std::string& root,
                                                 std::uint64_t title_id) {
    auto list = list_backups(root, title_id);
    if (list.empty())
        return std::nullopt;
    return list.front().timestamp;
}

bool delete_backup(const BackupEntry& entry) {
    if (entry.path.empty())
        return false;
    return thomaz::remove_tree(entry.path);
}

} // namespace thomaz::core
