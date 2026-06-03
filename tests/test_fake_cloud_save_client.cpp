#include "doctest.h"
#include "platform/saves/fake_cloud_save_client.hpp"

using namespace thomaz;

static std::vector<std::uint8_t> bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

TEST_CASE("status is empty before any push") {
    FakeCloudSaveClient c;
    auto s = c.getStatus("tok", 0x100ULL);
    CHECK(s.ok);
    CHECK_FALSE(s.exists);
}

TEST_CASE("push then status/pull reflect the upload") {
    FakeCloudSaveClient c;
    auto p = c.push("tok", 0x100ULL, bytes("save-bytes"), "Zelda", 0);
    REQUIRE(p.ok);
    CHECK(p.newRevision == 1);

    auto s = c.getStatus("tok", 0x100ULL);
    CHECK(s.exists);
    CHECK(s.revision == 1);
    CHECK(s.label == "Zelda");

    auto d = c.pull("tok", 0x100ULL);
    REQUIRE(d.ok);
    CHECK(d.exists);
    CHECK(d.revision == 1);
    std::string blob(d.blob.begin(), d.blob.end());
    CHECK(blob == "save-bytes");
}

TEST_CASE("pushing with a stale revision conflicts") {
    FakeCloudSaveClient c;
    c.push("tok", 0x100ULL, bytes("v1"), "g", 0);   // -> revision 1
    auto stale = c.push("tok", 0x100ULL, bytes("v2"), "g", 0); // expected 1, sent 0
    CHECK_FALSE(stale.ok);
    CHECK(stale.conflict);

    auto good = c.push("tok", 0x100ULL, bytes("v2"), "g", 1); // correct revision
    CHECK(good.ok);
    CHECK(good.newRevision == 2);
}
