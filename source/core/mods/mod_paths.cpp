#include "core/mods/mod_paths.hpp"
#include "core/db_paths.hpp" // title_id_hex

namespace thomaz::core {

std::string mod_staging_root() {
#ifdef __SWITCH__
    return "/switch/thomaz/mods";
#else
    return "thomaz-mods";
#endif
}

std::string mod_staging_title_dir(std::uint64_t title_id) {
    return mod_staging_root() + "/" + title_id_hex(title_id, false);
}

std::string mod_staging_dir(std::uint64_t title_id, const std::string& mod_name) {
    return mod_staging_title_dir(title_id) + "/" + mod_name;
}

std::string sd_romfs_dir(std::uint64_t title_id) {
    return "/atmosphere/contents/" + title_id_hex(title_id, false) + "/romfs";
}

std::string active_marker_path(std::uint64_t title_id) {
    return mod_staging_title_dir(title_id) + "/.active";
}

} // namespace thomaz::core
