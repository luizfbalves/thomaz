#include "core/themes/themezer_json.hpp"
#include <nlohmann/json.hpp>
#include <utility>

namespace thomaz::core {

using nlohmann::json;

namespace {
const json& switch_node(const json& doc, const char* key) {
    static const json kNull;
    if (!doc.is_object() || !doc.contains("data")) return kNull;
    const json& data = doc["data"];
    if (!data.is_object() || !data.contains("switch")) return kNull;
    const json& sw = data["switch"];
    if (!sw.is_object() || !sw.contains(key)) return kNull;
    return sw[key];
}

std::string author_of(const json& node) {
    if (node.contains("creator") && node["creator"].is_object())
        return node["creator"].value("username", std::string());
    return std::string();
}

std::string preview_of(const json& node) {
    for (const char* k : {"screenshotPreview", "collagePreview"}) {
        if (node.contains(k) && node[k].is_object())
            return node[k].value("jpgThumbUrl", std::string());
    }
    return std::string();
}

// nlohmann's value() throws if a present field is null; the Themezer API returns
// null for icons a theme doesn't override, so read strings defensively.
std::string str_field(const json& o, const char* key) {
    if (!o.is_object() || !o.contains(key)) return std::string();
    const json& v = o[key];
    return v.is_string() ? v.get<std::string>() : std::string();
}

struct ImgUrls { std::string hd; std::string thumb; };

// Reads an ImageSizes node; falls back across fields so we always get a usable
// url pair (hd for hero/fullscreen, thumb for the strip).
ImgUrls image_sizes_of(const json& node) {
    ImgUrls u;
    u.hd    = str_field(node, "hdUrl");
    u.thumb = str_field(node, "thumbUrl");
    if (u.hd.empty())    u.hd    = str_field(node, "jpgThumbUrl");
    if (u.thumb.empty()) u.thumb = u.hd;
    if (u.hd.empty())    u.hd    = u.thumb;
    return u;
}

// (display label, assets-node json key) in strip order. Background first.
const std::pair<const char*, const char*> kAssetImages[] = {
    {"Background",  "backgroundImageUrl"},
    {"Album",       "albumIconUrl"},
    {"Home",        "homeIconUrl"},
    {"News",        "newsIconUrl"},
    {"Shop",        "shopIconUrl"},
    {"Controllers", "controllerIconUrl"},
    {"Settings",    "settingsIconUrl"},
    {"Power",       "powerIconUrl"},
    {"Online",      "nsoIconUrl"},
    {"Game Card",   "cardIconUrl"},
    {"Share",       "shareIconUrl"},
};

// Theme gallery = rendered preview, then each non-null asset image.
void build_theme_gallery(const json& node, ThemeDetail& d) {
    if (node.contains("screenshotPreview") && node["screenshotPreview"].is_object()) {
        ImgUrls p = image_sizes_of(node["screenshotPreview"]);
        if (!p.hd.empty()) d.gallery.push_back({ p.hd, p.thumb, "Preview" });
    }
    if (node.contains("assets") && node["assets"].is_object()) {
        const json& a = node["assets"];
        for (const auto& asset : kAssetImages) {
            std::string url = str_field(a, asset.second);
            if (!url.empty()) d.gallery.push_back({ url, url, asset.first });
        }
    }
}

ThemeEntry entry_of(const json& node, ThemeKind kind) {
    ThemeEntry e;
    e.kind         = kind;
    e.hex_id       = node.value("hexId", std::string());
    e.name         = node.value("name", std::string());
    e.author       = author_of(node);
    e.target       = node.value("target", std::string());
    e.preview_url  = preview_of(node);
    e.download_url = node.value("downloadUrl", std::string());
    e.downloads    = node.value("downloadCount", (std::uint64_t)0);
    return e;
}
} // namespace

BrowsePage parse_browse_page(const std::string& body, ThemeKind kind) {
    BrowsePage page;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) return page;

    const json& list = switch_node(doc, kind == ThemeKind::Theme ? "themes" : "packs");
    if (!list.is_object()) return page;

    if (list.contains("pageInfo") && list["pageInfo"].is_object()) {
        page.page       = list["pageInfo"].value("page", 1);
        page.page_count = list["pageInfo"].value("pageCount", 1);
    }
    page.is_complete = page.page >= page.page_count;

    if (list.contains("nodes") && list["nodes"].is_array()) {
        for (const json& n : list["nodes"]) {
            if (!n.is_object()) continue;
            try { page.entries.push_back(entry_of(n, kind)); }
            catch (const json::exception&) { continue; }
        }
    }
    return page;
}

ThemeDetail parse_theme_detail(const std::string& body, bool* found) {
    if (found) *found = false;
    ThemeDetail d;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) return d;

    const json& node = switch_node(doc, "theme");
    if (!node.is_object()) return d;

    d.entry       = entry_of(node, ThemeKind::Theme);
    d.description = node.value("description", std::string());
    ThemePart self;
    self.hex_id       = d.entry.hex_id;
    self.target       = d.entry.target;
    self.name         = d.entry.name;
    self.download_url = d.entry.download_url;
    d.parts.push_back(self);
    build_theme_gallery(node, d);
    if (found) *found = true;
    return d;
}

ThemeDetail parse_pack_detail(const std::string& body, bool* found) {
    if (found) *found = false;
    ThemeDetail d;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded()) return d;

    const json& node = switch_node(doc, "pack");
    if (!node.is_object()) return d;

    d.entry       = entry_of(node, ThemeKind::Pack);
    d.description = node.value("description", std::string());
    if (node.contains("themes") && node["themes"].is_array()) {
        for (const json& t : node["themes"]) {
            if (!t.is_object()) continue;
            ThemePart p;
            p.hex_id       = t.value("hexId", std::string());
            p.target       = t.value("target", std::string());
            p.name         = t.value("name", std::string());
            p.download_url = t.value("downloadUrl", std::string());
            d.parts.push_back(p);
            if (t.contains("screenshotPreview") && t["screenshotPreview"].is_object()) {
                ImgUrls img = image_sizes_of(t["screenshotPreview"]);
                if (!img.hd.empty()) {
                    std::string label = p.name.empty() ? p.target : p.name;
                    d.gallery.push_back({ img.hd, img.thumb, label });
                }
            }
        }
    }
    if (found) *found = true;
    return d;
}

} // namespace thomaz::core
