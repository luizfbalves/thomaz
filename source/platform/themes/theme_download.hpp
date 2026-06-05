#pragma once
#include "core/themes/themezer_types.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace thomaz {

struct ThemeDownloadResult {
    bool ok = false;
    std::string error;
};

// "<target>.nxtheme" (or "theme<index>.nxtheme" when target is empty) — the
// on-SD filename used for a downloaded part. Shared with theme_install.
std::string nxtheme_filename(const thomaz::core::ThemePart& part, int index);

// Download every part of `detail` into theme_folder(detail.entry) as
// "<target or index>.nxtheme". A standalone theme writes one file; a pack writes
// one per section. On any failure the partial folder is removed.
//
// `cancelled` (optional, default null): cooperative abort flag. Pass the
// owning activity's cancelledFlag() shared_ptr; the underlying download_file
// call will abort the in-flight curl transfer as soon as the flag is set.
// Existing callers that omit this argument are unaffected (null = never aborts).
ThemeDownloadResult download_theme(const thomaz::core::ThemeDetail& detail,
                                   std::shared_ptr<std::atomic<bool>> cancelled = nullptr);

} // namespace thomaz
