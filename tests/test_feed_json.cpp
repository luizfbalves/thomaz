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

TEST_CASE("parse_auth_response success fills token + refreshToken") {
    auto r = parse_auth_response(R"({"ok":true,"token":"acc","refreshToken":"ref"})", 200);
    CHECK(r.ok);
    CHECK(r.token == "acc");
    CHECK(r.refreshToken == "ref");
}

TEST_CASE("parse_auth_response 401 -> invalid credentials, no crash") {
    auto r = parse_auth_response(R"({"ok":false,"error":"invalid_credentials"})", 401);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("parse_auth_response 409 -> username exists") {
    auto r = parse_auth_response(R"({"ok":false,"error":"username_already_exists"})", 409);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("parse_auth_response transport failure (status 0)") {
    auto r = parse_auth_response("", 0);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("parse_auth_response garbage body does not throw") {
    auto r = parse_auth_response("not json", 200);
    CHECK_FALSE(r.ok);
}

TEST_CASE("parse_refresh_response success / failure") {
    auto ok = parse_refresh_response(R"({"ok":true,"token":"t2","refreshToken":"r2"})", 200);
    CHECK(ok.ok); CHECK(ok.token == "t2"); CHECK(ok.refreshToken == "r2");
    auto bad = parse_refresh_response(R"({"ok":false,"error":"invalid_refresh_token"})", 401);
    CHECK_FALSE(bad.ok);
    CHECK_FALSE(parse_refresh_response("garbage", 200).ok);
}

TEST_CASE("parse_feed_page reads posts, gameTitleId hex string, createdAt") {
    const char* body = R"({
        "posts": [{
            "id":"p1",
            "author":{"id":"u1","username":"bea"},
            "imageUrl":"http://x/i.jpg",
            "caption":"hi",
            "gameTitleId":"0100000000010000",
            "gameName":"Mario",
            "likeCount":3,
            "likedByMe":true,
            "commentCount":2,
            "createdAt":1780000000
        }],
        "nextCursor":"abc",
        "hasMore":true
    })";
    auto page = parse_feed_page(body);
    REQUIRE(page.posts.size() == 1);
    const auto& p = page.posts[0];
    CHECK(p.id == "p1");
    CHECK(p.author.username == "bea");
    CHECK(p.gameTitleId == 0x0100000000010000ULL);
    CHECK(p.gameName == "Mario");
    CHECK(p.likeCount == 3);
    CHECK(p.likedByMe == true);
    CHECK(p.commentCount == 2);
    CHECK(p.createdAt == 1780000000);
    CHECK(page.nextCursor == "abc");
    CHECK(page.hasMore == true);
}

TEST_CASE("parse_feed_page tolerates missing fields and bad json") {
    auto empty = parse_feed_page("garbage");
    CHECK(empty.posts.empty());
    CHECK(empty.hasMore == false);

    auto partial = parse_feed_page(R"({"posts":[{"id":"p1"}],"hasMore":false})");
    REQUIRE(partial.posts.size() == 1);
    CHECK(partial.posts[0].id == "p1");
    CHECK(partial.posts[0].gameTitleId == 0); // missing -> 0
}

TEST_CASE("parse_comments reads list, tolerates bad json") {
    // API contract (GET /posts/:id/comments): bare JSON array.
    auto cs = parse_comments(R"([
        {"id":"c1","author":{"id":"u1","username":"kai"},"text":"gg","createdAt":1780000001}
    ])");
    REQUIRE(cs.size() == 1);
    CHECK(cs[0].id == "c1");
    CHECK(cs[0].author.username == "kai");
    CHECK(cs[0].text == "gg");
    CHECK(cs[0].createdAt == 1780000001);
    CHECK(parse_comments("nope").empty());
}

TEST_CASE("parse_post reads a single post, nullopt on bad json") {
    // API contract (POST /posts): { ok: true, post: {...} }.
    auto p = parse_post(R"({"post":{"id":"p9","caption":"yo","gameTitleId":"010000000E5EE000"}})");
    REQUIRE(p.has_value());
    CHECK(p->id == "p9");
    CHECK(p->caption == "yo");
    CHECK(p->gameTitleId == 0x010000000E5EE000ULL);
    CHECK_FALSE(parse_post("garbage").has_value());
}

TEST_CASE("parsers do not throw on type-mismatched fields (defensive)") {
    // feed page with wrong-typed scalars -> safe defaults, no throw
    auto page = parse_feed_page(R"({
        "posts":[{"id":"p1","caption":42,"likeCount":"5","likedByMe":"yes","createdAt":"oops","gameTitleId":99}],
        "nextCursor":123,
        "hasMore":1
    })");
    REQUIRE(page.posts.size() == 1);
    CHECK(page.posts[0].id == "p1");
    CHECK(page.posts[0].caption.empty());   // number -> default
    CHECK(page.posts[0].likeCount == 0);     // string -> default
    CHECK(page.posts[0].likedByMe == false); // string -> default
    CHECK(page.posts[0].createdAt == 0);     // string -> default
    CHECK(page.posts[0].gameTitleId == 0);   // number -> default (is_string guard)
    CHECK(page.nextCursor.empty());          // number -> default
    CHECK(page.hasMore == false);            // number -> default

    // auth with non-bool ok must not throw
    auto a = parse_auth_response(R"({"ok":"true","token":123})", 200);
    CHECK_FALSE(a.ok);

    // comments with wrong-typed fields
    auto cs = parse_comments(R"([{"id":"c1","text":99,"createdAt":"x"}])");
    REQUIRE(cs.size() == 1);
    CHECK(cs[0].id == "c1");
    CHECK(cs[0].text.empty());
    CHECK(cs[0].createdAt == 0);

    // post with wrong-typed createdAt
    auto p = parse_post(R"({"post":{"id":"p9","createdAt":"nope"}})");
    REQUIRE(p.has_value());
    CHECK(p->id == "p9");
    CHECK(p->createdAt == 0);
}
