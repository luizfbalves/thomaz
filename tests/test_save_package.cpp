#include "doctest.h"
#include "core/saves/save_package.hpp"

using namespace thomaz::core;

static std::vector<std::uint8_t> bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

TEST_CASE("empty package round-trips") {
    SavePackage pkg;
    auto blob = pack_save_package(pkg);
    auto out  = unpack_save_package(blob);
    REQUIRE(out.has_value());
    CHECK(out->files.empty());
}

TEST_CASE("single-profile package round-trips") {
    SavePackage pkg;
    pkg.files.push_back({ "1111/save.dat", bytes("hello world") });
    auto out = unpack_save_package(pack_save_package(pkg));
    REQUIRE(out.has_value());
    REQUIRE(out->files.size() == 1);
    CHECK(out->files[0].path == "1111/save.dat");
    CHECK(out->files[0].bytes == bytes("hello world"));
}

TEST_CASE("multi-profile package preserves order and bytes") {
    SavePackage pkg;
    pkg.files.push_back({ "aaaa/a.bin", bytes("A") });
    pkg.files.push_back({ "bbbb/sub/b.bin", bytes("BB") });
    pkg.files.push_back({ "aaaa/c.bin", {} }); // zero-length file
    auto out = unpack_save_package(pack_save_package(pkg));
    REQUIRE(out.has_value());
    REQUIRE(out->files.size() == 3);
    CHECK(out->files[1].path == "bbbb/sub/b.bin");
    CHECK(out->files[1].bytes == bytes("BB"));
    CHECK(out->files[2].bytes.empty());
}

TEST_CASE("corrupted blob is rejected") {
    CHECK_FALSE(unpack_save_package({}).has_value());                 // empty
    CHECK_FALSE(unpack_save_package(bytes("XXXX")).has_value());      // bad magic
    // valid magic + count=1 but truncated entry:
    std::vector<std::uint8_t> b = { 'T','S','A','V', 1,0,0,0, 5,0,0,0 };
    CHECK_FALSE(unpack_save_package(b).has_value());
}
