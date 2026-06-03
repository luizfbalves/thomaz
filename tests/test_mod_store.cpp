#include "doctest.h"
#include "platform/mods/mod_store.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

using namespace thomaz;

static const std::string TMP = "test-mods-tmp";

static void write_file(const std::string& path, const std::string& body) {
    std::ofstream(path, std::ios::binary) << body;
}
static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

TEST_CASE("copy_tree replicates a directory recursively") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/src").c_str(), 0777);
    ::mkdir((TMP + "/src/romfs").c_str(), 0777);
    write_file(TMP + "/src/romfs/a.bin", "AAA");

    std::string err;
    REQUIRE(copy_tree(TMP + "/src", TMP + "/dst", &err));
    CHECK(read_file(TMP + "/dst/romfs/a.bin") == "AAA");

    remove_tree(TMP);
}

TEST_CASE("remove_tree deletes a directory and its contents") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/x").c_str(), 0777);
    write_file(TMP + "/x/f", "z");

    REQUIRE(remove_tree(TMP + "/x"));
    struct stat st;
    CHECK(::stat((TMP + "/x").c_str(), &st) != 0); // gone

    remove_tree(TMP);
}

TEST_CASE("list_subdirs returns immediate child directory names") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/Skin A").c_str(), 0777);
    ::mkdir((TMP + "/Skin B").c_str(), 0777);
    write_file(TMP + "/loose.txt", "x"); // files are ignored

    std::vector<std::string> got = list_subdirs(TMP);
    std::sort(got.begin(), got.end());
    REQUIRE(got.size() == 2);
    CHECK(got[0] == "Skin A");
    CHECK(got[1] == "Skin B");

    remove_tree(TMP);
}

TEST_CASE("remove_tree on a nonexistent path reports success") {
    remove_tree(TMP);
    CHECK(remove_tree(TMP + "/does-not-exist"));
}

TEST_CASE("copy_tree overwrites an existing destination file") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/src").c_str(), 0777);
    write_file(TMP + "/src/a.bin", "NEW");
    ::mkdir((TMP + "/dst").c_str(), 0777);
    write_file(TMP + "/dst/a.bin", "OLD");

    std::string err;
    REQUIRE(copy_tree(TMP + "/src", TMP + "/dst", &err));
    CHECK(read_file(TMP + "/dst/a.bin") == "NEW");

    remove_tree(TMP);
}

TEST_CASE("markers round-trip and clear") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    std::string m = TMP + "/.active";

    CHECK_FALSE(read_marker(m).has_value());
    REQUIRE(write_marker(m, "Skin A"));
    REQUIRE(read_marker(m).has_value());
    CHECK(*read_marker(m) == "Skin A");
    REQUIRE(clear_marker(m));
    CHECK_FALSE(read_marker(m).has_value());

    remove_tree(TMP);
}
