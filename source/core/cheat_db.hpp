#pragma once
#include "core/cheat.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace thomaz::core {

// Parsed from versions/<TITLE_ID>.json.
struct VersionMap {
    std::map<std::uint32_t, std::string> by_version; // version u32 -> build_id (uppercase hex)
    std::optional<std::uint32_t> latest;
    std::string title;
};

// All build_ids that actually have cheats in cheats/<TITLE_ID>.json (excludes "attribution").
std::vector<std::string> build_ids_with_cheats(const std::string& cheats_json);

// Cheats for one build_id from cheats/<TITLE_ID>.json. Empty if the build_id is absent.
std::vector<Cheat> parse_db_cheats(const std::string& cheats_json, const std::string& build_id);

// Parse versions/<TITLE_ID>.json (keys "latest"/"title" are metadata, not versions).
VersionMap parse_versions(const std::string& versions_json);

// Parse the root versions.json (a map of <TITLE_ID hex> -> version info) into the
// set of title_ids the db covers. Used for the "has cheats" badge. Non-hex keys
// are ignored defensively.
std::set<std::uint64_t> parse_db_index(const std::string& index_json);

} // namespace thomaz::core
