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
//
// background_only: apply only each part's background image, skipping the custom
// layout. The safe fallback for layouts incompatible with the console firmware
// (see theme_compat.hpp). Parts with no background are skipped.
InstallResult install_theme(const thomaz::core::ThemeDetail& detail,
                            bool background_only = false);

// Delete the LayeredFS outputs recorded as active and clear active state.
InstallResult remove_active_theme();

} // namespace thomaz
