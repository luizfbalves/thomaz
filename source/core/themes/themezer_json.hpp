#pragma once
#include "core/themes/themezer_types.hpp"
#include <string>

namespace thomaz::core {

// Parse a themes/packs feed response. `kind` selects which list to read
// (data.switch.themes vs data.switch.packs) and stamps each entry's kind.
// Returns an empty page (page_count 1, is_complete true) on malformed input.
BrowsePage parse_browse_page(const std::string& body, ThemeKind kind);

// Parse a theme(hexId) detail response. `found` is set false when the node is
// null/absent (theme doesn't exist). A standalone theme yields one part.
ThemeDetail parse_theme_detail(const std::string& body, bool* found);

// Parse a pack(hexId) detail response. parts = the pack's member themes.
ThemeDetail parse_pack_detail(const std::string& body, bool* found);

} // namespace thomaz::core
