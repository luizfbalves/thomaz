#pragma once
#include <cstdint>
#include <string>

namespace thomaz::core {

// Root of the per-title mod staging area. SD on Switch, working-dir on desktop.
std::string mod_staging_root();

// <staging root>/<lower title id>
std::string mod_staging_title_dir(std::uint64_t title_id);

// <staging root>/<lower title id>/<mod_name>
std::string mod_staging_dir(std::uint64_t title_id, const std::string& mod_name);

// Atmosphere LayeredFS romfs target for a title (no trailing slash).
std::string sd_romfs_dir(std::uint64_t title_id);

// File that records the currently active mod name for a title.
std::string active_marker_path(std::uint64_t title_id);

} // namespace thomaz::core
