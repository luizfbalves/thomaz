#include "doctest.h"
#include "platform/saves/save_backup_io.hpp"

using namespace thomaz;

TEST_CASE("is_safe_relpath accepts normal profile-prefixed paths") {
    CHECK(is_safe_relpath("1111/save.dat"));
    CHECK(is_safe_relpath("aaaa/sub/file.bin"));
}

TEST_CASE("is_safe_relpath rejects empty, absolute, and traversal paths") {
    CHECK_FALSE(is_safe_relpath(""));
    CHECK_FALSE(is_safe_relpath("/etc/passwd"));
    CHECK_FALSE(is_safe_relpath(".."));
    CHECK_FALSE(is_safe_relpath("../x"));
    CHECK_FALSE(is_safe_relpath("a/../../b"));
    CHECK_FALSE(is_safe_relpath("aaaa/../bbbb/file"));
}
