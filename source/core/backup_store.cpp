#include "core/backup_store.hpp"

#include <nlohmann/json.hpp>

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

} // namespace thomaz::core
