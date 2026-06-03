#include "doctest.h"
#include <nlohmann/json.hpp>
#include "core/feed/feed_json.hpp"

using namespace thomaz::feed;
using nlohmann::json;

TEST_CASE("build_credentials_body emits username and password") {
    auto j = json::parse(build_credentials_body("luiz", "s3cret"));
    CHECK(j.at("username") == "luiz");
    CHECK(j.at("password") == "s3cret");
}

TEST_CASE("build_refresh_body emits refreshToken") {
    auto j = json::parse(build_refresh_body("ref-tok"));
    CHECK(j.at("refreshToken") == "ref-tok");
}

TEST_CASE("build_like_body emits boolean liked") {
    CHECK(json::parse(build_like_body(true)).at("liked") == true);
    CHECK(json::parse(build_like_body(false)).at("liked") == false);
}

TEST_CASE("build_comment_body emits text") {
    auto j = json::parse(build_comment_body("nice run!"));
    CHECK(j.at("text") == "nice run!");
}
