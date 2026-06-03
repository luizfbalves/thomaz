#include "core/cheat_db.hpp"
#include "core/cheat_txt.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

std::vector<std::string> build_ids_with_cheats(const std::string& cheats_json) {
    std::vector<std::string> ids;
    json j = json::parse(cheats_json, nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return ids;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.key() == "attribution") continue;
        if (it.value().is_object()) ids.push_back(it.key());
    }
    return ids;
}

std::vector<Cheat> parse_db_cheats(const std::string& cheats_json, const std::string& build_id) {
    std::vector<Cheat> cheats;
    json j = json::parse(cheats_json, nullptr, false);
    if (!j.is_object() || !j.contains(build_id) || !j[build_id].is_object()) return cheats;
    for (auto it = j[build_id].begin(); it != j[build_id].end(); ++it) {
        if (!it.value().is_string()) continue;
        // each value is a full single-cheat .txt snippet (header + opcodes)
        auto parsed = parse_txt(it.value().get<std::string>());
        for (auto& c : parsed) cheats.push_back(std::move(c));
    }
    return cheats;
}

std::set<std::uint64_t> parse_db_index(const std::string& index_json) {
    std::set<std::uint64_t> ids;
    json j = json::parse(index_json, nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return ids;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& key = it.key();
        // Top-level keys are 16-char hex title ids.
        if (key.size() != 16) continue;
        try {
            ids.insert(std::stoull(key, nullptr, 16));
        } catch (...) {
            // not a hex title id — ignore defensively
        }
    }
    return ids;
}

VersionMap parse_versions(const std::string& versions_json) {
    VersionMap vm;
    json j = json::parse(versions_json, nullptr, false);
    if (!j.is_object()) return vm;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& key = it.key();
        if (key == "latest") {
            if (it.value().is_number_unsigned()) vm.latest = it.value().get<std::uint32_t>();
            continue;
        }
        if (key == "title") {
            if (it.value().is_string()) vm.title = it.value().get<std::string>();
            continue;
        }
        // version keys are stringified u32; value is the build_id string
        if (!it.value().is_string()) continue;
        try {
            std::uint32_t version = static_cast<std::uint32_t>(std::stoul(key));
            vm.by_version[version] = it.value().get<std::string>();
        } catch (...) {
            // non-numeric key that is not latest/title: ignore defensively
        }
    }
    return vm;
}

} // namespace thomaz::core
