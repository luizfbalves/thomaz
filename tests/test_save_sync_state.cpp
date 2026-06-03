#include "doctest.h"
#include "core/saves/save_sync_state.hpp"

using namespace thomaz::core;

TEST_CASE("serialize then parse round-trips entries") {
    std::map<std::uint64_t, int> state;
    state[0x0100000000010000ULL] = 3;
    state[0x010000000E5EE000ULL] = 1;
    auto parsed = parse_sync_state(serialize_sync_state(state));
    CHECK(parsed.size() == 2);
    CHECK(synced_revision(parsed, 0x0100000000010000ULL) == 3);
    CHECK(synced_revision(parsed, 0x010000000E5EE000ULL) == 1);
}

TEST_CASE("synced_revision is 0 for an unknown title") {
    std::map<std::uint64_t, int> state;
    CHECK(synced_revision(state, 0xABCDULL) == 0);
}

TEST_CASE("malformed lines are ignored") {
    auto parsed = parse_sync_state("garbage\n0100000000010000 5\n\nonlyonefield\n");
    CHECK(parsed.size() == 1);
    CHECK(synced_revision(parsed, 0x0100000000010000ULL) == 5);
}

TEST_CASE("empty input yields empty state") {
    CHECK(parse_sync_state("").empty());
}
