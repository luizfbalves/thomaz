#pragma once
#include <string>
#include <vector>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

struct InstallResult {
    bool ok = false;
    std::string error;                  // human message when !ok
    std::vector<std::string> warnings;  // engine-dropped incompatible parts
};

// True if every applet target this detail needs has a base layout on the SD.
bool base_layouts_available(const thomaz::core::ThemeDetail& detail);

// Apply every part of `detail`: read its .nxtheme + base szs, patch, write to
// LayeredFS, write fsmitm.flag markers, and record the active theme. Rolls back
// files written in this call on any hard failure.
InstallResult install_theme(const thomaz::core::ThemeDetail& detail);

// Delete the LayeredFS outputs recorded as active and clear active state.
InstallResult remove_active_theme();

} // namespace thomaz
