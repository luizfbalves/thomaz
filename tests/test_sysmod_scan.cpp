#include "doctest.h"
#include "core/sysmod/sysmod_scan.hpp"

using namespace thomaz::core;

TEST_CASE("a folder with exefs becomes a sysmodule; enabled reflects the flag") {
    std::vector<RawSysmoduleEntry> raw = {
        {"00FF0000636C6BFF", /*exefs*/ true, /*flag*/ true,
         R"({"name":"sys-clk","requires_reboot":true})"},
    };
    std::vector<Sysmodule> out = build_sysmodule_list(raw);
    REQUIRE(out.size() == 1);
    CHECK(out[0].program_id == "00FF0000636C6BFF");
    CHECK(out[0].name == "sys-clk");
    CHECK(out[0].enabled == true);
    CHECK(out[0].requires_reboot == true);
    CHECK(out[0].has_metadata == true);
}

TEST_CASE("a romfs-only mod folder (no exefs) is skipped") {
    std::vector<RawSysmoduleEntry> raw = {
        {"0100000000010000", /*exefs*/ false, /*flag*/ false, ""},
    };
    CHECK(build_sysmodule_list(raw).empty());
}

TEST_CASE("a sysmodule without toolbox falls back to its program id as name") {
    std::vector<RawSysmoduleEntry> raw = {
        {"4200000000000000", /*exefs*/ true, /*flag*/ false, ""},
    };
    std::vector<Sysmodule> out = build_sysmodule_list(raw);
    REQUIRE(out.size() == 1);
    CHECK(out[0].name == "4200000000000000");
    CHECK(out[0].has_metadata == false);
    CHECK(out[0].enabled == false);
    CHECK(out[0].requires_reboot == true);
}

TEST_CASE("output is sorted by display name, case-insensitively") {
    std::vector<RawSysmoduleEntry> raw = {
        {"1", true, false, R"({"name":"zsys"})"},
        {"2", true, false, R"({"name":"Atmo"})"},
    };
    std::vector<Sysmodule> out = build_sysmodule_list(raw);
    REQUIRE(out.size() == 2);
    CHECK(out[0].name == "Atmo");
    CHECK(out[1].name == "zsys");
}
