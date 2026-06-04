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

static const char* GAME_SEARCH_JSON = R"json({
  "_aMetadata": { "_nRecordCount": 2, "_nPerpage": 15, "_bIsComplete": true },
  "_aRecords": [
    { "_idRow": 999, "_sModelName": "Game", "_sName": "Splatoon", "_sProfileUrl": "g" },
    { "_idRow": 6170, "_sModelName": "Game", "_sName": "Splatoon (Switch)", "_sProfileUrl": "g" }
  ]
})json";
static const char* GAME_MODS_JSON = R"json({
  "_aMetadata": { "_nRecordCount": 1, "_nPerpage": 50, "_bIsComplete": true },
  "_aRecords": [ { "_idRow": 42, "_sModelName": "Mod", "_sName": "Ink Skin",
                   "_sProfileUrl": "m", "_bHasFiles": true } ]
})json";

TEST_CASE("resolve_game uses the static override without any fetch") {
    auto fetch = mapFetcher({}); // override hit must not need the network
    GameResolve g = resolve_game(0x01007EF00011E000ULL, "Zelda BotW", fetch);
    REQUIRE(g.status == GameResolveStatus::Ok);
    CHECK(g.source == GameResolveSource::Override);
    CHECK(g.game_id == 6386);
}

TEST_CASE("resolve_game falls back to name search and prefers the Switch record") {
    std::uint64_t tid = 0x0100AAAAAAAAA000ULL; // not in the override table
    auto fetch = mapFetcher({ { gb_game_search_url("Splatoon (Switch)", 1), GAME_SEARCH_JSON } });
    GameResolve g = resolve_game(tid, "Splatoon", fetch);
    REQUIRE(g.status == GameResolveStatus::Ok);
    CHECK(g.source == GameResolveSource::NameMatch);
    CHECK(g.game_id == 6170);                 // the "(Switch)" record, not the first
    CHECK(g.matched_name == "Splatoon (Switch)");
}

TEST_CASE("resolve_game reports NotFound when the search has no records") {
    std::uint64_t tid = 0x0100AAAAAAAAA000ULL;
    auto fetch = mapFetcher({ { gb_game_search_url("Nope (Switch)", 1),
        R"({"_aMetadata":{"_nRecordCount":0},"_aRecords":[]})" } });
    GameResolve g = resolve_game(tid, "Nope", fetch);
    CHECK(g.status == GameResolveStatus::NotFound);
}

TEST_CASE("resolve_game reports NetworkError on transport failure") {
    std::uint64_t tid = 0x0100AAAAAAAAA000ULL;
    auto fetch = mapFetcher({});
    GameResolve g = resolve_game(tid, "Splatoon", fetch);
    CHECK(g.status == GameResolveStatus::NetworkError);
}

TEST_CASE("resolve_game skips the 'Nintendo Switch' platform hub and picks the real game") {
    std::uint64_t tid = 0x0100BBBBBBBBB000ULL; // not in override table
    const char* HUB_FIRST = R"hub({
      "_aMetadata": { "_nRecordCount": 2, "_nPerpage": 15, "_bIsComplete": true },
      "_aRecords": [
        { "_idRow": 6384, "_sModelName": "Game", "_sName": "Nintendo Switch", "_sProfileUrl": "g" },
        { "_idRow": 15056, "_sModelName": "Game", "_sName": "Splatoon 3 (Switch)", "_sProfileUrl": "g" }
      ]
    })hub";
    auto fetch = mapFetcher({ { gb_game_search_url("Splatoon 3 (Switch)", 1), HUB_FIRST } });
    GameResolve g = resolve_game(tid, "Splatoon 3", fetch);
    REQUIRE(g.status == GameResolveStatus::Ok);
    CHECK(g.game_id == 15056);                  // NOT 6384 (the hub)
    CHECK(g.matched_name == "Splatoon 3 (Switch)");
}

TEST_CASE("resolve_game returns NotFound when best_match yields an unrelated game") {
    std::uint64_t tid = 0x0100CCCCCCCCC000ULL; // not in override table
    const char* WRONG = R"json({
      "_aMetadata": { "_nRecordCount": 1, "_nPerpage": 15, "_bIsComplete": true },
      "_aRecords": [
        { "_idRow": 14819, "_sModelName": "Game", "_sName": "The Witcher 3 (Switch)", "_sProfileUrl": "g" }
      ]
    })json";
    auto fetch = mapFetcher({ { gb_game_search_url("Splatoon 3 (Switch)", 1), WRONG } });
    GameResolve g = resolve_game(tid, "Splatoon 3", fetch);
    CHECK(g.status == GameResolveStatus::NotFound);   // refuses the wrong game
}

TEST_CASE("resolve_game does NOT match a longer game name by substring") {
    // Regression: "Resident Evil 4" must not resolve to "Resident Evil 4
    // Remake" (its name is a superset). Otherwise the classic game's mod page
    // shows — and downloads stage — the remake's mods.
    std::uint64_t tid = 0x0100DDDDDDDDD000ULL; // not in override table
    const char* REMAKE_ONLY = R"json({
      "_aMetadata": { "_nRecordCount": 1, "_nPerpage": 15, "_bIsComplete": true },
      "_aRecords": [
        { "_idRow": 18000, "_sModelName": "Game",
          "_sName": "Resident Evil 4 Remake (Switch)", "_sProfileUrl": "g" }
      ]
    })json";
    auto fetch = mapFetcher({
        { gb_game_search_url("Resident Evil 4 (Switch)", 1), REMAKE_ONLY } });
    GameResolve g = resolve_game(tid, "Resident Evil 4", fetch);
    CHECK(g.status == GameResolveStatus::NotFound); // refuses the remake page
}

TEST_CASE("resolve_game matches the exact game even when '(Switch)' is appended") {
    std::uint64_t tid = 0x0100EEEEEEEEE000ULL; // not in override table
    const char* EXACT = R"json({
      "_aMetadata": { "_nRecordCount": 1, "_nPerpage": 15, "_bIsComplete": true },
      "_aRecords": [
        { "_idRow": 18001, "_sModelName": "Game",
          "_sName": "Resident Evil 4 (Switch)", "_sProfileUrl": "g" }
      ]
    })json";
    auto fetch = mapFetcher({
        { gb_game_search_url("Resident Evil 4 (Switch)", 1), EXACT } });
    GameResolve g = resolve_game(tid, "Resident Evil 4", fetch);
    REQUIRE(g.status == GameResolveStatus::Ok);
    CHECK(g.game_id == 18001);
}

TEST_CASE("list_game_mods returns the subfeed mods page") {
    auto fetch = mapFetcher({ { gb_subfeed_url(6170, "", 1), GAME_MODS_JSON } });
    BrowseResult r = list_game_mods(6170, "", 1, fetch);
    REQUIRE(r.status == BrowseStatus::Ok);
    REQUIRE(r.page.records.size() == 1);
    CHECK(r.page.records[0].id == 42);
    CHECK(r.page.records[0].name == "Ink Skin");
}

TEST_CASE("list_game_mods reports NetworkError on transport failure") {
    auto fetch = mapFetcher({});
    BrowseResult r = list_game_mods(6170, "", 1, fetch);
    CHECK(r.status == BrowseStatus::NetworkError);
}
