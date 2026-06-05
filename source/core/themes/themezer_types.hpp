#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

enum class ThemeKind { Theme, Pack };

// One downloadable .nxtheme. A standalone theme has exactly one; a pack has one
// per section it themes.
struct ThemePart {
    std::string hex_id;
    std::string target;        // e.g. "ResidentMenu" (may be empty)
    std::string name;
    std::string download_url;  // direct .nxtheme download
};

// One card in the browse grid.
struct ThemeEntry {
    ThemeKind     kind = ThemeKind::Theme;
    std::string   hex_id;        // Themezer hexId ("A24")
    std::string   name;
    std::string   author;        // creator.username
    std::string   target;        // theme section; empty for packs
    std::string   preview_url;   // jpgThumbUrl / collagePreview.jpgThumbUrl
    std::string   download_url;  // direct download (theme: .nxtheme; pack: archive)
    std::uint64_t downloads = 0; // downloadCount
};

// One page of browse results.
struct BrowsePage {
    std::vector<ThemeEntry> entries;
    int  page = 1;
    int  page_count = 1;
    bool is_complete = true;     // page >= page_count => no more pages
};

// One image shown in the theme detail gallery. `url` is the full-resolution
// image (hero + fullscreen); `thumb_url` is the small image for the strip
// (same as `url` for single-size assets like icons/backgrounds).
struct GalleryImage {
    std::string url;
    std::string thumb_url;
    std::string label;   // "Preview", "Background", an icon name, or a pack member name
};

// Detail resolved from theme(hexId)/pack(hexId). `parts` unifies download: a
// standalone theme yields one part (itself); a pack yields its members.
struct ThemeDetail {
    ThemeEntry              entry;
    std::string             description;
    std::vector<ThemePart>  parts;
    std::vector<GalleryImage> gallery;
};

} // namespace thomaz::core
