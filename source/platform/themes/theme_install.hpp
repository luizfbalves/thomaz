#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

struct InstallResult {
    bool ok = false;
    bool cancelled = false;             // user aborted via the cancel flag
    std::string error;                  // human message when !ok
    std::vector<std::string> warnings;  // engine-dropped incompatible parts
};

// Called on the worker thread after each part is applied: (done, total). Use it
// to drive a progress bar (marshal to the UI thread yourself).
using InstallProgress = std::function<void(int done, int total)>;

// True if every applet target this detail needs has a base layout on the SD.
bool base_layouts_available(const thomaz::core::ThemeDetail& detail);

// Apply every part of `detail`: read its .nxtheme + base szs, patch, write to
// LayeredFS, write fsmitm.flag markers, and record the active theme. Rolls back
// files written in this call on any hard failure.
//
// background_only: apply only each part's background image, skipping the custom
// layout. The safe fallback for layouts incompatible with the console firmware
// (see theme_compat.hpp). Parts with no background are skipped.
//
// on_progress: optional per-part progress callback (worker thread).
// cancelled: optional cooperative abort flag, checked between parts; when set,
// every file written in this call is rolled back and res.cancelled is returned.
InstallResult install_theme(const thomaz::core::ThemeDetail& detail,
                            bool background_only = false,
                            const InstallProgress& on_progress = {},
                            std::shared_ptr<std::atomic<bool>> cancelled = nullptr);

// Delete the LayeredFS outputs recorded as active and clear active state.
InstallResult remove_active_theme();

} // namespace thomaz
