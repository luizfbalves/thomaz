#include "core/games/recurse_plan.hpp"

namespace thomaz::core {

bool may_descend(const RecurseState& state, const RecurseBounds& bounds) {
    if (state.depth >= bounds.maxDepth)
        return false;
    if (state.entries >= bounds.maxEntries)
        return false;
    if (state.requests >= bounds.maxRequests)
        return false;
    return true;
}

} // namespace thomaz::core
