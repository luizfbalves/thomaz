#include "doctest.h"
#include "platform/themes/cfw_paths.hpp"
#include <filesystem>
#include <fstream>

using namespace thomaz;

TEST_CASE("target_map covers the known applet targets") {
    CHECK(target_map("ResidentMenu")->title_id == "0100000000001000");
    CHECK(target_map("ResidentMenu")->szs == "ResidentMenu.szs");
    CHECK(target_map("MyPage")->title_id == "0100000000001013");
    CHECK(target_map("Psl")->title_id == "0100000000001007");
    CHECK(target_map("Entrance")->szs == "Entrance.szs");
    CHECK_FALSE(target_map("").has_value());
    CHECK_FALSE(target_map("Bogus").has_value());
}

TEST_CASE("output_szs_path is the LayeredFS lyt path") {
    std::string p = output_szs_path("ResidentMenu");
    CHECK(p == layeredfs_root() + "/0100000000001000/romfs/lyt/ResidentMenu.szs");
    CHECK(output_szs_path("Bogus").empty());
}

TEST_CASE("base_szs_path joins the base dir") {
    CHECK(base_szs_path("Set") == base_layout_dir() + "/Set.szs");
}

TEST_CASE("base_present_for requires every base file to exist") {
    namespace fs = std::filesystem;
    fs::create_directories(base_layout_dir());
    fs::remove(base_szs_path("Entrance"));
    fs::remove(base_szs_path("Set"));

    CHECK_FALSE(base_present_for({"Entrance"}));

    std::ofstream(base_szs_path("Entrance")) << "x";
    CHECK(base_present_for({"Entrance"}));
    CHECK_FALSE(base_present_for({"Entrance", "Set"}));   // Set missing
    CHECK_FALSE(base_present_for({"Bogus"}));             // unknown target

    fs::remove(base_szs_path("Entrance"));
}
