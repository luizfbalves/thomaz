#include "doctest.h"
#include "core/mods/mod_install.hpp"

using namespace thomaz::core;

static ArchiveEntry f(const std::string& p) { return ArchiveEntry{p, false}; }
static ArchiveEntry d(const std::string& p) { return ArchiveEntry{p, true}; }

TEST_CASE("is_safe_archive_path rejects absolute and traversal paths") {
    CHECK(is_safe_archive_path("romfs/a/b.bin"));
    CHECK_FALSE(is_safe_archive_path(""));
    CHECK_FALSE(is_safe_archive_path("/etc/passwd"));
    CHECK_FALSE(is_safe_archive_path(".."));
    CHECK_FALSE(is_safe_archive_path("romfs/../../x"));
    CHECK_FALSE(is_safe_archive_path("a/../b"));
}

TEST_CASE("plan_install: archive already rooted at romfs/ needs no strip") {
    InstallPlan p = plan_install({d("romfs/"), f("romfs/Actor/foo.bin")});
    REQUIRE(p.ok());
    CHECK(p.strip_prefix == "");
}

TEST_CASE("plan_install: contents/<tid>/romfs/ is stripped down to romfs/") {
    InstallPlan p = plan_install({
        f("contents/0100000000010000/romfs/Actor/foo.bin"),
        f("contents/0100000000010000/romfs/Pack/bar.bin"),
    });
    REQUIRE(p.ok());
    CHECK(p.strip_prefix == "contents/0100000000010000/");
}

TEST_CASE("plan_install: a single wrapping folder is stripped") {
    InstallPlan p = plan_install({f("Cool Skin/romfs/tex.bin")});
    REQUIRE(p.ok());
    CHECK(p.strip_prefix == "Cool Skin/");
}

TEST_CASE("plan_install: empty archive is rejected") {
    CHECK(plan_install({}).error == InstallError::Empty);
}

TEST_CASE("plan_install: no romfs segment is NotRomfs") {
    CHECK(plan_install({f("exefs/main.npdm")}).error == InstallError::NotRomfs);
}

TEST_CASE("plan_install: unsafe entry is rejected") {
    CHECK(plan_install({f("romfs/../../evil")}).error == InstallError::UnsafePath);
}

TEST_CASE("plan_install: two different romfs roots are Ambiguous") {
    InstallPlan p = plan_install({
        f("A/romfs/x.bin"),
        f("B/romfs/y.bin"),
    });
    CHECK(p.error == InstallError::Ambiguous);
}
