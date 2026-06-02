#include "doctest.h"
#include "core/update.hpp"

using namespace thomaz::core;

static const char* RELEASE_JSON = R"({
  "tag_name": "v0.2.0",
  "name": "thomaz 0.2.0",
  "body": "- new stuff\n- fixes",
  "assets": [
    { "name": "thomaz.nro", "browser_download_url": "https://github.com/x/thomaz/releases/download/v0.2.0/thomaz.nro" },
    { "name": "other.zip",  "browser_download_url": "https://example.com/other.zip" }
  ]
})";

TEST_CASE("parse_latest_release extracts tag, notes, and the matching asset url") {
    ReleaseInfo r = parse_latest_release(RELEASE_JSON, "thomaz.nro");
    REQUIRE(r.valid);
    CHECK(r.tag == "v0.2.0");
    CHECK(r.nro_url == "https://github.com/x/thomaz/releases/download/v0.2.0/thomaz.nro");
    CHECK(r.notes == "- new stuff\n- fixes");
}

TEST_CASE("parse_latest_release is invalid when the asset is missing") {
    ReleaseInfo r = parse_latest_release(RELEASE_JSON, "missing.nro");
    CHECK_FALSE(r.valid);
    CHECK(r.tag == "v0.2.0");      // tag still parsed
    CHECK(r.nro_url.empty());
}

TEST_CASE("parse_latest_release on garbage is invalid") {
    CHECK_FALSE(parse_latest_release("not json", "thomaz.nro").valid);
    CHECK_FALSE(parse_latest_release("{}", "thomaz.nro").valid);
}

TEST_CASE("is_newer_version compares numerically, ignoring leading v") {
    CHECK(is_newer_version("v0.2.0", "0.1.0"));
    CHECK(is_newer_version("0.1.1", "0.1.0"));
    CHECK(is_newer_version("v1.0.0", "0.9.9"));
    CHECK(is_newer_version("0.2", "0.1.5"));        // missing parts = 0
    CHECK_FALSE(is_newer_version("0.1.0", "0.1.0")); // equal
    CHECK_FALSE(is_newer_version("0.1.0", "v0.2.0"));
    CHECK_FALSE(is_newer_version("v0.1.0", "0.1.0"));
}

TEST_CASE("is_newer_version tolerates suffixes") {
    CHECK(is_newer_version("v0.2.0-beta", "0.1.0"));
    CHECK_FALSE(is_newer_version("v0.1.0-beta", "0.1.0")); // 0.1.0 == 0.1.0
}
