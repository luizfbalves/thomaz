#pragma once
#include <optional>
#include <string>
#include <vector>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

// The theme currently applied to the console (as recorded by us).
struct ActiveTheme {
    std::string hex_id;
    std::string name;
    std::string author;
    std::vector<std::string> targets;  // applet targets we wrote, e.g. {"ResidentMenu"}
};

// Read themes_root()/.thomaz_active.json; nullopt if absent/malformed.
std::optional<ActiveTheme> get_active_theme();

// Write/overwrite the active-theme record.
void set_active_theme(const ActiveTheme& t);

// Remove the record (after a theme is removed).
void clear_active_theme();

// True if `entry` is the currently-applied theme (hex_id match).
bool is_active_theme(const thomaz::core::ThemeEntry& entry);

} // namespace thomaz
