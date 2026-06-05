#include "platform/themes/theme_download.hpp"
#include "platform/themes/theme_paths.hpp"
#include "platform/mods/mod_store.hpp"     // remove_tree
#include "platform/mods/mod_download.hpp"  // download_file

#include <sys/stat.h>

namespace thomaz {

std::string nxtheme_filename(const thomaz::core::ThemePart& p, int index) {
    std::string base = p.target.empty() ? ("theme" + std::to_string(index)) : p.target;
    // targets are enum-safe ASCII already; keep simple.
    return base + ".nxtheme";
}

ThemeDownloadResult download_theme(const thomaz::core::ThemeDetail& detail,
                                   std::shared_ptr<std::atomic<bool>> cancelled) {
    ThemeDownloadResult res;
    if (detail.parts.empty()) {
        res.error = "nothing to download";
        return res;
    }

    std::string folder = theme_folder(detail.entry);
    ::mkdir(themes_root().c_str(), 0777); // ensure root exists (best-effort)
    ::mkdir(folder.c_str(), 0777);

    int index = 0;
    for (const auto& part : detail.parts) {
        std::string dest = folder + "/" + nxtheme_filename(part, index++);
        std::string err;
        if (!download_file(part.download_url, dest, nullptr, &err, cancelled)) {
            remove_tree(folder); // no half-written theme left behind
            res.error = err.empty() ? "download failed" : err;
            return res;
        }
    }
    res.ok = true;
    return res;
}

} // namespace thomaz
