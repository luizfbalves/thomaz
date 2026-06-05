#pragma once
#include <cstdint>
#include <vector>

namespace thomaz {

/// Returns true if and only if the buffer is structurally valid as a .szs
/// layout file: if the buffer has Yaz0 magic it is decompressed first, then
/// the result must unpack as a non-empty SARC archive (D-04).
///
/// Returns false for:
///   - buffers shorter than 4 bytes
///   - buffers whose Yaz0 decompression or SARC unpack throws (garbage, truncated, corrupt)
///   - SARC archives with no files
///
/// Never throws to the caller for any input.
bool is_structurally_valid_szs(const std::vector<std::uint8_t>& buf);

} // namespace thomaz
