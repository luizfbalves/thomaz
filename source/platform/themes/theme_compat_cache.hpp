#pragma once
#include <optional>
#include <string>

#include "platform/themes/theme_compat.hpp"

namespace thomaz {

// Persistent cache of theme/firmware compatibility results (sd:/themes).
//
// The compat dry-run is the slow part of opening a downloaded theme, and its
// result only depends on the theme (immutable per hexId) and the console
// firmware. So we cache it keyed by hexId and tag the whole cache with the
// firmware it was computed on — a firmware update invalidates every entry.
// Only complete (dry-run) results are worth caching; partial ones are cheap to
// recompute and may change once base layouts get extracted.

// Returns the cached result for `hex_id` iff one exists AND it was computed on
// the same firmware as `fw`. Otherwise std::nullopt (caller should analyze).
std::optional<ThemeCompat> compat_cache_get(const std::string& hex_id, FwVersion fw);

// Stores `tc` for `hex_id` under firmware `fw`. If the cache was tagged with a
// different firmware, it is reset to the new firmware first.
void compat_cache_put(const std::string& hex_id, FwVersion fw, const ThemeCompat& tc);

} // namespace thomaz
