#pragma once
#include <cstdint>
#include <string>
#include "core/saves/save_package.hpp"

namespace thomaz {

// True if `path` is a safe relative path for extraction (no absolute path, no
// ".." traversal segment, non-empty). Exposed for testing; used by
// write_package_as_backup to reject malicious blob paths.
bool is_safe_relpath(const std::string& path);

// Writes a package as a new timestamped local backup under saves_root(), with a
// manifest.json listing the profiles (first path segment of each file). Returns
// true; on failure returns false and sets *outError. Shared by both the fake
// and Switch save services so the import path is identical.
bool write_package_as_backup(std::uint64_t title_id, const std::string& game_name,
                             const core::SavePackage& pkg, std::string* outError);

} // namespace thomaz
