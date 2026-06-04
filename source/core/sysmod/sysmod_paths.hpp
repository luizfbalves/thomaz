#pragma once
#include <string>

namespace thomaz::core {

// Root of Atmosphere's content overrides (sysmodules + LayeredFS mods).
std::string sysmod_contents_root();

// <root>/<program_id>  (program_id used verbatim, it is not a title id we own).
std::string sysmod_contents_dir(const std::string& program_id);

// <root>/<program_id>/flags
std::string sysmod_flags_dir(const std::string& program_id);

// <root>/<program_id>/flags/boot2.flag  — presence means "enabled at boot".
std::string sysmod_boot2_flag_path(const std::string& program_id);

} // namespace thomaz::core
