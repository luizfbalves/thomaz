#include "platform/games/catalog_cache.hpp"

#include "platform/cheat_store.hpp"
#include "platform/fs_util.hpp"

#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace thomaz {

namespace {

constexpr std::size_t kMaxIndexBytes = 8u << 20;

// FNV-1a 64-bit — stable across host/Switch builds (unlike std::hash).
std::uint64_t fnv1a64(const std::string& s) {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

std::string hash_hex(const std::string& url) {
    const std::uint64_t h = fnv1a64(url);
    std::ostringstream os;
    os << std::hex << std::setfill('0') << std::setw(16) << h;
    return os.str();
}

std::optional<std::string> read_index_file(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return std::nullopt;

    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0 && out.size() < kMaxIndexBytes)
        out.append(buf, n);

    const bool readErr = std::ferror(f) != 0;
    std::fclose(f);
    if (readErr)
        return std::nullopt;
    return out;
}

} // namespace

std::string cache_path(const thomaz::core::SourceConfig& cfg) {
#ifdef __SWITCH__
    return "/switch/thomaz/cache/catalog/" + hash_hex(cfg.url) + ".json";
#else
    return "thomaz-cache/catalog/" + hash_hex(cfg.url) + ".json";
#endif
}

std::optional<std::string> read_cached_index(const thomaz::core::SourceConfig& cfg) {
    return read_index_file(cache_path(cfg));
}

bool write_cached_index(const thomaz::core::SourceConfig& cfg, const std::string& body) {
    const std::string path = cache_path(cfg);
    ensure_parent_dirs(path);
    return write_text_file(path, body);
}

} // namespace thomaz
