#include "doctest.h"
#include "core/mods/gamebanana_urls.hpp"

using namespace thomaz::core;

TEST_CASE("url_encode percent-encodes spaces and reserved chars, keeps unreserved") {
    CHECK(url_encode("mario kart") == "mario%20kart");
    CHECK(url_encode("a+b&c") == "a%2Bb%26c");
    CHECK(url_encode("Zelda_2-v1.0") == "Zelda_2-v1.0"); // unreserved kept
}

TEST_CASE("gb_search_url builds the apiv11 Util/Search/Results query") {
    CHECK(gb_search_url("mario kart", 0, 1) ==
          "https://gamebanana.com/apiv11/Util/Search/Results"
          "?_sSearchString=mario%20kart&_sModelName=Mod&_idGameRow=&_nPage=1");
}

TEST_CASE("gb_search_url includes the game id when nonzero") {
    CHECK(gb_search_url("skin", 8694, 2) ==
          "https://gamebanana.com/apiv11/Util/Search/Results"
          "?_sSearchString=skin&_sModelName=Mod&_idGameRow=8694&_nPage=2");
}

TEST_CASE("gb_mod_files_url requests only the _aFiles property") {
    CHECK(gb_mod_files_url(682977) ==
          "https://gamebanana.com/apiv11/Mod/682977?_csvProperties=_aFiles");
}

TEST_CASE("gb_game_search_url builds the apiv11 Game search query") {
    CHECK(gb_game_search_url("Splatoon (Switch)", 1) ==
          "https://gamebanana.com/apiv11/Util/Search/Results"
          "?_sModelName=Game&_sOrder=best_match&_sSearchString=Splatoon%20%28Switch%29&_nPage=1");
}

TEST_CASE("gb_subfeed_url lists a game's mods, Mod-filtered, 50 per page") {
    CHECK(gb_subfeed_url(6170, "", 2) ==
          "https://gamebanana.com/apiv11/Game/6170/Subfeed"
          "?_nPage=2&_nPerpage=50&_csvModelInclusions=Mod");
}

TEST_CASE("gb_subfeed_url adds _sName for in-game text search") {
    CHECK(gb_subfeed_url(6170, "ink skin", 1) ==
          "https://gamebanana.com/apiv11/Game/6170/Subfeed"
          "?_nPage=1&_nPerpage=50&_csvModelInclusions=Mod&_sName=ink%20skin");
}
