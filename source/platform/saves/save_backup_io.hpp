#pragma once
#include <cstdint>
#include <string>
#include "core/saves/save_package.hpp"

namespace thomaz {

// Writes a package as a new timestamped local backup under saves_root(), with a
// manifest.json listing the profiles (first path segment of each file). Returns
// true; on failure returns false and sets *outError. Shared by both the fake
// and Switch save services so the import path is identical.
bool write_package_as_backup(std::uint64_t title_id, const std::string& game_name,
                             const core::SavePackage& pkg, std::string* outError);

} // namespace thomaz
