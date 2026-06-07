#include "platform/games/cover_art.hpp"

#include "core/db_paths.hpp"
#include "platform/cheat_store.hpp"
#include "platform/fs_util.hpp"
#include "platform/image_transcode.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace thomaz {

namespace {

constexpr std::size_t kMaxArtBytes        = 4u << 20;
constexpr std::size_t kMaxRegionalBytes   = 96u << 20; // US.en.json (~86 MiB)
constexpr std::size_t kMaxIconIndexBytes  = 32u << 20;

std::mutex                     g_indexMu;
bool                           g_indexLoaded = false;
std::unordered_map<std::uint64_t, std::string> g_iconUrls;

std::string titledb_dir() {
#ifdef __SWITCH__
    return "/switch/thomaz/cache/titledb";
#else
    return "thomaz-cache/titledb";
#endif
}

std::string regional_cache_path() { return titledb_dir() + "/US.en.json"; }

std::string icon_index_cache_path() { return titledb_dir() + "/icon_index.json"; }

std::string art_cache_path(std::uint64_t titleId) {
#ifdef __SWITCH__
    return "/switch/thomaz/cache/art/" + thomaz::core::title_id_hex(titleId, true) + ".img";
#else
    return "thomaz-cache/art/" + thomaz::core::title_id_hex(titleId, true) + ".img";
#endif
}

std::optional<std::string> read_capped_file(const std::string& path, std::size_t maxBytes) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return std::nullopt;

    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < maxBytes)
        out.append(buf, n);

    const bool readErr = std::ferror(f) != 0;
    std::fclose(f);
    if (readErr)
        return std::nullopt;
    return out;
}

bool write_binary_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    ensure_parent_dirs(path);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const std::size_t written =
        bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
    const bool ok = (written == bytes.size());
    return (std::fclose(f) == 0) && ok;
}

std::optional<std::vector<std::uint8_t>> read_binary_file(const std::string& path) {
    auto text = read_capped_file(path, kMaxArtBytes);
    if (!text)
        return std::nullopt;
    return std::vector<std::uint8_t>(text->begin(), text->end());
}

std::uint64_t parse_title_id_hex(const std::string& hex) {
    if (hex.size() != 16)
        return 0;
    try {
        return std::stoull(hex, nullptr, 16);
    } catch (...) {
        return 0;
    }
}

void ingest_icon_index_json(const std::string& body) {
    const auto j = nlohmann::json::parse(body, nullptr, false);
    if (!j.is_object())
        return;
    for (const auto& [hex, urlVal] : j.items()) {
        if (!urlVal.is_string())
            continue;
        const std::uint64_t id = parse_title_id_hex(hex);
        if (id != 0)
            g_iconUrls[id] = urlVal.get<std::string>();
    }
}

bool build_icon_index_from_regional(const std::string& regionalBody) {
    const auto j = nlohmann::json::parse(regionalBody, nullptr, false);
    if (!j.is_object())
        return false;

    nlohmann::json compact = nlohmann::json::object();
    for (const auto& [nsuKey, entry] : j.items()) {
        (void)nsuKey;
        if (!entry.is_object() || !entry.contains("id") || !entry.contains("iconUrl"))
            continue;
        const auto id  = entry["id"].get<std::string>();
        const auto url = entry["iconUrl"].get<std::string>();
        if (!id.empty() && !url.empty())
            compact[id] = url;
    }

    const std::string dumped = compact.dump();
    write_text_file(icon_index_cache_path(), dumped);
    ingest_icon_index_json(dumped);
    return !g_iconUrls.empty();
}

bool stream_download(IHttpClient* http, const std::string& url, const std::string& dest,
                     std::shared_ptr<std::atomic<bool>> cancelled) {
    ensure_parent_dirs(dest);
    const std::string tmp = dest + ".tmp";
    std::FILE* f          = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;

    StreamRequest req;
    req.url       = url;
    req.cancelled = cancelled;
    bool fileErr  = false;
    req.sink      = [&](const std::uint8_t* data, std::size_t n) -> bool {
        if (fileErr)
            return false;
        if (std::fwrite(data, 1, n, f) != n) {
            fileErr = true;
            return false;
        }
        return true;
    };

    const StreamResult result = http->stream(req);
    std::fclose(f);
    if (!result.ok || fileErr) {
        std::remove(tmp.c_str());
        return false;
    }
    std::remove(dest.c_str());
    return std::rename(tmp.c_str(), dest.c_str()) == 0;
}

bool ensure_regional_cached(IHttpClient* http, std::shared_ptr<std::atomic<bool>> cancelled) {
    const std::string path = regional_cache_path();
    if (read_capped_file(path, 64)) // presence check (any non-empty prefix)
        return true;
    return stream_download(http, titledb_regional_url(), path, cancelled);
}

bool ensure_icon_index_loaded(IHttpClient* http, std::shared_ptr<std::atomic<bool>> cancelled) {
    std::lock_guard<std::mutex> lock(g_indexMu);
    if (g_indexLoaded)
        return !g_iconUrls.empty();

    g_indexLoaded = true;
    g_iconUrls.clear();

    if (auto cached = read_capped_file(icon_index_cache_path(), kMaxIconIndexBytes)) {
        ingest_icon_index_json(*cached);
        if (!g_iconUrls.empty())
            return true;
    }

    if (!ensure_regional_cached(http, cancelled))
        return false;

    if (auto regional = read_capped_file(regional_cache_path(), kMaxRegionalBytes))
        return build_icon_index_from_regional(*regional);

    return false;
}

std::optional<std::vector<std::uint8_t>>
fetch_icon_bytes(IHttpClient* http, const std::string& url,
                 std::shared_ptr<std::atomic<bool>> cancelled) {
    HttpRequest req;
    req.url         = url;
    req.cancelled   = cancelled;
    req.maxBodyBytes = kMaxArtBytes;
    const HttpResponse r = http->request(req);
    if (!r.ok())
        return std::nullopt;

    const std::string decoded = thomaz::platform::to_decodable_image(r.body);
    return std::vector<std::uint8_t>(decoded.begin(), decoded.end());
}

std::optional<std::vector<std::uint8_t>>
libnx_icon_for(IHttpClient* http, ITitleService* titles, std::uint64_t titleId,
               std::shared_ptr<std::atomic<bool>> cancelled) {
    (void)http;
    (void)cancelled;
    if (!titles)
        return std::nullopt;

    for (const InstalledTitle& t : titles->listInstalled()) {
        if (t.title_id == titleId && !t.icon.empty())
            return t.icon;
    }
    return std::nullopt;
}

} // namespace

const char* titledb_regional_url() {
    return "https://raw.githubusercontent.com/blawar/titledb/master/US.en.json";
}

std::optional<std::string> titledb_icon_url(std::uint64_t titleId) {
    std::lock_guard<std::mutex> lock(g_indexMu);
    const auto it = g_iconUrls.find(titleId);
    if (it == g_iconUrls.end())
        return std::nullopt;
    return it->second;
}

CoverArt resolve_cover(IHttpClient* http, ITitleService* titles, std::uint64_t titleId,
                       thomaz::core::TitleKind kind,
                       std::shared_ptr<std::atomic<bool>> cancelled) {
    (void)kind;

    if (auto cached = read_binary_file(art_cache_path(titleId))) {
        CoverArt out;
        out.bytes = std::move(*cached);
        out.tier  = ArtTier::Titledb;
        out.ok    = true;
        return out;
    }

    ensure_icon_index_loaded(http, cancelled);

    if (auto url = titledb_icon_url(titleId)) {
        if (auto bytes = fetch_icon_bytes(http, *url, cancelled)) {
            write_binary_file(art_cache_path(titleId), *bytes);
            CoverArt out;
            out.bytes = std::move(*bytes);
            out.tier  = ArtTier::Titledb;
            out.ok    = true;
            return out;
        }
    }

    if (auto icon = libnx_icon_for(http, titles, titleId, cancelled)) {
        CoverArt out;
        out.bytes = std::move(*icon);
        out.tier  = ArtTier::LibnxIcon;
        out.ok    = true;
        return out;
    }

    CoverArt out;
    out.tier = ArtTier::Placeholder;
    out.ok   = true;
    return out;
}

} // namespace thomaz
