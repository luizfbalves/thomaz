#include "doctest.h"
#include "core/games/index_parse.hpp"
#include <fstream>
#include <sstream>

using namespace thomaz::core;

TEST_CASE("parse_index: object files with size and directories") {
    const char* json = R"({"files":[{"url":"a.nsp","size":100}],"directories":["d/"]})";
    ParsedIndex idx = parse_index(json);
    REQUIRE(idx.ok);
    REQUIRE(idx.files.size() == 1);
    CHECK(idx.files[0].url == "a.nsp");
    CHECK(idx.files[0].size == 100);
    REQUIRE(idx.directories.size() == 1);
    CHECK(idx.directories[0] == "d/");
}

TEST_CASE("parse_index: bare-string files default size to zero") {
    const char* json = R"({"files":["a.nsp","b.nsz"]})";
    ParsedIndex idx = parse_index(json);
    REQUIRE(idx.ok);
    REQUIRE(idx.files.size() == 2);
    CHECK(idx.files[0].size == 0);
    CHECK(idx.files[1].size == 0);
}

TEST_CASE("parse_index: missing directories key yields empty list") {
    ParsedIndex idx = parse_index(R"({"files":[]})");
    REQUIRE(idx.ok);
    CHECK(idx.directories.empty());
}

TEST_CASE("parse_index: success populates motd; error when success absent") {
    ParsedIndex ok = parse_index(R"({"files":[],"success":"hi"})");
    CHECK(ok.motd == "hi");
    ParsedIndex err = parse_index(R"({"files":[],"error":"oops"})");
    CHECK(err.motd == "oops");
}

TEST_CASE("parse_index: unknown extra keys are ignored") {
    const char* json = R"({"files":[],"titledb":"x","headers":[],"referrer":"r","version":1})";
    CHECK(parse_index(json).ok);
}

TEST_CASE("parse_index: malformed non-object body returns ok=false") {
    CHECK_FALSE(parse_index("not json").ok);
    CHECK_FALSE(parse_index("[]").ok);
}

TEST_CASE("parse_index: empty url entries are skipped") {
    const char* json = R"({"files":[{"url":""},{"url":"good.nsp"}]})";
    ParsedIndex idx = parse_index(json);
    REQUIRE(idx.ok);
    REQUIRE(idx.files.size() == 1);
    CHECK(idx.files[0].url == "good.nsp");
}

TEST_CASE("parse_index: fixture file parses successfully") {
    std::ifstream in("fixtures/tinfoil_index_sample.json");
    REQUIRE(in);
    std::ostringstream ss;
    ss << in.rdbuf();
    ParsedIndex idx = parse_index(ss.str());
    REQUIRE(idx.ok);
    CHECK(idx.files.size() == 3);
    CHECK(idx.directories.size() == 1);
    CHECK(idx.motd == "Welcome to the shop!");
}
