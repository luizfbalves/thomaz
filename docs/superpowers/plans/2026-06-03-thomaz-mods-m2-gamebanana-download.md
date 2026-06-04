# thomaz Mods — Fase M2: Download do GameBanana (busca livre) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Buscar mods no GameBanana por texto dentro do app, resolver a URL de download de um arquivo, baixá-lo em streaming com progresso, e instalá-lo via o pipeline M1 (extração → staging → enable) — entrando por um jogo instalado.

**Architecture:** Mesmo split de M1: `core/mods/` (parsing + orquestração puros, host-testáveis com fixtures e um `UrlFetcher` injetado, idêntico ao padrão de `fetch_cheat_set`), `platform/mods/` (download em streaming com libcurl), `app/` (telas Borealis de busca e detalhe — draft, como em M1). O download alimenta o `import_archive` de M1, então M2 reaproveita toda a cadeia de instalação já testada.

**Tech Stack:** C++17, doctest (host), nlohmann/json (`lib/json`), libcurl (já linkado), Borealis (UI).

**API — fatos VERIFICADOS (live-fetch, jun/2026; fontes primárias apiv11 + PoloNX/SimpleModDownloader):**
- Base: **`https://gamebanana.com/apiv11`** (NÃO o subdomínio `api.`; aquele é a Core API legada, sem busca full-text). Use apiv11.
- **Busca livre:** `GET /apiv11/Util/Search/Results?_sSearchString={q}&_sModelName=Mod&_idGameRow={gameId|vazio}&_nPage={n}`
- **Listagem por jogo (M3, não usada aqui):** `GET /apiv11/Game/{id}/Subfeed`
- **Resposta de busca/listagem:** objeto `{ "_aMetadata": {"_nRecordCount":N, "_nPerpage":15, "_bIsComplete":bool}, "_aRecords": [ ... ] }` — NUNCA um array nu. Ler `_nPerpage` da resposta (não hardcodar).
- **Record:** `_idRow` (num), `_sName`, `_sModelName` ("Mod"/"Wip"/...), `_sProfileUrl`, `_bHasFiles` (bool), contagens `_nLikeCount`/`_nViewCount`/`_nPostCount` (**OPCIONAIS** — omitidas quando zero). Busca **NÃO** traz download URL nem `_aFiles`.
- **Resolver download (2º request):** `GET /apiv11/Mod/{idRow}?_csvProperties=_aFiles` → `_aFiles[]` com `_idRow`, `_sFile` (filename), `_nFilesize`, `_sMd5Checksum`, `_sDownloadUrl` = `https://gamebanana.com/dl/{file_idRow}`.
- **Erro de record:** quando `_csvProperties` é passado, um mod inexistente volta **HTTP 200** com corpo `{"_sErrorCode":"NO_SUCH_RECORD","_sErrorMessage":"..."}` → o parser DEVE checar `_sErrorCode` no corpo, não só o status HTTP.
- **/dl/{id}:** o `CurlHttpClient` já usa `CURLOPT_FOLLOWLOCATION=1` e pula verificação TLS — então o redirect do /dl é seguido e não precisa de CA bundle. (Confirmar binário vs interstício no smoke da Task 8.)
- **Rate limits/auth:** NÃO documentados. Tratar como desconhecido: buscar só no submit do teclado (naturalmente "debounced"), não a cada tecla; sem polling.

**Escopo desta fase:** M2 apenas. M3 (mapeamento curado `title_id→gamebanana_game_id` + descoberta híbrida "seus jogos → mods" + listagem por Subfeed) é um plano separado. Esta correção de API (apiv11, não Core) supersede a menção a "Core API" no spec `2026-06-03-thomaz-mods-feature-design.md`.

---

## Decisões de escopo (YAGNI para o MVP M2)
- **Thumbnails NÃO** no MVP: cada thumb é um request remoto extra (N imagens = N downloads) e a construção da URL de preview é incerta. Mostrar nome + contagens + "tem arquivos". Thumbs ficam para depois.
- **Entrada por jogo:** o usuário abre Mods → escolhe um jogo (lista de M1, modo Mods) → na tela do jogo, botão "Baixar mods (GameBanana)" → busca livre por texto. O mod baixado é instalado no staging **daquele** jogo (`title.title_id`), reaproveitando `import_archive` de M1. Busca livre é global (sem filtro por jogo) no MVP; o filtro por jogo (`_idGameRow`) vem com o mapeamento de M3.
- **Sem cache de busca** no MVP (resultados mudam; rate limit desconhecido → só busca no submit).

## File Structure

**Criar (core, puro, host-testado):**
- `source/core/mods/gamebanana_types.hpp` — `ModRecord`, `SearchPage`, `ModFile`, result structs.
- `source/core/mods/gamebanana_urls.hpp`/`.cpp` — builders de URL + `url_encode`.
- `source/core/mods/gamebanana_json.hpp`/`.cpp` — `parse_search_page`, `parse_mod_files` (com caso de erro `_sErrorCode`).
- `source/core/mods/mod_browse.hpp`/`.cpp` — orquestração com `UrlFetcher` injetado.
- `tests/test_gamebanana_urls.cpp`, `tests/test_gamebanana_json.cpp`, `tests/test_mod_browse.cpp`.

**Criar (platform):**
- `source/platform/mods/mod_download.hpp`/`.cpp` — download streaming-para-arquivo com progresso (libcurl). Fora do build de testes.

**Criar (app — draft, não compilável no sandbox):**
- `source/app/mod_browser_activity.hpp`/`.cpp`, `source/app/mod_detail_activity.hpp`/`.cpp`.
- `resources/xml/activity/mod_browser.xml`, `resources/xml/activity/mod_detail.xml` + entradas no allowlist do `.gitignore`.

**Modificar:**
- `tests/Makefile` — core/mods já é globbed; sem mudança necessária (confirmar). Adicionar `../source/platform/mods/mod_download.cpp`? NÃO — depende de libcurl, fora do build de testes (igual aos extractors de M1).
- `source/app/mod_manager_activity.cpp` — botão "Baixar mods" → push `ModBrowserActivity(title)`.
- `resources/i18n/en-US/mods.json` + `pt-BR/mods.json` — novas chaves (arquivo já é tracked/limpo; pode editar à vontade).
- `.gitignore` — un-ignore dos 2 novos XML (allowlist em `/resources/**`).

`UrlFetcher` reusa o typedef de `core/cheat_repository.hpp`: `std::function<std::optional<std::string>(const std::string& url)>` (corpo no sucesso; `nullopt` só em falha de transporte). Mesma semântica de `fetch_cheat_set`.

---

### Task 1: Tipos + URLs do GameBanana (`core/mods/gamebanana_urls`)

**Files:**
- Create: `source/core/mods/gamebanana_types.hpp`, `source/core/mods/gamebanana_urls.hpp`, `source/core/mods/gamebanana_urls.cpp`
- Test: `tests/test_gamebanana_urls.cpp`

- [ ] **Step 1: Criar os tipos puros**

Create `source/core/mods/gamebanana_types.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

// One mod as returned by an apiv11 search/listing record (_aRecords[]).
struct ModRecord {
    std::uint64_t id = 0;        // _idRow
    std::string name;            // _sName
    std::string model;           // _sModelName ("Mod", "Wip", ...)
    std::string profile_url;     // _sProfileUrl
    bool has_files = false;      // _bHasFiles
    std::uint32_t likes = 0;     // _nLikeCount (optional in JSON -> 0)
    std::uint32_t views = 0;     // _nViewCount (optional in JSON -> 0)
};

// One page of search/listing results (apiv11 {_aMetadata, _aRecords}).
struct SearchPage {
    std::vector<ModRecord> records;
    std::uint64_t total = 0;     // _aMetadata._nRecordCount
    int per_page = 0;            // _aMetadata._nPerpage (read from response)
    bool is_complete = true;     // _aMetadata._bIsComplete (false => more pages)
};

// One downloadable file of a mod (from _aFiles[]).
struct ModFile {
    std::uint64_t file_id = 0;   // _idRow
    std::string filename;        // _sFile
    std::uint64_t filesize = 0;  // _nFilesize (bytes)
    std::string md5;             // _sMd5Checksum
    std::string download_url;    // _sDownloadUrl (https://gamebanana.com/dl/<file_id>)
};

} // namespace thomaz::core
```

- [ ] **Step 2: Escrever o teste que falha (URLs)**

Create `tests/test_gamebanana_urls.cpp`:

```cpp
#include "doctest.h"
#include "core/mods/gamebanana_urls.hpp"

using namespace thomaz::core;

TEST_CASE("url_encode percent-encodes spaces and reserved chars, keeps unreserved") {
    CHECK(url_encode("mario kart") == "mario%20kart");
    CHECK(url_encode("a+b&c") == "a%2Bb%26c");
    CHECK(url_encode("Zelda_2-v1.0") == "Zelda_2-v1.0"); // unreserved kept
}

TEST_CASE("gb_search_url builds the apiv11 Util/Search/Results query") {
    // game_id 0 -> empty _idGameRow (global search)
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
```

- [ ] **Step 3: Confirmar que o build de testes inclui core/mods**

The tests/Makefile SRCS already globs `$(wildcard ../source/core/mods/*.cpp)` (added in M1). Run `grep -n 'core/mods' tests/Makefile` — confirm the wildcard is present. No edit needed. (If somehow absent, add `$(wildcard ../source/core/mods/*.cpp)` after the core wildcard.)

- [ ] **Step 4: Rodar o teste e confirmar que falha**

Run: `make -C tests test`
Expected: FAIL — `core/mods/gamebanana_urls.hpp: No such file or directory`.

- [ ] **Step 5: Implementar gamebanana_urls**

Create `source/core/mods/gamebanana_urls.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>

namespace thomaz::core {

// Percent-encode `s` per RFC 3986 (unreserved A-Za-z0-9-_.~ kept; everything
// else %XX). Used for the search query string.
std::string url_encode(const std::string& s);

// apiv11 free-text mod search. game_id==0 => global (empty _idGameRow).
std::string gb_search_url(const std::string& query, std::uint64_t game_id, int page);

// apiv11 per-mod fetch of just the file list (download URLs live here).
std::string gb_mod_files_url(std::uint64_t mod_id);

} // namespace thomaz::core
```

Create `source/core/mods/gamebanana_urls.cpp`:

```cpp
#include "core/mods/gamebanana_urls.hpp"

namespace thomaz::core {

namespace {
const char* kBase = "https://gamebanana.com/apiv11";

bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}
} // namespace

std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (is_unreserved(c)) {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

std::string gb_search_url(const std::string& query, std::uint64_t game_id, int page) {
    std::string game = game_id == 0 ? std::string() : std::to_string(game_id);
    return std::string(kBase) + "/Util/Search/Results?_sSearchString=" +
           url_encode(query) + "&_sModelName=Mod&_idGameRow=" + game +
           "&_nPage=" + std::to_string(page);
}

std::string gb_mod_files_url(std::uint64_t mod_id) {
    return std::string(kBase) + "/Mod/" + std::to_string(mod_id) +
           "?_csvProperties=_aFiles";
}

} // namespace thomaz::core
```

- [ ] **Step 6: Rodar o teste e confirmar que passa**

Run: `make -C tests test` → expected: PASS (URLs verdes; suíte existente intacta).

- [ ] **Step 7: Commit**

```bash
git add source/core/mods/gamebanana_types.hpp source/core/mods/gamebanana_urls.hpp source/core/mods/gamebanana_urls.cpp tests/test_gamebanana_urls.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): GameBanana apiv11 url builders + types"
```

---

### Task 2: Parsers de JSON apiv11 (`core/mods/gamebanana_json`)

**Files:**
- Create: `source/core/mods/gamebanana_json.hpp`, `source/core/mods/gamebanana_json.cpp`
- Test: `tests/test_gamebanana_json.cpp`

- [ ] **Step 1: Escrever o teste que falha (parsers, com fixtures reais)**

Create `tests/test_gamebanana_json.cpp` (fixtures mirror the verified apiv11 shapes — note the OPTIONAL `_nLikeCount`/`_nViewCount` and the HTTP-200 error body):

```cpp
#include "doctest.h"
#include "core/mods/gamebanana_json.hpp"

using namespace thomaz::core;

static const char* SEARCH_JSON = R"({
  "_aMetadata": { "_nRecordCount": 158197, "_nPerpage": 15, "_bIsComplete": false },
  "_aRecords": [
    { "_idRow": 445087, "_sModelName": "Mod", "_sName": "Cool Skin",
      "_sProfileUrl": "https://gamebanana.com/mods/445087",
      "_bHasFiles": true, "_nLikeCount": 12, "_nViewCount": 3400 },
    { "_idRow": 999, "_sModelName": "Mod", "_sName": "Zero Likes Mod",
      "_sProfileUrl": "https://gamebanana.com/mods/999",
      "_bHasFiles": false }
  ]
})";

static const char* FILES_JSON = R"({
  "_aFiles": [
    { "_idRow": 1719374, "_sFile": "escambio.zip", "_nFilesize": 1048576,
      "_sMd5Checksum": "abc123", "_sDownloadUrl": "https://gamebanana.com/dl/1719374" },
    { "_idRow": 1719375, "_sFile": "extra.7z", "_nFilesize": 2048,
      "_sMd5Checksum": "def456", "_sDownloadUrl": "https://gamebanana.com/dl/1719375" }
  ]
})";

static const char* FILES_ERROR_JSON = R"({
  "_sErrorCode": "NO_SUCH_RECORD", "_sErrorMessage": "This Mod doesn't exist"
})";

TEST_CASE("parse_search_page reads metadata and records") {
    SearchPage p = parse_search_page(SEARCH_JSON);
    CHECK(p.total == 158197);
    CHECK(p.per_page == 15);
    CHECK(p.is_complete == false);
    REQUIRE(p.records.size() == 2);
    CHECK(p.records[0].id == 445087);
    CHECK(p.records[0].name == "Cool Skin");
    CHECK(p.records[0].has_files == true);
    CHECK(p.records[0].likes == 12);
    CHECK(p.records[0].views == 3400);
}

TEST_CASE("parse_search_page defaults optional counts to zero") {
    SearchPage p = parse_search_page(SEARCH_JSON);
    REQUIRE(p.records.size() == 2);
    CHECK(p.records[1].likes == 0);   // _nLikeCount absent
    CHECK(p.records[1].views == 0);   // _nViewCount absent
    CHECK(p.records[1].has_files == false);
}

TEST_CASE("parse_search_page tolerates malformed json (empty page)") {
    SearchPage p = parse_search_page("not json");
    CHECK(p.records.empty());
    CHECK(p.total == 0);
}

TEST_CASE("parse_mod_files reads the file list") {
    ModFilesResult r = parse_mod_files(FILES_JSON);
    REQUIRE(r.ok);
    REQUIRE(r.files.size() == 2);
    CHECK(r.files[0].file_id == 1719374);
    CHECK(r.files[0].filename == "escambio.zip");
    CHECK(r.files[0].filesize == 1048576);
    CHECK(r.files[0].md5 == "abc123");
    CHECK(r.files[0].download_url == "https://gamebanana.com/dl/1719374");
}

TEST_CASE("parse_mod_files surfaces the HTTP-200 error body") {
    ModFilesResult r = parse_mod_files(FILES_ERROR_JSON);
    CHECK_FALSE(r.ok);
    CHECK(r.error == "This Mod doesn't exist");
    CHECK(r.files.empty());
}

TEST_CASE("parse_mod_files on malformed json fails cleanly") {
    ModFilesResult r = parse_mod_files("{");
    CHECK_FALSE(r.ok);
    CHECK(r.files.empty());
}
```

- [ ] **Step 2: Rodar e confirmar que falha**

Run: `make -C tests test` → FAIL: missing `core/mods/gamebanana_json.hpp`.

- [ ] **Step 3: Implementar gamebanana_json**

Create `source/core/mods/gamebanana_json.hpp`:

```cpp
#pragma once
#include "core/mods/gamebanana_types.hpp"
#include <string>

namespace thomaz::core {

// Parse an apiv11 search/listing response ({_aMetadata, _aRecords}). On
// malformed input returns an empty SearchPage (records empty, total 0).
SearchPage parse_search_page(const std::string& json);

struct ModFilesResult {
    bool ok = false;
    std::string error;            // _sErrorMessage when the body is an error
    std::vector<ModFile> files;
};

// Parse an apiv11 Mod/{id}?_csvProperties=_aFiles response. If the body carries
// an _sErrorCode (HTTP 200 error) or is malformed, returns ok=false.
ModFilesResult parse_mod_files(const std::string& json);

} // namespace thomaz::core
```

Create `source/core/mods/gamebanana_json.cpp` (uses nlohmann/json from `lib/json`, like `core/cheat_db.cpp`):

```cpp
#include "core/mods/gamebanana_json.hpp"

#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

SearchPage parse_search_page(const std::string& body) {
    SearchPage page;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object())
        return page;

    if (doc.contains("_aMetadata") && doc["_aMetadata"].is_object()) {
        const json& m = doc["_aMetadata"];
        page.total       = m.value("_nRecordCount", (std::uint64_t)0);
        page.per_page    = m.value("_nPerpage", 0);
        page.is_complete = m.value("_bIsComplete", true);
    }

    if (doc.contains("_aRecords") && doc["_aRecords"].is_array()) {
        for (const json& r : doc["_aRecords"]) {
            if (!r.is_object())
                continue;
            ModRecord rec;
            rec.id          = r.value("_idRow", (std::uint64_t)0);
            rec.name        = r.value("_sName", std::string());
            rec.model       = r.value("_sModelName", std::string());
            rec.profile_url = r.value("_sProfileUrl", std::string());
            rec.has_files   = r.value("_bHasFiles", false);
            rec.likes       = r.value("_nLikeCount", (std::uint32_t)0); // optional
            rec.views       = r.value("_nViewCount", (std::uint32_t)0); // optional
            page.records.push_back(std::move(rec));
        }
    }
    return page;
}

ModFilesResult parse_mod_files(const std::string& body) {
    ModFilesResult result;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object())
        return result; // ok=false

    // apiv11 returns HTTP 200 with an error body when _csvProperties is set.
    if (doc.contains("_sErrorCode")) {
        result.error = doc.value("_sErrorMessage", std::string("error"));
        return result; // ok=false
    }

    if (doc.contains("_aFiles") && doc["_aFiles"].is_array()) {
        for (const json& f : doc["_aFiles"]) {
            if (!f.is_object())
                continue;
            ModFile mf;
            mf.file_id      = f.value("_idRow", (std::uint64_t)0);
            mf.filename     = f.value("_sFile", std::string());
            mf.filesize     = f.value("_nFilesize", (std::uint64_t)0);
            mf.md5          = f.value("_sMd5Checksum", std::string());
            mf.download_url = f.value("_sDownloadUrl", std::string());
            result.files.push_back(std::move(mf));
        }
    }
    result.ok = true;
    return result;
}

} // namespace thomaz::core
```

- [ ] **Step 4: Rodar e confirmar que passa**

Run: `make -C tests test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add source/core/mods/gamebanana_json.hpp source/core/mods/gamebanana_json.cpp tests/test_gamebanana_json.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): parse apiv11 search pages + mod file lists"
```

---

### Task 3: Orquestração de busca/resolução (`core/mods/mod_browse`)

**Files:**
- Create: `source/core/mods/mod_browse.hpp`, `source/core/mods/mod_browse.cpp`
- Test: `tests/test_mod_browse.cpp`

- [ ] **Step 1: Escrever o teste que falha**

Create `tests/test_mod_browse.cpp` (canned fetcher mirrors test_cheat_repository.cpp):

```cpp
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
        if (it == docs.end()) return std::nullopt; // transport failure
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
    auto fetch = mapFetcher({}); // any url -> nullopt
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
```

- [ ] **Step 2: Rodar e confirmar que falha**

Run: `make -C tests test` → FAIL: missing `core/mods/mod_browse.hpp`.

- [ ] **Step 3: Implementar mod_browse**

Create `source/core/mods/mod_browse.hpp`:

```cpp
#pragma once
#include "core/cheat_repository.hpp" // UrlFetcher
#include "core/mods/gamebanana_types.hpp"
#include <cstdint>
#include <string>

namespace thomaz::core {

enum class BrowseStatus { Ok, NetworkError };
struct BrowseResult {
    BrowseStatus status = BrowseStatus::NetworkError;
    SearchPage page;
};

// Free-text mod search via apiv11. game_id==0 => global. Pure: the fetcher is
// injected (same UrlFetcher contract as fetch_cheat_set).
BrowseResult search_mods(const std::string& query, std::uint64_t game_id, int page,
                         const UrlFetcher& fetch);

enum class ResolveStatus { Ok, NotFound, NetworkError };
struct ResolveResult {
    ResolveStatus status = ResolveStatus::NetworkError;
    std::vector<ModFile> files;
    std::string error;
};

// Resolve a mod's downloadable files (second request). NotFound when the API
// returns an _sErrorCode body; NetworkError on transport failure.
ResolveResult resolve_mod_files(std::uint64_t mod_id, const UrlFetcher& fetch);

} // namespace thomaz::core
```

Create `source/core/mods/mod_browse.cpp`:

```cpp
#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_json.hpp"
#include "core/mods/gamebanana_urls.hpp"

namespace thomaz::core {

BrowseResult search_mods(const std::string& query, std::uint64_t game_id, int page,
                         const UrlFetcher& fetch) {
    BrowseResult result;
    std::optional<std::string> body = fetch(gb_search_url(query, game_id, page));
    if (!body) {
        result.status = BrowseStatus::NetworkError;
        return result;
    }
    result.page   = parse_search_page(*body);
    result.status = BrowseStatus::Ok;
    return result;
}

ResolveResult resolve_mod_files(std::uint64_t mod_id, const UrlFetcher& fetch) {
    ResolveResult result;
    std::optional<std::string> body = fetch(gb_mod_files_url(mod_id));
    if (!body) {
        result.status = ResolveStatus::NetworkError;
        return result;
    }
    ModFilesResult parsed = parse_mod_files(*body);
    if (!parsed.ok) {
        result.status = ResolveStatus::NotFound;
        result.error  = parsed.error;
        return result;
    }
    result.status = ResolveStatus::Ok;
    result.files  = std::move(parsed.files);
    return result;
}

} // namespace thomaz::core
```

- [ ] **Step 4: Rodar e confirmar que passa**

Run: `make -C tests test` → PASS.

- [ ] **Step 5: Commit**

```bash
git add source/core/mods/mod_browse.hpp source/core/mods/mod_browse.cpp tests/test_mod_browse.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): GameBanana search + file-resolution orchestration"
```

---

### Task 4: Download em streaming (`platform/mods/mod_download`)

Mirror the curl setup of `source/platform/http_client_curl.cpp` (FOLLOWLOCATION, USERAGENT, SSL verify off — so the /dl redirect is followed and no CA bundle is needed), but write to a FILE* and report progress. NOT in the host test build; verified by compile-check + the Task 8 smoke.

**Files:**
- Create: `source/platform/mods/mod_download.hpp`, `source/platform/mods/mod_download.cpp`

- [ ] **Step 1: Header**

Create `source/platform/mods/mod_download.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace thomaz {

// Stream `url` to `dest_path` (parent dirs created). `progress(done,total)` is
// called during transfer; total may be 0 if the server doesn't send a length.
// Follows redirects (the GameBanana /dl/<id> link 30x-redirects to the file).
// Returns false and sets *err on failure.
bool download_file(const std::string& url, const std::string& dest_path,
                   const std::function<void(std::uint64_t done, std::uint64_t total)>& progress,
                   std::string* err);

} // namespace thomaz
```

- [ ] **Step 2: Implementation**

Create `source/platform/mods/mod_download.cpp`:

```cpp
#include "platform/mods/mod_download.hpp"

#include <curl/curl.h>
#include <cstdio>
#include <sys/stat.h>

namespace thomaz {

namespace {

void ensure_parent_dirs(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i)
        if (path[i] == '/')
            ::mkdir(path.substr(0, i).c_str(), 0777); // ignore EEXIST
}

size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::FILE*>(userdata);
    return std::fwrite(ptr, 1, size * nmemb, f);
}

struct ProgressCtx {
    const std::function<void(std::uint64_t, std::uint64_t)>* cb;
};

int xferInfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<ProgressCtx*>(p);
    if (ctx && ctx->cb && *ctx->cb)
        (*ctx->cb)((std::uint64_t)dlnow, (std::uint64_t)dltotal);
    return 0; // nonzero would abort
}

} // namespace

bool download_file(const std::string& url, const std::string& dest_path,
                   const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                   std::string* err) {
    ensure_parent_dirs(dest_path);

    std::FILE* out = std::fopen(dest_path.c_str(), "wb");
    if (!out) {
        if (err) *err = "cannot open " + dest_path;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(out);
        if (err) *err = "curl init failed";
        return false;
    }

    ProgressCtx ctx{&progress};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // /dl/<id> redirects
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    // No overall CURLOPT_TIMEOUT: large mods can take a while. Abort only on a
    // long stall instead.
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfo);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    // v1: skip TLS verification (no CA bundle shipped), matching http_client_curl.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode rc = curl_easy_perform(curl);
    long httpStatus = 0;
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);

    bool closeOk = (std::fclose(out) == 0);
    bool ok = (rc == CURLE_OK) && (httpStatus >= 200 && httpStatus < 300) && closeOk;
    if (!ok) {
        if (err) {
            if (rc != CURLE_OK) *err = curl_easy_strerror(rc);
            else if (!closeOk)  *err = "write error";
            else                *err = "HTTP " + std::to_string(httpStatus);
        }
        std::remove(dest_path.c_str()); // don't leave a partial file
    }
    return ok;
}

} // namespace thomaz
```

- [ ] **Step 3: Compile-check (object only) against host libcurl**

Run:
```bash
cd /home/solid/www/personal/playground/thomas
g++ -std=c++17 -I source -I lib/json $(curl-config --cflags 2>/dev/null) -c source/platform/mods/mod_download.cpp -o /tmp/mod_download.o && echo "COMPILE OK" && rm -f /tmp/mod_download.o
```
Expected: `COMPILE OK`. If `curl/curl.h` is missing, install libcurl dev headers or use the same header-fetch workaround M1 used for libarchive. Fix any error (e.g. a missing include) until it compiles.

- [ ] **Step 4: Host tests unaffected**

Run: `make -C tests test` → expect all prior tests still pass (this file is not in the test build).

- [ ] **Step 5: Commit**

```bash
git add source/platform/mods/mod_download.hpp source/platform/mods/mod_download.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): streaming file download with progress (libcurl)"
```

---

### Task 5: i18n strings (mods.json) + .gitignore for new XML

**Files:**
- Modify: `resources/i18n/en-US/mods.json`, `resources/i18n/pt-BR/mods.json`, `.gitignore`

- [ ] **Step 1: Add browse/detail keys to BOTH mods.json (same keys)**

Add these keys (merge into the existing JSON object; do not remove M1 keys). pt-BR:
```json
"browse": "Baixar mods (GameBanana)",
"search": "Buscar mods",
"search_hint": "Digite o nome do mod",
"searching": "Buscando…",
"no_results": "Nenhum mod encontrado.",
"load_more": "Carregar mais",
"likes": "curtidas",
"no_files": "Este mod não tem arquivos.",
"download": "Baixar e instalar",
"downloading": "Baixando…",
"download_failed": "Falha no download",
"search_error": "Falha na busca. Verifique a conexão.",
"installed_ok": "Mod baixado e instalado."
```
en-US:
```json
"browse": "Get mods (GameBanana)",
"search": "Search mods",
"search_hint": "Type the mod name",
"searching": "Searching…",
"no_results": "No mods found.",
"load_more": "Load more",
"likes": "likes",
"no_files": "This mod has no files.",
"download": "Download & install",
"downloading": "Downloading…",
"download_failed": "Download failed",
"search_error": "Search failed. Check your connection.",
"installed_ok": "Mod downloaded and installed."
```

- [ ] **Step 2: Un-ignore the two new activity XML in .gitignore**

The `/resources/**` block is an allowlist. Add, next to the other `!/resources/xml/activity/*.xml` lines:
```
!/resources/xml/activity/mod_browser.xml
!/resources/xml/activity/mod_detail.xml
```

- [ ] **Step 3: Commit**

```bash
git add resources/i18n/en-US/mods.json resources/i18n/pt-BR/mods.json .gitignore
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): i18n + resource allowlist for browse/detail screens"
```

---

### Task 6: Tela de busca (`app/mod_browser_activity`) — DRAFT

> UNCOMPILABLE in the sandbox (no devkitPro / desktop deps). Mirror existing activities EXACTLY for every brls call. Read `source/app/game_list_activity.cpp` (async + row list), `source/app/mod_manager_activity.cpp` (M1, the deferred-rebuild pattern), and `lib/borealis/library/include/borealis/core/ime.hpp` (text input). Confirm each brls API against an analog; this is a draft to be compiled by the user.

**Files:**
- Create: `source/app/mod_browser_activity.hpp`, `.cpp`, `resources/xml/activity/mod_browser.xml`

- [ ] **Step 1: XML** — mirror `clear_cheats.xml`/`mod_manager.xml`: AppletFrame `title="@i18n/mods/search"`, a `ProgressSpinner id="spinner"` (gone by default), a `Label id="emptyLabel"` (gone), and an `AnimatedBox id="resultsBox"`.

- [ ] **Step 2: Header** — `class ModBrowserActivity : public brls::Activity` with `explicit ModBrowserActivity(InstalledTitle title, IHttpClient* http);`, `~ModBrowserActivity() override;`, `CONTENT_FROM_XML_RES("activity/mod_browser.xml");`, `void onContentAvailable() override;`. Members: `InstalledTitle title; IHttpClient* http; std::string query; int page = 1; SearchPage lastPage;` and the `alive` shared_ptr pattern from the analogs. Include `platform/http_client.hpp`, `platform/title.hpp`, `core/mods/mod_browse.hpp`.

- [ ] **Step 3: Implementation** — wire to `core::search_mods` via a UrlFetcher backed by `this->http` (build the fetcher exactly like game_list_activity builds its index fetch: call `http->get(url)`, return `r.ok() ? std::optional(r.body) : std::nullopt`). Flow:
  - `onContentAvailable()`: `install_header_username(this);` then open the keyboard for the query:
    `brls::Application::getPlatform()->getImeManager()->openForText([this](std::string q){ this->query = q; this->page = 1; this->runSearch(); }, "mods/search"_i18n, "mods/search_hint"_i18n, 64);`
    (If the user cancels with empty text, show emptyLabel.)
  - `runSearch()`: show spinner; `brls::async` → build fetcher → `core::search_mods(query, /*game_id=*/0, page, fetch)` → `brls::sync` (guard `alive`) → `populate(result)`.
  - `populate(BrowseResult)`: hide spinner. On `NetworkError` notify `"mods/search_error"_i18n`. On Ok with empty records show emptyLabel `"mods/no_results"_i18n`. Else build one focusable row per `ModRecord` (Box ROW: name Label grow + a small Label showing `std::to_string(rec.likes) + " " + "mods/likes"_i18n`); tap → `brls::Application::pushActivity(new ModDetailActivity(this->title, rec.id, this->http));`. If `!lastPage.is_complete`, append a "load more" row (`"mods/load_more"_i18n`) that increments `page` and calls `runSearch()` (append mode) — for the draft, simplest is to replace the list with the next page; a true append is a future refinement (note it).
  - Reuse the deferred-rebuild discipline from M1 mod_manager: any list rebuild triggered from inside a row/button click handler must be wrapped in `brls::sync([this]{...})` to avoid the use-after-free M1 hit.
  - Add `#include "app/mod_detail_activity.hpp"`.

- [ ] **Step 4** — Self-review every brls call against the analogs; run `make -C tests test` (must stay green — app layer). Commit `mod_browser_activity.*` + `mod_browser.xml` only (never feed_activity/feed.xml/thomaz.json):
```bash
git add source/app/mod_browser_activity.hpp source/app/mod_browser_activity.cpp resources/xml/activity/mod_browser.xml
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): GameBanana search screen (draft, pending build)"
```

---

### Task 7: Tela de detalhe + download (`app/mod_detail_activity`) — DRAFT

> Same draft caveat as Task 6.

**Files:**
- Create: `source/app/mod_detail_activity.hpp`, `.cpp`, `resources/xml/activity/mod_detail.xml`

- [ ] **Step 1: XML** — mirror the others: AppletFrame `title="@i18n/mods/download"`, spinner, emptyLabel, `AnimatedBox id="filesBox"`.

- [ ] **Step 2: Header** — `class ModDetailActivity : public brls::Activity` with `ModDetailActivity(InstalledTitle title, std::uint64_t mod_id, IHttpClient* http);`, dtor, `CONTENT_FROM_XML_RES("activity/mod_detail.xml");`, `onContentAvailable()`. Members `InstalledTitle title; std::uint64_t modId; IHttpClient* http;` + `alive`. Includes: `core/mods/mod_browse.hpp`, `core/mods/mod_paths.hpp`, `platform/mods/mod_download.hpp`, `platform/mods/mod_actions.hpp`, `platform/http_client.hpp`, `platform/title.hpp`.

- [ ] **Step 3: Implementation**:
  - `onContentAvailable()`: header; spinner; `brls::async` → fetcher (as in Task 6) → `core::resolve_mod_files(modId, fetch)` → `brls::sync` (guard alive) → `populate`.
  - `populate(ResolveResult)`: hide spinner. `NetworkError` → notify `"mods/search_error"_i18n`. `NotFound` → emptyLabel with `r.error` (or `"mods/no_files"_i18n`). `Ok` with empty files → emptyLabel `"mods/no_files"_i18n`. Else one row per `ModFile` (name + human size); tap → `startDownload(file)`.
  - `startDownload(const core::ModFile& f)`: compute `dest = core::mod_staging_root() + "/_incoming/" + f.filename;` derive `mod_name` = filename without extension. Show a progress dialog/label (`"mods/downloading"_i18n`). `brls::async`:
    - `std::string err; bool ok = download_file(f.download_url, dest, [this](uint64_t done,uint64_t total){ /* optionally brls::sync update a label */ }, &err);`
    - if ok: `ModActionResult ir = import_archive(this->title.title_id, mod_name, dest, nullptr);` then on the UI thread (`brls::sync`, guard alive) notify `ir.ok ? "mods/installed_ok"_i18n : ("mods/download_failed"_i18n + ": " + ir.error)` and `brls::Application::popActivity();` back toward the manager.
    - if !ok: `brls::sync` → notify `"mods/download_failed"_i18n + ": " + err`.
    - NOTE: progress callback runs on the worker thread; only touch UI via `brls::sync`. Keep the draft simple — a single "Downloading…" label is fine; live percent is a refinement.
  - Defer any list rebuild done from inside a click handler via `brls::sync` (M1 lesson).

- [ ] **Step 4** — Self-review brls calls vs analogs; `make -C tests test` green. Commit the 3 files only:
```bash
git add source/app/mod_detail_activity.hpp source/app/mod_detail_activity.cpp resources/xml/activity/mod_detail.xml
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): mod detail + download/install screen (draft, pending build)"
```

---

### Task 8: Wire the entry + live API smoke

**Files:**
- Modify: `source/app/mod_manager_activity.cpp`

- [ ] **Step 1: Add the "Get mods" entry** — in `ModManagerActivity::refreshList()` (M1), add a button row (mirror the existing Import button) labeled `"mods/browse"_i18n` that pushes the browser. It needs an `IHttpClient*` — add an `IHttpClient* http` param to `ModManagerActivity`'s constructor and store it; update the call site in `game_list_activity.cpp` (Mods-mode row tap) to pass `this->http`: `new ModManagerActivity(rowTitle, client)`. Then the button: `brls::Application::pushActivity(new ModBrowserActivity(this->title, this->http));`. Add `#include "app/mod_browser_activity.hpp"`. Keep all rebuilds inside click handlers deferred via `brls::sync` (M1 lesson).

- [ ] **Step 2: Commit the wiring** (mod_manager_activity.cpp + game_list_activity.cpp only):
```bash
git add source/app/mod_manager_activity.cpp source/app/mod_manager_activity.hpp source/app/game_list_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(mods): open GameBanana browser from the mod manager"
```

- [ ] **Step 3: LIVE smoke of the core pipeline (host, real apiv11)** — write a throwaway harness `tests/smoke_gb.cpp` that uses libcurl to fetch and runs the parsers against the LIVE API, and probes a /dl redirect:
```cpp
#include "core/mods/mod_browse.hpp"
#include "core/mods/gamebanana_urls.hpp"
#include <curl/curl.h>
#include <cstdio>
using namespace thomaz::core;
static size_t w(char* p, size_t s, size_t n, void* u){ ((std::string*)u)->append(p, s*n); return s*n; }
static UrlFetcher curlFetch(){ return [](const std::string& url)->std::optional<std::string>{
    CURL* c=curl_easy_init(); std::string body; if(!c) return std::nullopt;
    curl_easy_setopt(c,CURLOPT_URL,url.c_str()); curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c,CURLOPT_USERAGENT,"thomaz/0.1"); curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,0L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,0L); curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,w);
    curl_easy_setopt(c,CURLOPT_WRITEDATA,&body); CURLcode rc=curl_easy_perform(c); curl_easy_cleanup(c);
    if(rc!=CURLE_OK) return std::nullopt; return body; }; }
int main(){
    auto f=curlFetch();
    BrowseResult b=search_mods("mario", 0, 1, f);
    std::printf("search status=%d total=%llu got=%zu\n",(int)b.status,(unsigned long long)b.page.total,b.page.records.size());
    if(b.page.records.empty()){ std::printf("NO RECORDS\n"); return 1; }
    std::uint64_t id=b.page.records[0].id;
    std::printf("first: id=%llu name=%s hasFiles=%d\n",(unsigned long long)id,b.page.records[0].name.c_str(),b.page.records[0].has_files);
    ResolveResult r=resolve_mod_files(id, f);
    std::printf("resolve status=%d files=%zu\n",(int)r.status,r.files.size());
    if(!r.files.empty()) std::printf("file: %s url=%s\n",r.files[0].filename.c_str(),r.files[0].download_url.c_str());
    return 0;
}
```
Build + run:
```bash
g++ -std=c++17 -I source -I lib/json $(curl-config --cflags) tests/smoke_gb.cpp \
  source/core/mods/gamebanana_urls.cpp source/core/mods/gamebanana_json.cpp source/core/mods/mod_browse.cpp \
  $(curl-config --libs) -o /tmp/smoke_gb && /tmp/smoke_gb
# Probe the /dl redirect target (expect a 30x then a binary content-type / attachment):
curl -sIL "$(/tmp/smoke_gb 2>/dev/null | grep -o 'https://gamebanana.com/dl/[0-9]*' | head -1)" | grep -iE "HTTP/|location:|content-type|content-disposition" | head
```
EXPECTED: search returns records, resolve returns ≥1 file with a `https://gamebanana.com/dl/<id>` URL, and the /dl probe shows a redirect to a binary (confirming `download_file`'s FOLLOWLOCATION approach works). Record the output. If apiv11 shapes differ from the fixtures, fix the parser (Task 2) and re-run. Clean up: `rm -f tests/smoke_gb.cpp /tmp/smoke_gb`. No commit from this step.

---

### Task 9: Build + hardware verification (manual checklist)

> Cannot run in the sandbox. Record results.

- [ ] Build desktop (with libarchive-dev + SDL2) and `.nro` (CI). Confirm new core/mods files compile and link; confirm `mod_browser.xml`/`mod_detail.xml`/`mods.json` keys bundle into RomFS.
- [ ] On hardware (Atmosphère, online): Mods → pick a game → "Get mods" → search (e.g. a known mod) → results render → open a mod → file list renders → download → progress → auto-installs into that game's staging (M1) → returns to manager with the new mod listed.
- [ ] Enable the downloaded mod, open the game, confirm LayeredFS applies it.
- [ ] Error paths: search with no connection → "search failed"; a mod with no files → "no files"; a deleted mod → graceful NotFound.
- [ ] Update README roadmap (mods: GameBanana download M2).

---

## Self-Review

**Spec coverage (M2):** search (Task 1-3 + 6), file resolution (Task 2-3 + 7), streaming download (Task 4 + 7), install via M1 import (Task 7), entry from the per-game manager (Task 8), i18n (Task 5). Live-API smoke (Task 8) + hardware (Task 9). ✅ The plan corrects the spec's "Core API" to the verified **apiv11**. ✅ M3 (curated mapping, Subfeed listing, game-scoped search via `_idGameRow`) explicitly deferred. ✅

**Placeholders:** core/platform tasks (1-5) have complete code + tests. UI tasks (6-8) are intentionally draft-with-analog-guidance (uncompilable here — same constraint as M1's UI task), with exact API calls (`search_mods`, `resolve_mod_files`, `download_file`, `import_archive`, `getImeManager()->openForText`) specified as the fixed contract. ✅

**Type consistency:** `ModRecord`/`SearchPage`/`ModFile` (Task 1) used in 2/3/6/7; `parse_search_page`/`parse_mod_files`/`ModFilesResult` (Task 2) used in 3; `BrowseResult`/`BrowseStatus`/`ResolveResult`/`ResolveStatus`/`search_mods`/`resolve_mod_files` (Task 3) used in 6/7; `download_file` (Task 4) used in 7; `import_archive`/`mod_staging_root` (M1) used in 7. `UrlFetcher` reused from `core/cheat_repository.hpp`. Signatures match across definition and use. ✅

**Known risks flagged for the executor:** rate limits/auth unknown (search only on submit); /dl redirect-to-binary assumption verified in the Task 8 smoke; apiv11 base is `gamebanana.com/apiv11` (not `api.`); per-mod error is HTTP-200-with-body; counts optional; thumbnails deferred; UI is an uncompiled draft (build on real toolchain). The `ModManagerActivity` constructor gains an `IHttpClient*` (Task 8) — update its M1 call site in `game_list_activity.cpp`.
