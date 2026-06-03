#pragma once
#include "core/mods/mod_types.hpp"
#include <string>
#include <vector>

namespace thomaz::core {

// True if `path` is a safe relative archive entry: non-empty, not absolute,
// no ".." traversal segment. (Zip-slip guard, mirrors save_backup_io.)
bool is_safe_archive_path(const std::string& path);

// Analyse an archive's entry list. Finds the single "romfs/" root and the
// prefix that must be stripped so each file lands under "romfs/...".
InstallPlan plan_install(const std::vector<ArchiveEntry>& entries);

} // namespace thomaz::core
