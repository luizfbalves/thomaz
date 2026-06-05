#pragma once

// Pure helpers for formatting storage sizes and computing usage ratios.
// Uses decimal SI units (KB=1e3, MB=1e6, GB=1e9)

#include <cstdint>
#include <string>

namespace thomaz {

// Format a byte count as a human-readable string using decimal SI units.
// Examples: 0 -> "0 B", 1000 -> "1.0 KB", 1500000 -> "1.5 MB", 2000000000 -> "2.0 GB"
std::string format_bytes(std::uint64_t bytes);

// Compute the fraction of storage used: (total - free) / total.
// Returns 0.0f when total == 0 (division guard).
// Returns 1.0f when free >= total (nothing free / free > total treated as full).
// Result is clamped to [0.0f, 1.0f].
float used_ratio(std::uint64_t total_bytes, std::uint64_t free_bytes);

} // namespace thomaz
