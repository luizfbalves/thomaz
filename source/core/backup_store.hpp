#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

// A local console user profile that has save data.
struct SaveProfile {
    std::uint64_t uid = 0;   // AccountUid low/high folded to a 16-hex string elsewhere
    std::string uid_hex;     // 32-hex string form, used as the on-SD folder name
    std::string name;        // friendly nickname (display only)
};

// Contents of a backup's manifest.json (and the data needed to write one).
struct ManifestInfo {
    std::string game_name;
    std::uint64_t title_id = 0;
    std::string timestamp;             // "YYYY-MM-DD_HH-MM-SS"
    std::vector<std::string> profiles; // profile uid_hex strings included in the backup
};

// One backup on disk for a title.
struct BackupEntry {
    std::string path;                  // absolute dir of this backup
    std::string timestamp;             // "YYYY-MM-DD_HH-MM-SS"
    std::vector<std::string> profiles; // uid_hex strings present (from manifest)
};

// Serialize a manifest to a JSON string.
std::string build_manifest(const ManifestInfo& info);

// Parse a manifest.json body; nullopt if malformed.
std::optional<ManifestInfo> parse_manifest(const std::string& body);

} // namespace thomaz::core
