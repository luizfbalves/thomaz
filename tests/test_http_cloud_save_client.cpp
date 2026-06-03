#include "doctest.h"
#include "platform/saves/http_cloud_save_client.hpp"

using namespace thomaz;

namespace {
// Records the last request and returns a canned response.
struct StubHttp : IHttpClient {
    HttpRequest  last;
    HttpResponse next;
    HttpResponse request(const HttpRequest& req) override {
        last = req;
        return next;
    }
};
} // namespace

TEST_CASE("getStatus issues a GET with bearer and parses the slot") {
    StubHttp http;
    http.next = HttpResponse{ 200,
        R"({"slot":{"titleId":"0100000000010000","label":"Z","revision":4,"updatedAt":9}})" };
    HttpCloudSaveClient c(&http, "http://api.test");

    auto s = c.getStatus("mytoken", 0x0100000000010000ULL);
    CHECK(s.ok);
    CHECK(s.exists);
    CHECK(s.revision == 4);
    CHECK(http.last.method == HttpMethod::Get);
    CHECK(http.last.url == "http://api.test/saves/0100000000010000");
    bool hasBearer = false;
    for (auto& h : http.last.headers)
        if (h.first == "Authorization" && h.second == "Bearer mytoken") hasBearer = true;
    CHECK(hasBearer);
}

TEST_CASE("getStatus maps 404 to exists=false (still ok)") {
    StubHttp http;
    http.next = HttpResponse{ 404, R"({"error":"save_not_found"})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto s = c.getStatus("t", 0x1ULL);
    CHECK(s.ok);
    CHECK_FALSE(s.exists);
}

TEST_CASE("getStatus maps 401 to an auth-expired error") {
    StubHttp http;
    http.next = HttpResponse{ 401, R"({"error":"unauthorized"})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto s = c.getStatus("t", 0x1ULL);
    CHECK_FALSE(s.ok);
    CHECK(s.error == kCloudAuthExpired);
}

TEST_CASE("pull GETs with includeData and decodes the blob") {
    StubHttp http;
    http.next = HttpResponse{ 200,
        R"({"slot":{"titleId":"01","label":"x","revision":2,"updatedAt":1,"data":"aGk="}})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto d = c.pull("t", 0x1ULL);
    REQUIRE(d.ok);
    CHECK(d.exists);
    CHECK(d.revision == 2);
    std::string blob(d.blob.begin(), d.blob.end());
    CHECK(blob == "hi");
    CHECK(http.last.url == "http://api.test/saves/0000000000000001?includeData=1");
}

TEST_CASE("push sends multipart and reads the new revision") {
    StubHttp http;
    http.next = HttpResponse{ 200, R"({"ok":true,"slot":{"revision":3}})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    std::vector<std::uint8_t> blob = { 1, 2, 3 };
    auto r = c.push("t", 0x1ULL, blob, "Zelda", 2);
    REQUIRE(r.ok);
    CHECK(r.newRevision == 3);
    CHECK(http.last.method == HttpMethod::Put);
    REQUIRE(http.last.files.size() == 1);
    CHECK(http.last.files[0].field == "data");
    CHECK(http.last.files[0].bytes == blob);
    bool sentRevision = false, sentLabel = false;
    for (auto& f : http.last.fields) {
        if (f.first == "revision" && f.second == "2") sentRevision = true;
        if (f.first == "label" && f.second == "Zelda") sentLabel = true;
    }
    CHECK(sentRevision);
    CHECK(sentLabel);
}

TEST_CASE("push maps 409 to a conflict and 413 to a too-large error") {
    StubHttp http;
    http.next = HttpResponse{ 409, R"({"error":"revision_conflict"})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto conflict = c.push("t", 0x1ULL, { 1 }, "g", 0);
    CHECK_FALSE(conflict.ok);
    CHECK(conflict.conflict);

    http.next = HttpResponse{ 413, R"({"error":"save_too_large"})" };
    auto tooBig = c.push("t", 0x1ULL, { 1 }, "g", 0);
    CHECK_FALSE(tooBig.ok);
    CHECK_FALSE(tooBig.conflict);
    CHECK(tooBig.error == "save_too_large");
}
