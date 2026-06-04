#include "doctest.h"
#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_urls.hpp"

#include <map>

using namespace thomaz::core;

static const char* SEARCH_JSON = R"({
  "_aMetadata": { "_nRecordCount": 1, "_nPerpage": 15, "_bIsComplete": true },
  "_aRecords": [ { "_idRow": 7, "_sModelName": "Mod", "_sName": "Hit",
                   "_sProfileUrl": "u", "_bHasFiles": true } ]
})";
static const char* FILES_JSON = R"({ "_aFiles": [
  { "_idRow": 50, "_sFile": "m.zip", "_nFilesize": 10, "_sMd5Checksum": "x",
    "_sDownloadUrl": "https://gamebanana.com/dl/50" } ] })";

static UrlFetcher mapFetcher(std::map<std::string, std::string> docs) {
    return [docs](const std::string& url) -> std::optional<std::string> {
        auto it = docs.find(url);
        if (it == docs.end()) return std::nullopt;
        return it->second;
    };
}

TEST_CASE("search_mods returns the parsed page on success") {
    auto fetch = mapFetcher({ { gb_search_url("hit", 0, 1), SEARCH_JSON } });
    BrowseResult r = search_mods("hit", 0, 1, fetch);
    REQUIRE(r.status == BrowseStatus::Ok);
    REQUIRE(r.page.records.size() == 1);
    CHECK(r.page.records[0].id == 7);
}

TEST_CASE("search_mods reports a network error when the fetch fails") {
    auto fetch = mapFetcher({});
    BrowseResult r = search_mods("hit", 0, 1, fetch);
    CHECK(r.status == BrowseStatus::NetworkError);
    CHECK(r.page.records.empty());
}

TEST_CASE("resolve_mod_files returns files on success") {
    auto fetch = mapFetcher({ { gb_mod_files_url(7), FILES_JSON } });
    ResolveResult r = resolve_mod_files(7, fetch);
    REQUIRE(r.status == ResolveStatus::Ok);
    REQUIRE(r.files.size() == 1);
    CHECK(r.files[0].download_url == "https://gamebanana.com/dl/50");
}

TEST_CASE("resolve_mod_files maps an error body to NotFound") {
    auto fetch = mapFetcher({ { gb_mod_files_url(7),
        R"({"_sErrorCode":"NO_SUCH_RECORD","_sErrorMessage":"gone"})" } });
    ResolveResult r = resolve_mod_files(7, fetch);
    CHECK(r.status == ResolveStatus::NotFound);
    CHECK(r.error == "gone");
}

TEST_CASE("resolve_mod_files maps a transport failure to NetworkError") {
    auto fetch = mapFetcher({});
    ResolveResult r = resolve_mod_files(7, fetch);
    CHECK(r.status == ResolveStatus::NetworkError);
}
