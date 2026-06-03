#include "doctest.h"
#include "core/build_id.hpp"

using namespace thomaz::core;

static VersionMap mario() {
    VersionMap vm;
    vm.by_version = {{262144, "B424BE150A8E7D78"}, {393216, "B424BE150A8E7D78"}};
    vm.latest = 393216;
    return vm;
}

// Smash: latest version 1966080 -> 3EAE..., which has NO cheats (verified in spike).
// Older 1769472 -> B9B1... DOES have cheats.
static VersionMap smash() {
    VersionMap vm;
    vm.by_version = {{1769472, "B9B166DF1DB90BAF"}, {1966080, "3EAE0063B12FD81E"}};
    vm.latest = 1966080;
    return vm;
}

TEST_CASE("exact version maps to a build_id that has cheats") {
    auto r = resolve_build_id(393216, mario(), {"B424BE150A8E7D78"});
    CHECK(r.source == Resolution::Source::ExactVersion);
    CHECK(r.build_id == "B424BE150A8E7D78");
}

TEST_CASE("coverage lag: latest build_id has no cheats -> newest build_id that does") {
    // 3EAE... not in available; B9B1... is.
    auto r = resolve_build_id(1966080, smash(), {"B9B166DF1DB90BAF"});
    CHECK(r.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.build_id == "B9B166DF1DB90BAF");
}

TEST_CASE("unknown version -> newest mapped build_id that has cheats") {
    auto r = resolve_build_id(999999 /*not in map*/, smash(), {"B9B166DF1DB90BAF"});
    CHECK(r.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.build_id == "B9B166DF1DB90BAF");
}

TEST_CASE("nothing mapped has cheats -> NotInDb") {
    auto r = resolve_build_id(1966080, smash(), {/*empty: no cheats anywhere*/});
    CHECK(r.source == Resolution::Source::NotInDb);
    CHECK(r.build_id.empty());
}

TEST_CASE("fallback picks the HIGHEST version whose build_id has cheats") {
    VersionMap vm;
    vm.by_version = {{100, "AAA"}, {300, "CCC"}, {200, "BBB"}};
    // only AAA and BBB have cheats; requested version 999 unknown -> pick BBB (version 200 > 100)
    auto r = resolve_build_id(999, vm, {"AAA", "BBB"});
    CHECK(r.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.build_id == "BBB");
}
