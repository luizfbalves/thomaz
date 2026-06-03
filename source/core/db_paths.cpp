#include "core/db_paths.hpp"
#include <array>

namespace thomaz::core {

std::string title_id_hex(std::uint64_t title_id, bool upper) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[i] = digits[title_id & 0xF];
        title_id >>= 4;
    }
    return out;
}

namespace {
const std::string DB_BASE =
    "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master";
}

std::string cheats_url(std::uint64_t title_id) {
    return DB_BASE + "/cheats/" + title_id_hex(title_id, true) + ".json";
}

std::string versions_url(std::uint64_t title_id) {
    return DB_BASE + "/versions/" + title_id_hex(title_id, true) + ".json";
}

std::string db_index_url() {
    return DB_BASE + "/versions.json";
}

std::string sd_cheat_path(std::uint64_t title_id, const std::string& build_id) {
    return "/atmosphere/contents/" + title_id_hex(title_id, false) +
           "/cheats/" + build_id + ".txt";
}

std::string sd_cheats_dir(std::uint64_t title_id) {
    return "/atmosphere/contents/" + title_id_hex(title_id, false) + "/cheats";
}

} // namespace thomaz::core
