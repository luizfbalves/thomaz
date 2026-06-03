// doctest's main is provided once by tests/test_main.cpp — do NOT define it here.
#include "doctest.h"
#include "core/feed/session_codec.hpp"

using namespace thomaz::feed;

TEST_CASE("session round-trips token, refreshToken, username") {
    Session s{ "acc-tok", "ref-tok", "luiz" };
    std::string text = serialize_session(s);
    auto parsed = parse_session(text);
    REQUIRE(parsed.has_value());
    CHECK(parsed->token == "acc-tok");
    CHECK(parsed->refreshToken == "ref-tok");
    CHECK(parsed->username == "luiz");
}

TEST_CASE("legacy 2-line session parses with empty refreshToken") {
    // Old format: "<token>\n<username>\n"
    auto parsed = parse_session("acc-tok\nluiz\n");
    REQUIRE(parsed.has_value());
    CHECK(parsed->token == "acc-tok");
    CHECK(parsed->refreshToken.empty());
    CHECK(parsed->username == "luiz");
}

TEST_CASE("empty/garbage session is rejected") {
    CHECK_FALSE(parse_session("").has_value());
    CHECK_FALSE(parse_session("\n\n").has_value());
}

TEST_CASE("3-line session with empty refreshToken parses (logged in pre-refresh)") {
    Session s{ "acc-tok", "", "luiz" };
    auto parsed = parse_session(serialize_session(s));
    REQUIRE(parsed.has_value());
    CHECK(parsed->token == "acc-tok");
    CHECK(parsed->refreshToken.empty());
    CHECK(parsed->username == "luiz");
}
