#include "core/storage_format.hpp"

#include <cstdio>
#include <algorithm>

namespace thomaz {

std::string format_bytes(std::uint64_t bytes)
{
    char buf[32];

    if (bytes >= 1'000'000'000ULL) {
        double gb = (double)bytes / 1e9;
        std::snprintf(buf, sizeof(buf), "%.1f GB", gb);
    } else if (bytes >= 1'000'000ULL) {
        double mb = (double)bytes / 1e6;
        std::snprintf(buf, sizeof(buf), "%.1f MB", mb);
    } else if (bytes >= 1'000ULL) {
        double kb = (double)bytes / 1e3;
        std::snprintf(buf, sizeof(buf), "%.1f KB", kb);
    } else {
        std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    }

    return std::string(buf);
}

float used_ratio(std::uint64_t total_bytes, std::uint64_t free_bytes)
{
    if (total_bytes == 0)
        return 0.0f;

    // free > total means somehow more free than total — treat as full (all used)
    if (free_bytes >= total_bytes)
        return (free_bytes > total_bytes) ? 1.0f : 0.0f;

    float ratio = (float)(total_bytes - free_bytes) / (float)total_bytes;
    // Clamp to [0.0f, 1.0f] for safety
    return std::min(1.0f, std::max(0.0f, ratio));
}

} // namespace thomaz
