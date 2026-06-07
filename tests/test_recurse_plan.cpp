#include "doctest.h"
#include "core/games/recurse_plan.hpp"

using namespace thomaz::core;

TEST_CASE("may_descend: false at depth==maxDepth") {
    RecurseBounds bounds;
    RecurseState  state;
    state.depth = bounds.maxDepth;
    CHECK_FALSE(may_descend(state, bounds));
}

TEST_CASE("may_descend: false when entries or requests hit bounds") {
    RecurseBounds bounds;
    RecurseState  e;
    e.entries = bounds.maxEntries;
    CHECK_FALSE(may_descend(e, bounds));
    RecurseState r;
    r.requests = bounds.maxRequests;
    CHECK_FALSE(may_descend(r, bounds));
}

TEST_CASE("may_descend: true within all bounds") {
    RecurseBounds bounds;
    RecurseState  state;
    CHECK(may_descend(state, bounds));
}
