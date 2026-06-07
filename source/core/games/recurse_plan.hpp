#pragma once

#include <cstddef>

namespace thomaz::core {

// Pure bound check for directories recursion. Cycle tracking by visited URL
// is the platform index fetcher's responsibility (Plan 02).

struct RecurseBounds {
    int         maxDepth    = 3;
    std::size_t maxEntries  = 50000;
    int         maxRequests = 256;
};

struct RecurseState {
    int         depth      = 0;
    std::size_t entries    = 0;
    int         requests   = 0;
    bool        truncated  = false;
};

bool may_descend(const RecurseState& state, const RecurseBounds& bounds);

} // namespace thomaz::core
