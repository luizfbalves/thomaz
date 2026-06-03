#include "doctest.h"
#include "core/feed/session_codec.hpp"

using namespace thomaz::feed;

TEST_CASE("serialize then parse round-trips a session") {
    Session s; s.token = "tok123"; s.username = "joao";
    std::string text = serialize_session(s);
    auto back = parse_session(text);
    REQUIRE(back.has_value());
    CHECK(back->token == "tok123");
    CHECK(back->username == "joao");
}

TEST_CASE("parse_session returns nullopt on empty or malformed input") {
    CHECK_FALSE(parse_session("").has_value());
    CHECK_FALSE(parse_session("only-one-line").has_value());
}

TEST_CASE("parse_session ignores trailing whitespace/newlines") {
    auto back = parse_session("tok123\njoao\n");
    REQUIRE(back.has_value());
    CHECK(back->token == "tok123");
    CHECK(back->username == "joao");
}

TEST_CASE("username is never empty in a valid session") {
    CHECK_FALSE(parse_session("tok123\n\n").has_value());
}
