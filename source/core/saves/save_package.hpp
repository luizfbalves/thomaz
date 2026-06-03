#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

// One file inside a save, keyed by a path relative to the save root. The first
// path segment is the profile's uid_hex (e.g. "1111.../save.dat").
struct SaveFileEntry {
    std::string               path;
    std::vector<std::uint8_t> bytes;
};

// A whole save (all profiles) as an ordered list of files. This is what gets
// serialized into the opaque blob the API stores.
struct SavePackage {
    std::vector<SaveFileEntry> files;
};

// Binary layout (little-endian): magic "TSAV", u32 fileCount, then per file:
// u32 pathLen, path bytes, u32 dataLen, data bytes.
std::vector<std::uint8_t> pack_save_package(const SavePackage& pkg);

// Returns nullopt on any malformed/truncated input.
std::optional<SavePackage> unpack_save_package(const std::vector<std::uint8_t>& blob);

} // namespace thomaz::core
