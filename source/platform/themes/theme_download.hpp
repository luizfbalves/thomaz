#pragma once
#include "core/themes/themezer_types.hpp"
#include <string>

namespace thomaz {

struct ThemeDownloadResult {
    bool ok = false;
    std::string error;
};

// Download every part of `detail` into theme_folder(detail.entry) as
// "<target or index>.nxtheme". A standalone theme writes one file; a pack writes
// one per section. On any failure the partial folder is removed.
ThemeDownloadResult download_theme(const thomaz::core::ThemeDetail& detail);

} // namespace thomaz
