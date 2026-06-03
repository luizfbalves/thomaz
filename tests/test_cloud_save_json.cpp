#include "doctest.h"
#include "core/saves/cloud_save_json.hpp"

using namespace thomaz::core;

TEST_CASE("base64 decodes including padding") {
    auto d = base64_decode("aGVsbG8=");           // "hello"
    REQUIRE(d.has_value());
    std::string s(d->begin(), d->end());
    CHECK(s == "hello");
    CHECK(base64_decode("")->empty());
    CHECK_FALSE(base64_decode("!!!!").has_value()); // invalid alphabet
}

TEST_CASE("base64 rejects misplaced padding and accepts unpadded input") {
    CHECK_FALSE(base64_decode("dGhv=bWF6").has_value()); // '=' in the middle
    auto np = base64_decode("aGVsbG8");                  // unpadded "hello"
    REQUIRE(np.has_value());
    CHECK(std::string(np->begin(), np->end()) == "hello");
}

TEST_CASE("parse_slot_meta: 200 with a slot") {
    const char* body = R"({"slot":{"titleId":"0100000000010000","label":"Zelda","revision":4,"updatedAt":1733242800}})";
    auto m = parse_slot_meta(body, 200);
    REQUIRE(m.has_value());
    CHECK(m->exists);
    CHECK(m->revision == 4);
    CHECK(m->label == "Zelda");
    CHECK(m->updatedAt == 1733242800);
}

TEST_CASE("parse_slot_meta: 404 means no slot, not an error") {
    auto m = parse_slot_meta(R"({"error":"save_not_found"})", 404);
    REQUIRE(m.has_value());
    CHECK_FALSE(m->exists);
}

TEST_CASE("parse_slot_meta: other status is an error (nullopt)") {
    CHECK_FALSE(parse_slot_meta(R"({"error":"boom"})", 500).has_value());
}

TEST_CASE("parse_slot_data: meta plus decoded blob") {
    const char* body = R"({"slot":{"titleId":"0100000000010000","label":"x","revision":2,"updatedAt":10,"data":"aGk="}})";
    auto d = parse_slot_data(body, 200);
    REQUIRE(d.has_value());
    CHECK(d->meta.exists);
    CHECK(d->meta.revision == 2);
    std::string blob(d->data.begin(), d->data.end());
    CHECK(blob == "hi");
}

TEST_CASE("parse_slot_data: invalid base64 in data is rejected") {
    const char* body = R"({"slot":{"titleId":"01","label":"x","revision":1,"updatedAt":1,"data":"!!!!"}})";
    CHECK_FALSE(parse_slot_data(body, 200).has_value());
}

TEST_CASE("parse_push_revision reads the new revision") {
    auto r = parse_push_revision(R"({"ok":true,"slot":{"revision":7}})");
    REQUIRE(r.has_value());
    CHECK(*r == 7);
    CHECK_FALSE(parse_push_revision(R"({"nope":1})").has_value());
}

TEST_CASE("parse_error_message prefers the error field, falls back to status") {
    CHECK(parse_error_message(R"({"error":"revision_conflict"})", 409) == "revision_conflict");
    CHECK(parse_error_message("not json", 500) == "http_500");
}
