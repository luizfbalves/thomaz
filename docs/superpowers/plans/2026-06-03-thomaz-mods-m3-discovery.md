# thomaz Mods — Fase M3: Descoberta híbrida (resolve-by-name + override) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Descoberta "seus jogos → mods": ao abrir os mods de um jogo instalado, resolver automaticamente o game_id do GameBanana (override estático → senão busca por nome via apiv11) e listar os mods DAQUELE jogo (Game/{id}/Subfeed), com busca textual dentro do jogo e fallback para busca livre global (M2) quando o jogo não resolve.

**Architecture:** Reaproveita pesadamente o M2. O endpoint de busca de jogos e o Subfeed por jogo usam o MESMO envelope `{_aMetadata,_aRecords}` e os mesmos campos de record do M2 — então `core::parse_search_page` e `core::ModRecord` são reutilizados sem mudança para resolver jogos E listar mods por jogo. Novidades: 2 url builders, uma tabela de override estática (pura), e 2 funções de orquestração em `mod_browse` (resolver + listar por jogo). A UI altera a `ModBrowserActivity` do M2 para o modo "por jogo". Download/instalação continuam via M2/M1 (`ModDetailActivity` → `download_file` → `import_archive`).

**Tech Stack:** C++17, doctest, nlohmann/json, libcurl, Borealis. Sem backend novo (decisão: híbrido sem `api/`).

**Branch base:** Esta fase DEPENDE do código M2 (PR #6, ainda não merjado). Criar a branch a partir de `feat/mods-m2-gamebanana` (stack sobre M2). Quando M2 merjar em main, a PR do M3 mostra só o diff do M3.

**Decisão de descoberta (refina o spec):** o spec dizia "mapeamento curado no backend (B)". A pesquisa M3 (25/25 verificada) mostrou que NÃO existe mapa público title_id→game_id e que o tool de referência resolve por NOME. Escolha do usuário: **híbrido** — resolução por nome em runtime + uma pequena tabela de override estática (no código) para jogos ambíguos/que falham no name-match. Sem backend.

**API — fatos VERIFICADOS (live apiv11, jun/2026; + PoloNX/SimpleModDownloader game.cpp/mod.cpp):**
- **Resolver game_id por nome:** `GET /apiv11/Util/Search/Results?_sModelName=Game&_sOrder=best_match&_sSearchString=<nome>` (sufixar " (Switch)" para desambiguar). Envelope `{_aMetadata,_aRecords}`; cada record: `_idRow`(=game_id), `_sName`, `_sModelName="Game"`, `_sProfileUrl=/games/{id}`, `_sAbbreviation`. Escolher o primeiro record cujo `_sName` contém "Switch"; senão o primeiro (best_match).
- **Listar mods por jogo:** `GET /apiv11/Game/{id}/Subfeed?_nPage={}&_nPerpage=50&_csvModelInclusions=Mod` (o filtro `_csvModelInclusions=Mod` restringe o feed cru — que mistura Question/Thread/Mod/Wip/... — a só `_sModelName="Mod"`). Busca textual no jogo: adicionar `&_sName=<query>`. Mesmo envelope/record do M2.
- `game_id 6384` = hub de plataforma "Nintendo Switch" (NÃO um jogo). Jogos têm id próprio (BotW Switch = **6386**; Splatoon=6170, Splatoon2=6383, Splatoon3=15056).
- Não há filtro de plataforma dedicado; "(Switch)" é heurística.

## Escopo / YAGNI
- Override seedado APENAS com pares verificados (BotW title_id `01007EF00011E000` → game_id `6386`). Outros game_ids são conhecidos mas seus title_ids não foram verificados — NÃO seedar com dados não confirmados; deixar a tabela trivialmente extensível.
- Sem backend (decisão híbrida). Sem thumbnails (como M2). Resolução/listagem só no submit/abertura (rate limits desconhecidos).
- Reusar `ModDetailActivity` (M2) para download/instalação — M3 só muda a descoberta/listagem.

## File Structure
**Modificar (core, reaproveitando M2):**
- `source/core/mods/gamebanana_urls.hpp/.cpp` — add `gb_game_search_url`, `gb_subfeed_url`.
- `source/core/mods/mod_browse.hpp/.cpp` — add `resolve_game`, `list_game_mods` (+ tipos GameResolve).
**Criar (core):**
- `source/core/mods/gamebanana_overrides.hpp/.cpp` — tabela estática title_id→game_id.
- Tests: `tests/test_gamebanana_overrides.cpp`; extend `tests/test_gamebanana_urls.cpp`, `tests/test_mod_browse.cpp`.
**Modificar (app, draft — uncompilable no sandbox):**
- `source/app/mod_browser_activity.hpp/.cpp` — modo "por jogo" (resolve → list_game_mods → busca no jogo → fallback global).

`UrlFetcher`, `ModRecord`, `SearchPage`, `BrowseResult`/`BrowseStatus`, `parse_search_page` já existem (M2). Commit identity: `git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com'`.

---

### Task 1: URL builders (game search + subfeed)

**Files:** Modify `source/core/mods/gamebanana_urls.hpp/.cpp`; Test: extend `tests/test_gamebanana_urls.cpp`.

- [ ] **Step 1: Add failing tests** — append to tests/test_gamebanana_urls.cpp:
```cpp
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
```
(Note `(` → `%28`, `)` → `%29`, space → `%20` per the existing url_encode.)

- [ ] **Step 2: Run** `make -C tests test` → FAIL (gb_game_search_url/gb_subfeed_url undeclared).

- [ ] **Step 3: Implement** — add to gamebanana_urls.hpp (in namespace thomaz::core):
```cpp
// apiv11 game search by name to resolve a game_id (best_match ordering).
std::string gb_game_search_url(const std::string& query, int page);

// apiv11 per-game mod listing (Subfeed), filtered to the Mod model, 50/page.
// query empty => no _sName (full listing); non-empty => in-game text search.
std::string gb_subfeed_url(std::uint64_t game_id, const std::string& query, int page);
```
add to gamebanana_urls.cpp (in namespace thomaz::core, using the existing `kBase` + `url_encode`):
```cpp
std::string gb_game_search_url(const std::string& query, int page) {
    return std::string(kBase) +
           "/Util/Search/Results?_sModelName=Game&_sOrder=best_match&_sSearchString=" +
           url_encode(query) + "&_nPage=" + std::to_string(page);
}

std::string gb_subfeed_url(std::uint64_t game_id, const std::string& query, int page) {
    std::string u = std::string(kBase) + "/Game/" + std::to_string(game_id) +
                    "/Subfeed?_nPage=" + std::to_string(page) +
                    "&_nPerpage=50&_csvModelInclusions=Mod";
    if (!query.empty())
        u += "&_sName=" + url_encode(query);
    return u;
}
```

- [ ] **Step 4: Run** `make -C tests test` → PASS.
- [ ] **Step 5: Commit**
```bash
git add source/core/mods/gamebanana_urls.hpp source/core/mods/gamebanana_urls.cpp tests/test_gamebanana_urls.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): apiv11 game-search + per-game Subfeed url builders"
```

---

### Task 2: Static game-id override table

**Files:** Create `source/core/mods/gamebanana_overrides.hpp/.cpp`, `tests/test_gamebanana_overrides.cpp`.

- [ ] **Step 1: Failing test** — tests/test_gamebanana_overrides.cpp:
```cpp
#include "doctest.h"
#include "core/mods/gamebanana_overrides.hpp"

using namespace thomaz::core;

TEST_CASE("override returns the curated game_id for a seeded title") {
    // The Legend of Zelda: Breath of the Wild (Switch) -> GameBanana game 6386
    auto id = gamebanana_game_override(0x01007EF00011E000ULL);
    REQUIRE(id.has_value());
    CHECK(*id == 6386);
}

TEST_CASE("override returns nullopt for an unmapped title") {
    CHECK_FALSE(gamebanana_game_override(0x0123456789ABCDEFULL).has_value());
}
```

- [ ] **Step 2: Run** `make -C tests test` → FAIL (missing header).

- [ ] **Step 3: Implement** — gamebanana_overrides.hpp:
```cpp
#pragma once
#include <cstdint>
#include <optional>

namespace thomaz::core {

// Static title_id -> GameBanana game_id override, for games whose GameBanana
// page name doesn't match the NACP name (or to skip name resolution entirely).
// Returns nullopt when the title isn't in the table (caller then resolves by
// name via apiv11). Extend kOverrides as (title_id, game_id) pairs are verified.
std::optional<std::uint64_t> gamebanana_game_override(std::uint64_t title_id);

} // namespace thomaz::core
```
gamebanana_overrides.cpp:
```cpp
#include "core/mods/gamebanana_overrides.hpp"

namespace thomaz::core {

namespace {
struct Entry {
    std::uint64_t title_id;
    std::uint64_t game_id;
};
// Only VERIFIED pairs. Known GameBanana game_ids whose Switch title_ids still
// need confirming before adding here: Splatoon=6170, Splatoon 2=6383,
// Splatoon 3=15056. (game_id 6384 is the platform hub, NOT a game.)
constexpr Entry kOverrides[] = {
    { 0x01007EF00011E000ULL, 6386ULL }, // The Legend of Zelda: Breath of the Wild
};
} // namespace

std::optional<std::uint64_t> gamebanana_game_override(std::uint64_t title_id) {
    for (const Entry& e : kOverrides)
        if (e.title_id == title_id)
            return e.game_id;
    return std::nullopt;
}

} // namespace thomaz::core
```

- [ ] **Step 4: Run** `make -C tests test` → PASS.
- [ ] **Step 5: Commit**
```bash
git add source/core/mods/gamebanana_overrides.hpp source/core/mods/gamebanana_overrides.cpp tests/test_gamebanana_overrides.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): static title_id->gamebanana game_id override table"
```

---

### Task 3: Resolver + per-game listing (mod_browse)

**Files:** Modify `source/core/mods/mod_browse.hpp/.cpp`; Test: extend `tests/test_mod_browse.cpp`.

- [ ] **Step 1: Failing tests** — append to tests/test_mod_browse.cpp (reuses the existing `mapFetcher` helper + includes; add `#include "core/mods/gamebanana_urls.hpp"` if not present):
```cpp
static const char* GAME_SEARCH_JSON = R"({
  "_aMetadata": { "_nRecordCount": 2, "_nPerpage": 15, "_bIsComplete": true },
  "_aRecords": [
    { "_idRow": 999, "_sModelName": "Game", "_sName": "Splatoon", "_sProfileUrl": "g" },
    { "_idRow": 6170, "_sModelName": "Game", "_sName": "Splatoon (Switch)", "_sProfileUrl": "g" }
  ]
})";
static const char* GAME_MODS_JSON = R"({
  "_aMetadata": { "_nRecordCount": 1, "_nPerpage": 50, "_bIsComplete": true },
  "_aRecords": [ { "_idRow": 42, "_sModelName": "Mod", "_sName": "Ink Skin",
                   "_sProfileUrl": "m", "_bHasFiles": true } ]
})";

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
```

- [ ] **Step 2: Run** `make -C tests test` → FAIL (resolve_game/list_game_mods/GameResolve undeclared).

- [ ] **Step 3: Implement** — add to mod_browse.hpp (namespace thomaz::core):
```cpp
enum class GameResolveStatus { Ok, NotFound, NetworkError };
enum class GameResolveSource { Override, NameMatch };
struct GameResolve {
    GameResolveStatus status = GameResolveStatus::NetworkError;
    std::uint64_t game_id = 0;
    std::string matched_name;
    GameResolveSource source = GameResolveSource::NameMatch;
};

// Resolve a Switch game to a GameBanana game_id: static override first, then a
// name search (apiv11), preferring the result whose name contains "Switch".
GameResolve resolve_game(std::uint64_t title_id, const std::string& name,
                         const UrlFetcher& fetch);

// List a resolved game's mods via Game/{id}/Subfeed (Mod-filtered). query
// empty => full listing; non-empty => in-game text search. Reuses SearchPage.
BrowseResult list_game_mods(std::uint64_t game_id, const std::string& query,
                            int page, const UrlFetcher& fetch);
```
add to mod_browse.cpp (add includes `#include "core/mods/gamebanana_overrides.hpp"` and ensure gamebanana_urls.hpp/gamebanana_json.hpp are included):
```cpp
GameResolve resolve_game(std::uint64_t title_id, const std::string& name,
                         const UrlFetcher& fetch) {
    GameResolve r;
    if (auto ov = gamebanana_game_override(title_id)) {
        r.status = GameResolveStatus::Ok;
        r.game_id = *ov;
        r.matched_name = name;
        r.source = GameResolveSource::Override;
        return r;
    }
    std::optional<std::string> body = fetch(gb_game_search_url(name + " (Switch)", 1));
    if (!body) {
        r.status = GameResolveStatus::NetworkError;
        return r;
    }
    SearchPage page = parse_search_page(*body);
    if (page.records.empty()) {
        r.status = GameResolveStatus::NotFound;
        return r;
    }
    const ModRecord* chosen = &page.records[0];
    for (const ModRecord& rec : page.records) {
        if (rec.name.find("Switch") != std::string::npos) {
            chosen = &rec;
            break;
        }
    }
    r.status = GameResolveStatus::Ok;
    r.game_id = chosen->id;
    r.matched_name = chosen->name;
    r.source = GameResolveSource::NameMatch;
    return r;
}

BrowseResult list_game_mods(std::uint64_t game_id, const std::string& query,
                            int page, const UrlFetcher& fetch) {
    BrowseResult result;
    std::optional<std::string> body = fetch(gb_subfeed_url(game_id, query, page));
    if (!body) {
        result.status = BrowseStatus::NetworkError;
        return result;
    }
    result.page = parse_search_page(*body);
    result.status = BrowseStatus::Ok;
    return result;
}
```

- [ ] **Step 4: Run** `make -C tests test` → PASS.
- [ ] **Step 5: Commit**
```bash
git add source/core/mods/mod_browse.hpp source/core/mods/mod_browse.cpp tests/test_mod_browse.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): resolve game_id (override->name) + per-game mod listing"
```

---

### Task 4: ModBrowserActivity "per-game" mode — DRAFT

> UNCOMPILABLE in the sandbox — mirror the M2 `mod_browser_activity.cpp` patterns EXACTLY (async+alive, brls::sync deferral, IHttpClient fetcher, keyboard via getImeManager). CI compiles the .nro. Read source/app/mod_browser_activity.cpp (M2) and source/app/cheat_detail_activity.cpp first.

**Files:** Modify `source/app/mod_browser_activity.hpp/.cpp`. (i18n: add keys below.)

- [ ] **Step 1: i18n** — add to BOTH resources/i18n/{en-US,pt-BR}/mods.json (valid JSON; localize):
  - en: `"resolving": "Finding this game on GameBanana…"`, `"game_not_found": "This game isn't on GameBanana. Search manually:"`, `"game_search": "Search within this game"`
  - pt-BR: `"resolving": "Procurando este jogo no GameBanana…"`, `"game_not_found": "Este jogo não está no GameBanana. Busque manualmente:"`, `"game_search": "Buscar neste jogo"`
  Validate with `python3 -c "import json;json.load(open('resources/i18n/pt-BR/mods.json'));json.load(open('resources/i18n/en-US/mods.json'));print('JSON OK')"`.

- [ ] **Step 2: Header** — add members to ModBrowserActivity: `std::uint64_t gameId = 0;` (0 = unresolved/global fallback). Keep existing query/page/lastPage/alive/http/title.

- [ ] **Step 3: Implementation** — change onContentAvailable to resolve-then-list:
  - `onContentAvailable()`: `install_header_username(this);` show spinner; `brls::async([this, alive=this->alive, http=this->http, tid=this->title.title_id, name=this->title.name]{ auto fetch = <IHttpClient fetcher as in M2>; core::GameResolve g = core::resolve_game(tid, name, fetch); core::BrowseResult mods; if (g.status==core::GameResolveStatus::Ok) mods = core::list_game_mods(g.game_id, "", 1, fetch); brls::sync([this, alive, g, mods]{ if(!alive->load()) return; this->onResolved(g, mods); }); });`
  - `onResolved(const core::GameResolve& g, const core::BrowseResult& mods)`: hide spinner.
    - g.status==NetworkError → notify "mods/search_error"_i18n.
    - g.status==NotFound → store gameId=0; show emptyLabel "mods/game_not_found"_i18n AND open the keyboard for GLOBAL free-text search (the M2 path: openForText → this->query → runGlobalSearch()). (This is the fallback.)
    - g.status==Ok → store this->gameId=g.game_id; if mods.status!=Ok → notify error; else populate the mods list (reuse the M2 row-builder: name + likes rows → push ModDetailActivity(title, rec.id, http)). Add an in-game search button row ("mods/game_search"_i18n) that opens the keyboard and runs an in-game search.
  - `runGameSearch(query)`: spinner; brls::async → list_game_mods(this->gameId, query, 1, fetch) → brls::sync → populate. (Deferred rebuild discipline: any rebuild called from inside a click/keyboard handler goes through brls::sync — but keyboard callbacks on Switch fire synchronously; still wrap list rebuilds triggered from row/button clicks in brls::sync as M2 does.)
  - Keep the M2 global-search code path as `runGlobalSearch()` (the old runSearch) for the NotFound fallback.
  - REUSE the existing populate(BrowseResult) row-builder from M2 for both game-mods and global results (it already builds rows from SearchPage.records and pushes ModDetailActivity). Only the data source differs.
  - Off-thread safety (M2 lesson): capture title_id/name/http BY VALUE into async workers; never deref `this` on the worker; touch UI only inside brls::sync after `alive->load()`.

- [ ] **Step 4** — Self-review every brls call against M2's mod_browser_activity.cpp. `make -C tests test` stays green (122+). Commit:
```bash
git add source/app/mod_browser_activity.hpp source/app/mod_browser_activity.cpp resources/i18n/en-US/mods.json resources/i18n/pt-BR/mods.json
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): per-game discovery in the browser (resolve+Subfeed, global fallback) (draft)"
```

---

### Task 5: Live-API smoke + CI build

- [ ] **Step 1: Live smoke** (host, real apiv11) — throwaway harness exercising resolve_game (name path) + list_game_mods:
```cpp
// tests/smoke_m3.cpp
#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_urls.hpp"
#include <curl/curl.h>
#include <cstdio>
using namespace thomaz::core;
static size_t w(char*p,size_t s,size_t n,void*u){((std::string*)u)->append(p,s*n);return s*n;}
static UrlFetcher cf(){return [](const std::string&url)->std::optional<std::string>{CURL*c=curl_easy_init();std::string b;if(!c)return std::nullopt;curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(c,CURLOPT_USERAGENT,"thomaz/0.1");curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,0L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,0L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,w);curl_easy_setopt(c,CURLOPT_WRITEDATA,&b);CURLcode rc=curl_easy_perform(c);curl_easy_cleanup(c);if(rc!=CURLE_OK)return std::nullopt;return b;};}
int main(){
    auto f=cf();
    // name-resolve a known Switch game (not relying on the override)
    GameResolve g=resolve_game(0x0100000000099999ULL, "Splatoon 3", f);
    std::printf("resolve status=%d source=%d game_id=%llu matched=\"%s\"\n",(int)g.status,(int)g.source,(unsigned long long)g.game_id,g.matched_name.c_str());
    if(g.status!=GameResolveStatus::Ok) return 1;
    BrowseResult r=list_game_mods(g.game_id,"",1,f);
    std::printf("list status=%d total=%llu got=%zu per_page=%d\n",(int)r.status,(unsigned long long)r.page.total,r.page.records.size(),r.page.per_page);
    if(!r.page.records.empty()) std::printf("first mod: id=%llu name=\"%s\" model=%s\n",(unsigned long long)r.page.records[0].id,r.page.records[0].name.c_str(),r.page.records[0].model.c_str());
    // override path
    GameResolve o=resolve_game(0x01007EF00011E000ULL,"BotW",f);
    std::printf("override status=%d source=%d game_id=%llu\n",(int)o.status,(int)o.source,(unsigned long long)o.game_id);
    return 0;
}
```
Build + run:
```bash
g++ -std=c++17 -I source -I lib/json $(curl-config --cflags) tests/smoke_m3.cpp \
  source/core/mods/gamebanana_urls.cpp source/core/mods/gamebanana_json.cpp \
  source/core/mods/gamebanana_overrides.cpp source/core/mods/mod_browse.cpp \
  $(curl-config --libs) -o /tmp/smoke_m3 && /tmp/smoke_m3
rm -f /tmp/smoke_m3 tests/smoke_m3.cpp
```
EXPECTED: resolve status=0 source=1(NameMatch) with a real game_id and matched name containing "Switch"; list status=0 with mods (model=Mod); override status=0 source=0(Override) game_id=6386. Record output. If apiv11 shape differs, fix the parser/resolver. No commit from this step.

- [ ] **Step 2: Push + CI** — push the branch; watch the `Nintendo Switch (.nro)` CI run to green (`gh run watch <id> --exit-status`). Fix any compile/link error (the app layer is only verified here). 

---

### Task 6: Hardware checklist + docs

- [ ] On hardware (Atmosphère, online): open Mods → pick an installed game that IS on GameBanana (e.g. BotW via the override, or any name-resolvable game) → the browser auto-lists THAT game's mods → in-game search filters → open a mod → download → installs → enable → LayeredFS applies.
- [ ] A game NOT on GameBanana → "this game isn't on GameBanana" + manual global search fallback works.
- [ ] Update README roadmap (mods: GameBanana per-game discovery M3) and add a couple verified title_id→game_id pairs to kOverrides as you confirm them on hardware.

---

## Self-Review
**Spec coverage (M3):** game-id resolution (Task 2 override + Task 3 name-match), per-game listing + in-game search (Task 1 urls + Task 3 list_game_mods), hybrid discovery UX with global fallback (Task 4), live + CI verification (Task 5), hardware (Task 6). Reuses M2 `parse_search_page`/`ModRecord`/`ModDetailActivity` — no duplicate parser. Decision recorded: hybrid (name + static override), no backend — refines the spec's "B". ✅
**Placeholders:** core tasks (1–3) full code + tests; UI (Task 4) is draft-with-analog-guidance (uncompilable here, CI verifies), with the fixed contract calls (`resolve_game`, `list_game_mods`, `ModDetailActivity`, `getImeManager()->openForText`) specified. ✅
**Type consistency:** `GameResolve`/`GameResolveStatus`/`GameResolveSource` (Task 3) used in 4; `gb_game_search_url`/`gb_subfeed_url` (Task 1) used in 3; `gamebanana_game_override` (Task 2) used in 3; `BrowseResult`/`SearchPage`/`ModRecord`/`parse_search_page` reused from M2. Signatures match across def/use. ✅
**Risks flagged:** name-match heuristic ("(Switch)" suffix + first record containing "Switch") can mismatch/miss → override table + global-search fallback cover it; rate limits unknown → resolve/list only on open/submit; game_id longevity inferred not longitudinally tested → override table is editable; the branch stacks on M2 (PR #6) — its PR shows M2+M3 until #6 merges (then just M3), same as the feed-removal split. UI is an uncompiled draft (CI-verified).
