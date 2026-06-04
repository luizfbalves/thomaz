#include "doctest.h"
#include "core/sysmod/toolbox_json.hpp"

using namespace thomaz::core;

TEST_CASE("parses name and requires_reboot from a well-formed toolbox") {
    ToolboxInfo t = parse_toolbox(R"({"name":"sys-clk","requires_reboot":true})");
    CHECK(t.valid);
    CHECK(t.name == "sys-clk");
    CHECK(t.requires_reboot == true);
}

TEST_CASE("requires_reboot defaults to true when the field is absent") {
    ToolboxInfo t = parse_toolbox(R"({"name":"MissionControl"})");
    CHECK(t.valid);
    CHECK(t.name == "MissionControl");
    CHECK(t.requires_reboot == true);
}

TEST_CASE("respects requires_reboot=false") {
    ToolboxInfo t = parse_toolbox(R"({"name":"x","requires_reboot":false})");
    CHECK(t.requires_reboot == false);
}

TEST_CASE("empty / malformed / non-object input is not valid") {
    CHECK(parse_toolbox("").valid == false);
    CHECK(parse_toolbox("not json").valid == false);
    CHECK(parse_toolbox("[1,2,3]").valid == false);
}

TEST_CASE("wrong-typed fields fall back to defaults but stay valid") {
    ToolboxInfo t = parse_toolbox(R"({"name":123,"requires_reboot":"yes"})");
    CHECK(t.valid);
    CHECK(t.name == "");
    CHECK(t.requires_reboot == true);
}
