#include "doctest.h"
#include "core/cheat_repository.hpp"
#include "core/db_paths.hpp"

#include <map>

using namespace thomaz::core;

static const char* VERSIONS_JSON = R"({
  "0": "3CA12DFAAF9C82DA",
  "262144": "B424BE150A8E7D78",
  "393216": "B424BE150A8E7D78",
  "latest": 393216,
  "title": "Super Mario Odyssey"
})";

static const char* CHEATS_JSON = R"({
  "B424BE150A8E7D78": {
    "[Infinite Health]": "[Infinite Health]\n11160000 5C3BE7DC 00000000\n20000000\n",
    "{Master}": "{Master}\n580F0000 0149D940\n"
  },
  "attribution": { "[Infinite Health]": "someuser" }
})";

// A canned URL->body fetcher; any URL not in the map returns nullopt (network error).
static UrlFetcher mapFetcher(std::map<std::string, std::string> docs) {
    return [docs](const std::string& url) -> std::optional<std::string> {
        auto it = docs.find(url);
        if (it == docs.end())
            return std::nullopt;
        return it->second;
    };
}

static constexpr std::uint64_t SMO = 0x0100000000010000ULL;

TEST_CASE("fetch_cheat_set resolves an exact version to its cheats") {
    auto fetch = mapFetcher({
        {versions_url(SMO), VERSIONS_JSON},
        {cheats_url(SMO),   CHEATS_JSON},
    });
    FetchResult r = fetch_cheat_set(SMO, 393216, fetch);

    REQUIRE(r.status == FetchStatus::Ok);
    CHECK(r.set.resolution.source == Resolution::Source::ExactVersion);
    CHECK(r.set.resolution.build_id == "B424BE150A8E7D78");
    CHECK(r.set.sd_path == "/atmosphere/contents/0100000000010000/cheats/B424BE150A8E7D78.txt");
    CHECK(r.set.title == "Super Mario Odyssey");
    CHECK(r.set.cheats.size() == 2);
}

TEST_CASE("fetch_cheat_set falls back to an older build for an unknown version") {
    auto fetch = mapFetcher({
        {versions_url(SMO), VERSIONS_JSON},
        {cheats_url(SMO),   CHEATS_JSON},
    });
    // Version 999999 is not in the map -> newest build_id that has cheats.
    FetchResult r = fetch_cheat_set(SMO, 999999, fetch);

    REQUIRE(r.status == FetchStatus::Ok);
    CHECK(r.set.resolution.source == Resolution::Source::FallbackOlderBuild);
    CHECK(r.set.resolution.build_id == "B424BE150A8E7D78");
    CHECK_FALSE(r.set.cheats.empty());
}

TEST_CASE("fetch_cheat_set reports NotInDb when no build has cheats") {
    const char* emptyCheats = R"({ "attribution": { "[x]": "u" } })";
    auto fetch = mapFetcher({
        {versions_url(SMO), VERSIONS_JSON},
        {cheats_url(SMO),   emptyCheats},
    });
    FetchResult r = fetch_cheat_set(SMO, 393216, fetch);

    CHECK(r.status == FetchStatus::NotInDb);
    CHECK(r.set.cheats.empty());
}

TEST_CASE("fetch_cheat_set treats a reachable-but-empty doc (HTTP 404) as NotInDb, not NetworkError") {
    // The fetcher contract: nullopt == transport failure; an empty string ==
    // server reached but no such document (404 — the game simply isn't in the
    // db). A 404 must NOT surface as a connection error to the user.
    auto fetch = mapFetcher({
        {versions_url(SMO), std::string{}},
        {cheats_url(SMO),   std::string{}},
    });
    FetchResult r = fetch_cheat_set(SMO, 393216, fetch);
    CHECK(r.status == FetchStatus::NotInDb);
}

TEST_CASE("fetch_cheat_set reports NetworkError when a document is unreachable") {
    // versions reachable, cheats missing
    auto fetch1 = mapFetcher({ {versions_url(SMO), VERSIONS_JSON} });
    CHECK(fetch_cheat_set(SMO, 393216, fetch1).status == FetchStatus::NetworkError);

    // nothing reachable
    auto fetch2 = mapFetcher({});
    CHECK(fetch_cheat_set(SMO, 393216, fetch2).status == FetchStatus::NetworkError);
}
