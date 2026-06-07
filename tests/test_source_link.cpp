#include "doctest.h"
#include "core/games/source_link.hpp"

using namespace thomaz::core;

TEST_CASE("serialize_source_link omits credential from JSON") {
    SourceConfig cfg;
    cfg.label      = "My Shop";
    cfg.url        = "https://example.com/index";
    cfg.authType   = SourceAuthType::Header;
    cfg.authSecret = "super-secret-token";
    const std::string out = serialize_source_link(cfg);
    CHECK(out.find("super-secret-token") == std::string::npos);
    CHECK(out.find("authSecret") == std::string::npos);
}

TEST_CASE("serialize_source_link maps authType to API strings") {
    SourceConfig cfg;
    cfg.label = "x";
    cfg.url   = "https://x";
    cfg.authType = SourceAuthType::BasicInUrl;
    CHECK(serialize_source_link(cfg).find("basicInUrl") != std::string::npos);
    cfg.authType = SourceAuthType::Referrer;
    CHECK(serialize_source_link(cfg).find("referrer") != std::string::npos);
}

TEST_CASE("parse_source_link round-trips label, url, authType") {
    SourceConfig in;
    in.label    = "Shop";
    in.url      = "https://shop.test/t";
    in.authType = SourceAuthType::Header;
    in.authSecret = "must-not-leak";
    const std::string json = serialize_source_link(in);
    auto parsed = parse_source_link(json);
    REQUIRE(parsed.has_value());
    CHECK(parsed->label == in.label);
    CHECK(parsed->url == in.url);
    CHECK(parsed->authType == SourceAuthType::Header);
    CHECK(parsed->authSecret.empty());
}

TEST_CASE("parse_source_link rejects malformed body") {
    CHECK_FALSE(parse_source_link("not json").has_value());
    CHECK_FALSE(parse_source_link("[]").has_value());
}
