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

// Root directory holding all backups. Platform-specific:
//   Switch  -> "/switch/thomaz/saves"
//   desktop -> "thomaz-saves" (relative to the working dir)
std::string saves_root();

// <root>/<titleIdLowerHex>  — all backups of one title live here.
std::string title_backups_dir(const std::string& root, std::uint64_t title_id);

// <root>/<titleIdLowerHex>/<timestamp>  — one specific backup.
std::string backup_dir(const std::string& root, std::uint64_t title_id,
                       const std::string& timestamp);

// "2026-06-03_14-20-00" -> "03/06 14:20" for display. Returns the input
// unchanged if it does not match the expected shape.
std::string format_timestamp_label(const std::string& timestamp);

// List a title's backups, newest first. Reads each subdir's manifest.json.
// Subdirs without a valid manifest are skipped. Empty if none / dir missing.
std::vector<BackupEntry> list_backups(const std::string& root, std::uint64_t title_id);

// The newest backup's "YYYY-MM-DD_HH-MM-SS", or nullopt if there are none.
std::optional<std::string> last_backup_timestamp(const std::string& root,
                                                 std::uint64_t title_id);

} // namespace thomaz::core
