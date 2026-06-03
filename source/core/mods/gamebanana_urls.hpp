#pragma once
#include <cstdint>
#include <string>

namespace thomaz::core {

// Percent-encode `s` per RFC 3986 (unreserved A-Za-z0-9-_.~ kept; everything
// else %XX). Used for the search query string.
std::string url_encode(const std::string& s);

// apiv11 free-text mod search. game_id==0 => global (empty _idGameRow).
std::string gb_search_url(const std::string& query, std::uint64_t game_id, int page);

// apiv11 per-mod fetch of just the file list (download URLs live here).
std::string gb_mod_files_url(std::uint64_t mod_id);

} // namespace thomaz::core
