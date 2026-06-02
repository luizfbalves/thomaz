#pragma once
#include <cstdint>
#include <string>

namespace thomaz::core {

// 16-char hex of a Switch title id. upper=false -> lowercase (SD path form),
// upper=true -> uppercase (switch-cheats-db filename form).
std::string title_id_hex(std::uint64_t title_id, bool upper);

// switch-cheats-db raw URLs (per-title JSON).
std::string cheats_url(std::uint64_t title_id);    // .../cheats/<UPPER>.json
std::string versions_url(std::uint64_t title_id);  // .../versions/<UPPER>.json

// On-SD Atmosphère cheat file path for a resolved build id.
// build_id is used verbatim (already uppercase hex from the db).
std::string sd_cheat_path(std::uint64_t title_id, const std::string& build_id);

} // namespace thomaz::core
