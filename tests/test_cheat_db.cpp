#include "doctest.h"
#include "core/cheat_db.hpp"

using namespace thomaz::core;

// Shape of cheats/<TITLE_ID>.json: build_id -> { "[Name]": "<full text>\n" }, plus "attribution".
static const char* CHEATS_JSON = R"({
  "B424BE150A8E7D78": {
    "[Infinite Health]": "[Infinite Health]\n11160000 5C3BE7DC 00000000\n20000000\n",
    "{Master}": "{Master}\n580F0000 0149D940\n"
  },
  "OLDBUILD00000000": {
    "[Old Cheat]": "[Old Cheat]\n04000000 0000\n"
  },
  "attribution": { "[Infinite Health]": "someuser" }
})";

TEST_CASE("build_ids_with_cheats lists build ids and excludes attribution") {
    auto ids = build_ids_with_cheats(CHEATS_JSON);
    REQUIRE(ids.size() == 2);
    // order not guaranteed; check membership
    bool has_new = false, has_old = false, has_attr = false;
    for (auto& id : ids) {
        if (id == "B424BE150A8E7D78") has_new = true;
        if (id == "OLDBUILD00000000") has_old = true;
        if (id == "attribution") has_attr = true;
    }
    CHECK(has_new);
    CHECK(has_old);
    CHECK_FALSE(has_attr);
}

TEST_CASE("parse_db_cheats returns the cheats for one build id") {
    auto cheats = parse_db_cheats(CHEATS_JSON, "B424BE150A8E7D78");
    REQUIRE(cheats.size() == 2);
    // entries parsed via the .txt grammar; find by name
    bool master_ok = false, health_ok = false;
    for (auto& c : cheats) {
        if (c.is_master && c.name == "Master") master_ok = (c.opcode_lines == std::vector<std::string>{"580F0000 0149D940"});
        if (!c.is_master && c.name == "Infinite Health") health_ok = (c.opcode_lines.size() == 2 && c.opcode_lines[1] == "20000000");
    }
    CHECK(master_ok);
    CHECK(health_ok);
}

TEST_CASE("parse_db_cheats on unknown build id is empty") {
    CHECK(parse_db_cheats(CHEATS_JSON, "DOESNOTEXIST0000").empty());
}

TEST_CASE("parse_db_index collects title ids from the root versions.json keys") {
    const char* index = R"({
      "0100000000010000": { "latest": 393216, "title": "Super Mario Odyssey" },
      "010000000E5EE000": { "latest": 0, "title": "8-BIT" },
      "not_a_title_id": { "x": 1 }
    })";
    auto ids = parse_db_index(index);
    CHECK(ids.size() == 2);
    CHECK(ids.count(0x0100000000010000ULL) == 1);
    CHECK(ids.count(0x010000000E5EE000ULL) == 1);
}

TEST_CASE("parse_db_index on garbage is empty") {
    CHECK(parse_db_index("not json").empty());
    CHECK(parse_db_index("[]").empty());
}

TEST_CASE("parse_versions reads version->build_id and metadata") {
    const char* versions = R"({
      "0": "3CA12DFAAF9C82DA",
      "262144": "B424BE150A8E7D78",
      "393216": "B424BE150A8E7D78",
      "latest": 393216,
      "title": "Super Mario Odyssey"
    })";
    VersionMap vm = parse_versions(versions);
    CHECK(vm.by_version.at(0) == "3CA12DFAAF9C82DA");
    CHECK(vm.by_version.at(393216) == "B424BE150A8E7D78");
    CHECK(vm.by_version.count(0) == 1);
    CHECK(vm.by_version.size() == 3); // "latest" and "title" excluded
    REQUIRE(vm.latest.has_value());
    CHECK(vm.latest.value() == 393216);
    CHECK(vm.title == "Super Mario Odyssey");
}
