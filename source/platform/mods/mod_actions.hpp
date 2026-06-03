#pragma once
#include "core/mods/mod_types.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace thomaz {

struct ModActionResult {
    bool ok = false;
    std::string error;
};

// Extract an archive on the SD into this title's staging area as a new mod
// named `mod_name`. Validates it is a romfs mod (plan_install) first.
ModActionResult import_archive(std::uint64_t title_id, const std::string& mod_name,
                               const std::string& archive_path,
                               const std::function<void(int, int)>& progress);

// Staged mods for a title, with `active` reflecting the marker.
std::vector<core::StagedMod> installed_mods(std::uint64_t title_id);

// Currently active mod name for a title, or empty if none.
std::string active_mod(std::uint64_t title_id);

// Activate `mod_name`: disable the currently active mod (if any), copy this
// mod's romfs into /atmosphere/contents/<tid>/romfs, update the marker.
ModActionResult enable_mod(std::uint64_t title_id, const std::string& mod_name);

// Remove this title's romfs from /atmosphere/contents and clear the marker.
ModActionResult disable_mod(std::uint64_t title_id);

// Delete a staged mod from the staging area. If it is the active mod, disable
// it first.
ModActionResult uninstall_mod(std::uint64_t title_id, const std::string& mod_name);

} // namespace thomaz
