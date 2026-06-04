#include "core/sysmod/sysmod_paths.hpp"

namespace thomaz::core {

std::string sysmod_contents_root() {
    return "/atmosphere/contents";
}

std::string sysmod_contents_dir(const std::string& program_id) {
    return sysmod_contents_root() + "/" + program_id;
}

std::string sysmod_flags_dir(const std::string& program_id) {
    return sysmod_contents_dir(program_id) + "/flags";
}

std::string sysmod_boot2_flag_path(const std::string& program_id) {
    return sysmod_flags_dir(program_id) + "/boot2.flag";
}

} // namespace thomaz::core
