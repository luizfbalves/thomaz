#pragma once
#include <string>

namespace thomaz::core {

// All builders return a ready-to-POST JSON body: {"query":...,"variables":...}.

// Themes feed/search. `query` empty => no text filter. `target` empty => no
// section filter (otherwise a Target enum value like "ResidentMenu").
std::string themes_feed_body(const std::string& query, const std::string& target,
                             int page, int limit);

// Packs feed/search. `query` empty => no text filter.
std::string packs_feed_body(const std::string& query, int page, int limit);

// Detail bodies. `hex_id` is sanitized to [0-9A-Fa-f] before being inlined.
std::string theme_detail_body(const std::string& hex_id);
std::string pack_detail_body(const std::string& hex_id);

} // namespace thomaz::core
