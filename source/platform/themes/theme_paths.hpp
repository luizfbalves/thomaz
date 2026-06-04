#pragma once
#include "core/themes/themezer_types.hpp"
#include <string>

namespace thomaz {

// Root holding downloaded themes. Switch -> "/themes" (NXThemes Installer reads
// sd:/themes/); desktop -> "themes" (working dir).
std::string themes_root();

// <root>/<sanitized "Author - Name">  — one folder per theme/pack download.
std::string theme_folder(const thomaz::core::ThemeEntry& entry);

// True if theme_folder(entry) already exists (drives the "downloaded" badge).
bool theme_already_downloaded(const thomaz::core::ThemeEntry& entry);

} // namespace thomaz
