#include "core/saves/save_package.hpp"

namespace thomaz::core {

namespace {
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back((std::uint8_t)(v & 0xFF));
    out.push_back((std::uint8_t)((v >> 8) & 0xFF));
    out.push_back((std::uint8_t)((v >> 16) & 0xFF));
    out.push_back((std::uint8_t)((v >> 24) & 0xFF));
}

// Reads a u32 at `pos`, advancing it. Returns false if out of bounds.
bool read_u32(const std::vector<std::uint8_t>& b, size_t& pos, std::uint32_t& out) {
    if (pos + 4 > b.size()) return false;
    out = (std::uint32_t)b[pos] | ((std::uint32_t)b[pos + 1] << 8) |
          ((std::uint32_t)b[pos + 2] << 16) | ((std::uint32_t)b[pos + 3] << 24);
    pos += 4;
    return true;
}
} // namespace

std::vector<std::uint8_t> pack_save_package(const SavePackage& pkg) {
    std::vector<std::uint8_t> out = { 'T', 'S', 'A', 'V' };
    put_u32(out, (std::uint32_t)pkg.files.size());
    for (const auto& f : pkg.files) {
        put_u32(out, (std::uint32_t)f.path.size());
        out.insert(out.end(), f.path.begin(), f.path.end());
        put_u32(out, (std::uint32_t)f.bytes.size());
        out.insert(out.end(), f.bytes.begin(), f.bytes.end());
    }
    return out;
}

std::optional<SavePackage> unpack_save_package(const std::vector<std::uint8_t>& blob) {
    if (blob.size() < 8) return std::nullopt;
    if (!(blob[0] == 'T' && blob[1] == 'S' && blob[2] == 'A' && blob[3] == 'V'))
        return std::nullopt;

    size_t pos = 4;
    std::uint32_t count = 0;
    if (!read_u32(blob, pos, count)) return std::nullopt;

    SavePackage pkg;
    pkg.files.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t pathLen = 0;
        if (!read_u32(blob, pos, pathLen)) return std::nullopt;
        if (pos + pathLen > blob.size()) return std::nullopt;
        SaveFileEntry e;
        e.path.assign((const char*)&blob[pos], pathLen);
        pos += pathLen;

        std::uint32_t dataLen = 0;
        if (!read_u32(blob, pos, dataLen)) return std::nullopt;
        if (pos + dataLen > blob.size()) return std::nullopt;
        e.bytes.assign(blob.begin() + pos, blob.begin() + pos + dataLen);
        pos += dataLen;

        pkg.files.push_back(std::move(e));
    }
    return pkg;
}

} // namespace thomaz::core
