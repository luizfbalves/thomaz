#include "doctest.h"
#include "platform/fs_util.hpp"

#include <filesystem>
#include <string>
#include <sys/stat.h>

using namespace thomaz;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper: char-by-char ensure_parent_dirs copied verbatim from
// source/platform/themes/theme_install.cpp:38-46 — used as the D-05 oracle.
// ---------------------------------------------------------------------------
static void ref_ensure_parent_dirs(const std::string& file) {
    std::string acc;
    for (size_t i = 0; i < file.size(); ++i) {
        acc.push_back(file[i]);
        if (file[i] == '/' && acc.size() > 1) ::mkdir(acc.c_str(), 0777);
    }
}

// ---------------------------------------------------------------------------
// Returns true if the given path exists as a directory.
// ---------------------------------------------------------------------------
static bool dir_exists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

TEST_CASE("ensure_parent_dirs creates parent directories but not the final segment") {
    fs::path tmp = fs::temp_directory_path() / "test_fs_util_1";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    std::string path = tmp.string() + "/themes/a/b/c.bin";
    thomaz::ensure_parent_dirs(path);

    // Intermediate dirs must exist
    CHECK(dir_exists(tmp.string() + "/themes"));
    CHECK(dir_exists(tmp.string() + "/themes/a"));
    CHECK(dir_exists(tmp.string() + "/themes/a/b"));

    // Final segment must NOT be created as a directory
    CHECK_FALSE(dir_exists(path));

    fs::remove_all(tmp);
}

TEST_CASE("ensure_parent_dirs handles trailing-slash path") {
    fs::path tmp = fs::temp_directory_path() / "test_fs_util_2";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    // Trailing slash: "x/y/" — both x and x/y should be created.
    std::string path = tmp.string() + "/x/y/";
    thomaz::ensure_parent_dirs(path);

    CHECK(dir_exists(tmp.string() + "/x"));
    CHECK(dir_exists(tmp.string() + "/x/y"));

    fs::remove_all(tmp);
}

// IN-05: this asserts equivalence-OVER-THE-TESTED-INPUTS, not universal
// equivalence. The canonical impl guards with `!dir.empty()` while the oracle
// guards with `acc.size() > 1`, so they DIVERGE on a leading-`/`-only or
// double-slash segment such as "//x" (canonical would mkdir("/"), the oracle
// would skip it). All inputs below are repo-shaped absolute temp paths whose
// first real segment is non-empty, where the two agree. Do not read this test as
// proof that canonical == oracle for arbitrary paths.
TEST_CASE("D-05: canonical ensure_parent_dirs is equivalent to char-by-char oracle") {
    fs::path tmp_canon = fs::temp_directory_path() / "test_fs_util_canon";
    fs::path tmp_ref   = fs::temp_directory_path() / "test_fs_util_ref";
    fs::remove_all(tmp_canon);
    fs::remove_all(tmp_ref);
    fs::create_directories(tmp_canon);
    fs::create_directories(tmp_ref);

    // Case 1: interior path ("themes/a/b/c.bin" equivalent)
    {
        std::string canon_path = tmp_canon.string() + "/themes/a/b/c.bin";
        std::string ref_path   = tmp_ref.string()   + "/themes/a/b/c.bin";

        thomaz::ensure_parent_dirs(canon_path);
        ref_ensure_parent_dirs(ref_path);

        // Both must create the same set of parent directories
        CHECK(dir_exists(tmp_canon.string() + "/themes")       == dir_exists(tmp_ref.string() + "/themes"));
        CHECK(dir_exists(tmp_canon.string() + "/themes/a")     == dir_exists(tmp_ref.string() + "/themes/a"));
        CHECK(dir_exists(tmp_canon.string() + "/themes/a/b")   == dir_exists(tmp_ref.string() + "/themes/a/b"));
        CHECK(dir_exists(canon_path)                           == dir_exists(ref_path)); // neither creates the file leaf
    }

    // Case 2: trailing-slash path ("x/y/")
    {
        std::string canon_path = tmp_canon.string() + "/x/y/";
        std::string ref_path   = tmp_ref.string()   + "/x/y/";

        thomaz::ensure_parent_dirs(canon_path);
        ref_ensure_parent_dirs(ref_path);

        CHECK(dir_exists(tmp_canon.string() + "/x")   == dir_exists(tmp_ref.string() + "/x"));
        CHECK(dir_exists(tmp_canon.string() + "/x/y") == dir_exists(tmp_ref.string() + "/x/y"));
    }

    fs::remove_all(tmp_canon);
    fs::remove_all(tmp_ref);
}
