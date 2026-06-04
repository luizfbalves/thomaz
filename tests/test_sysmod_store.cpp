#include "doctest.h"
#include "platform/sysmod/sysmod_store.hpp"

#include <cstdio>
#include <sys/stat.h>
#include <string>

using namespace thomaz;

namespace {
const std::string ROOT = "test-sysmod-tmp";

void rm_rf(const std::string& p) {
    std::string cmd = "rm -rf '" + p + "'";
    (void)std::system(cmd.c_str());
}
void mkpath(const std::string& p) {
    std::string cmd = "mkdir -p '" + p + "'";
    (void)std::system(cmd.c_str());
}
void touch(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "wb");
    if (f) std::fclose(f);
}
bool exists(const std::string& p) {
    struct stat st; return ::stat(p.c_str(), &st) == 0;
}
} // namespace

TEST_CASE("scan_contents finds an exefs sysmodule and reads its flag + toolbox") {
    rm_rf(ROOT);
    mkpath(ROOT + "/00FF0000636C6BFF/flags");
    touch(ROOT + "/00FF0000636C6BFF/exefs.nsp");
    touch(ROOT + "/00FF0000636C6BFF/flags/boot2.flag");
    std::FILE* tb = std::fopen((ROOT + "/00FF0000636C6BFF/toolbox.json").c_str(), "wb");
    REQUIRE(tb != nullptr);
    std::fputs(R"({"name":"sys-clk"})", tb);
    std::fclose(tb);

    // A romfs-only mod folder that must be ignored.
    mkpath(ROOT + "/0100000000010000/romfs");

    auto entries = sysmod_scan_contents(ROOT);
    rm_rf(ROOT);

    REQUIRE(entries.size() == 2); // scan returns all folders; core filters exefs
    const core::RawSysmoduleEntry* mod = nullptr;
    const core::RawSysmoduleEntry* game = nullptr;
    for (auto& e : entries) {
        if (e.program_id == "00FF0000636C6BFF") mod = &e;
        if (e.program_id == "0100000000010000") game = &e;
    }
    REQUIRE(mod != nullptr);
    CHECK(mod->has_exefs == true);
    CHECK(mod->has_boot2_flag == true);
    CHECK(mod->toolbox_json == R"({"name":"sys-clk"})");
    REQUIRE(game != nullptr);
    CHECK(game->has_exefs == false);
}

TEST_CASE("scan_contents on a missing root returns empty") {
    CHECK(sysmod_scan_contents("does-not-exist-xyz").empty());
}

TEST_CASE("set_boot2_flag creates and removes the flag file") {
    rm_rf(ROOT);
    mkpath(ROOT + "/AAAA");
    const std::string dir = ROOT + "/AAAA";

    CHECK(sysmod_set_boot2_flag(dir, true));
    CHECK(exists(dir + "/flags/boot2.flag"));

    CHECK(sysmod_set_boot2_flag(dir, false));
    CHECK(!exists(dir + "/flags/boot2.flag"));

    rm_rf(ROOT);
}
