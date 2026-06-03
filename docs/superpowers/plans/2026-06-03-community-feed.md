# Feed da Comunidade — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adicionar ao thomaz um feed social (estilo Twitter, scroll infinito) onde usuários logam, postam screenshots do Álbum do Switch com legenda, curtem e comentam — com toda a rede atrás de uma interface stubada (fake), rodável no desktop hoje.

**Architecture:** Mesma estratégia em camadas do projeto: `core/feed/` (tipos + lógica pura, testável via doctest) → `platform/feed/` (contrato `IFeedClient` + `FakeFeedClient`, `IAlbumSource` + impls Switch/Fake, `auth_store`) → `app/` (`FeedActivity` master-detail, `ComposerActivity` split, `AuthActivity`). Injeção por construtor escolhida em `main.cpp` por plataforma, exatamente como `ITitleService`/`IHttpClient` hoje.

**Tech Stack:** C++17, Borealis (xfangfang fork), doctest (testes desktop), nlohmann/json, libnx `caps:a` (Switch), CMake (app) + Makefile (testes).

**Spec:** `docs/superpowers/specs/2026-06-03-community-feed-design.md`

---

## Mapa de arquivos

**Criar — core (testável):**
- `source/core/feed/feed_types.hpp` — `User`, `Comment`, `Post`, `FeedPage`, `Session`.
- `source/core/feed/feed_pagination.hpp` / `.cpp` — `merge_feed_page`, `find_post`.
- `source/core/feed/session_codec.hpp` / `.cpp` — `serialize_session`, `parse_session`.

**Criar — platform:**
- `source/platform/feed/feed_client.hpp` — `AuthResult`, `ActionResult`, `IFeedClient`.
- `source/platform/feed/fake_feed_client.hpp` / `.cpp` — `FakeFeedClient`.
- `source/platform/feed/album_source.hpp` — `CaptureDate`, `AlbumEntry`, `IAlbumSource`.
- `source/platform/feed/fake_album_source.hpp` / `.cpp` — `FakeAlbumSource`.
- `source/platform/feed/switch_album_source.hpp` / `.cpp` — `SwitchAlbumSource` (`caps:a`, só `__SWITCH__`).
- `source/platform/feed/auth_store.hpp` / `.cpp` — `load_session`/`save_session`/`clear_session`.

**Criar — app + XML:**
- `source/app/auth_activity.hpp` / `.cpp` + `resources/xml/activity/auth.xml`.
- `source/app/feed_activity.hpp` / `.cpp` + `resources/xml/activity/feed.xml`.
- `source/app/composer_activity.hpp` / `.cpp` + `resources/xml/activity/composer.xml`.

**Modificar:**
- `tests/Makefile` — compilar também `../source/core/feed/*.cpp`.
- `resources/xml/activity/home.xml` — hero vira Feed; card "saves" vira Cheats.
- `source/app/home_activity.cpp` — navegação dos novos cards.
- `source/main.cpp` — instanciar e injetar os serviços do feed.
- `resources/i18n/en-US/thomaz.json` e `resources/i18n/pt-BR/thomaz.json` — chaves novas.

**Convenções herdadas (seguir à risca):**
- Toda rede/IO em `brls::async(...)`, voltando à UI com `brls::sync(...)`, guardado por `std::shared_ptr<std::atomic_bool> alive` setado `false` no destrutor (ver `GameListActivity`).
- Activities usam `CONTENT_FROM_XML_RES("activity/<x>.xml")`, `onContentAvailable()`, `getView(id)`.
- Cores: surface_1 `nvgRGB(0x1A,0x1C,0x23)`, surface_2 `nvgRGB(0x22,0x24,0x2D)`, acento `nvgRGB(0x7C,0x5C,0xFF)`.
- Texto via `brls::InputCell` ou `brls::Application::getImeManager()->openForText(cb, header, sub, maxLen, initial)`.

---

## FASE 1 — Núcleo do feed (TDD, sem UI)

### Task 1: Habilitar testes em `core/feed/`

**Files:**
- Modify: `tests/Makefile:3`

- [ ] **Step 1: Atualizar o glob de fontes do Makefile**

Trocar a linha `SRCS` para incluir o subdiretório novo:

```makefile
CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib/doctest -I../lib/json -I../source
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/feed/*.cpp)
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
```

- [ ] **Step 2: Confirmar que os testes atuais ainda passam**

Run: `cd tests && make clean && make test`
Expected: PASS (sem novos arquivos ainda, build idêntico ao atual).

- [ ] **Step 3: Commit**

```bash
git add tests/Makefile
git commit -m "test(feed): compile core/feed sources in the test runner"
```

---

### Task 2: Tipos do feed

**Files:**
- Create: `source/core/feed/feed_types.hpp`

- [ ] **Step 1: Escrever os tipos (header puro, sem deps)**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::feed {

struct User {
    std::string id;
    std::string username;
};

struct Comment {
    std::string  id;
    User         author;
    std::string  text;
    std::int64_t createdAt = 0; // epoch seconds
};

struct Post {
    std::string   id;
    User          author;
    std::string   imageUrl;       // URL remota (real) ou chave do fake
    std::string   caption;
    std::uint64_t gameTitleId = 0; // 0 = jogo desconhecido
    std::string   gameName;        // resolvido via ITitleService no compositor
    int           likeCount   = 0;
    bool          likedByMe   = false;
    int           commentCount = 0;
    std::int64_t  createdAt   = 0;
};

struct FeedPage {
    std::vector<Post> posts;
    std::string       nextCursor; // passar de volta em fetchFeed; vazio = fim
    bool              hasMore = false;
};

struct Session {
    std::string token;
    std::string username;
};

} // namespace thomaz::feed
```

- [ ] **Step 2: Commit**

```bash
git add source/core/feed/feed_types.hpp
git commit -m "feat(feed): add core feed types"
```

---

### Task 3: Paginação por cursor (merge + dedup)

**Files:**
- Create: `source/core/feed/feed_pagination.hpp`
- Create: `source/core/feed/feed_pagination.cpp`
- Test: `tests/test_feed_pagination.cpp`

- [ ] **Step 1: Escrever o teste que falha**

```cpp
#include "doctest.h"
#include "core/feed/feed_pagination.hpp"

using namespace thomaz::feed;

static Post mk(const std::string& id) { Post p; p.id = id; return p; }

TEST_CASE("merge_feed_page appends all posts into an empty accumulator") {
    std::vector<Post> acc;
    FeedPage page; page.posts = { mk("a"), mk("b") }; page.hasMore = true;
    bool more = merge_feed_page(acc, page);
    REQUIRE(acc.size() == 2);
    CHECK(acc[0].id == "a");
    CHECK(acc[1].id == "b");
    CHECK(more == true);
}

TEST_CASE("merge_feed_page skips posts whose id is already present") {
    std::vector<Post> acc = { mk("a"), mk("b") };
    FeedPage page; page.posts = { mk("b"), mk("c") }; page.hasMore = false;
    bool more = merge_feed_page(acc, page);
    REQUIRE(acc.size() == 3);
    CHECK(acc[2].id == "c");
    CHECK(more == false);
}

TEST_CASE("merge_feed_page preserves insertion order across pages") {
    std::vector<Post> acc;
    FeedPage p1; p1.posts = { mk("a"), mk("b") };
    FeedPage p2; p2.posts = { mk("c") };
    merge_feed_page(acc, p1);
    merge_feed_page(acc, p2);
    REQUIRE(acc.size() == 3);
    CHECK(acc[0].id == "a");
    CHECK(acc[2].id == "c");
}

TEST_CASE("find_post returns a pointer to the matching post or null") {
    std::vector<Post> acc = { mk("a"), mk("b") };
    Post* hit = find_post(acc, "b");
    REQUIRE(hit != nullptr);
    CHECK(hit->id == "b");
    CHECK(find_post(acc, "zzz") == nullptr);
}
```

- [ ] **Step 2: Rodar o teste e confirmar a falha**

Run: `cd tests && make test`
Expected: FAIL na compilação — `core/feed/feed_pagination.hpp` não existe.

- [ ] **Step 3: Escrever o header**

```cpp
#pragma once
#include <string>
#include <vector>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// Anexa em `acc` apenas os posts de `page` cujo id ainda não está em `acc`
// (dedup por Post::id), preservando a ordem. Retorna page.hasMore.
bool merge_feed_page(std::vector<Post>& acc, const FeedPage& page);

// Ponteiro para o post com esse id dentro de `acc`, ou nullptr. Usado para
// atualizar contadores de curtida/comentário in-place. Válido até `acc` mudar.
Post* find_post(std::vector<Post>& acc, const std::string& id);

} // namespace thomaz::feed
```

- [ ] **Step 4: Escrever a implementação mínima**

```cpp
#include "core/feed/feed_pagination.hpp"
#include <unordered_set>

namespace thomaz::feed {

bool merge_feed_page(std::vector<Post>& acc, const FeedPage& page)
{
    std::unordered_set<std::string> seen;
    seen.reserve(acc.size());
    for (const auto& p : acc)
        seen.insert(p.id);

    for (const auto& p : page.posts)
        if (seen.insert(p.id).second)
            acc.push_back(p);

    return page.hasMore;
}

Post* find_post(std::vector<Post>& acc, const std::string& id)
{
    for (auto& p : acc)
        if (p.id == id)
            return &p;
    return nullptr;
}

} // namespace thomaz::feed
```

- [ ] **Step 5: Rodar o teste e confirmar PASS**

Run: `cd tests && make test`
Expected: PASS (todos os `TEST_CASE` de paginação verdes).

- [ ] **Step 6: Commit**

```bash
git add source/core/feed/feed_pagination.hpp source/core/feed/feed_pagination.cpp tests/test_feed_pagination.cpp
git commit -m "feat(feed): cursor pagination merge with id dedup"
```

---

### Task 4: Codec da sessão (persistência testável)

**Files:**
- Create: `source/core/feed/session_codec.hpp`
- Create: `source/core/feed/session_codec.cpp`
- Test: `tests/test_session_codec.cpp`

- [ ] **Step 1: Escrever o teste que falha**

```cpp
#include "doctest.h"
#include "core/feed/session_codec.hpp"

using namespace thomaz::feed;

TEST_CASE("serialize then parse round-trips a session") {
    Session s; s.token = "tok123"; s.username = "joao";
    std::string text = serialize_session(s);
    auto back = parse_session(text);
    REQUIRE(back.has_value());
    CHECK(back->token == "tok123");
    CHECK(back->username == "joao");
}

TEST_CASE("parse_session returns nullopt on empty or malformed input") {
    CHECK_FALSE(parse_session("").has_value());
    CHECK_FALSE(parse_session("only-one-line").has_value());
}

TEST_CASE("parse_session ignores trailing whitespace/newlines") {
    auto back = parse_session("tok123\njoao\n");
    REQUIRE(back.has_value());
    CHECK(back->token == "tok123");
    CHECK(back->username == "joao");
}

TEST_CASE("username is never empty in a valid session") {
    CHECK_FALSE(parse_session("tok123\n\n").has_value());
}
```

- [ ] **Step 2: Rodar e confirmar falha**

Run: `cd tests && make test`
Expected: FAIL — `core/feed/session_codec.hpp` não existe.

- [ ] **Step 3: Header**

```cpp
#pragma once
#include <optional>
#include <string>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// Formato em disco: "<token>\n<username>\n". Funções puras (sem IO) para
// serem testáveis; o auth_store faz o read/write do arquivo.
std::string serialize_session(const Session& s);
std::optional<Session> parse_session(const std::string& text);

} // namespace thomaz::feed
```

- [ ] **Step 4: Implementação mínima**

```cpp
#include "core/feed/session_codec.hpp"
#include <sstream>

namespace thomaz::feed {

std::string serialize_session(const Session& s)
{
    return s.token + "\n" + s.username + "\n";
}

std::optional<Session> parse_session(const std::string& text)
{
    std::istringstream in(text);
    Session s;
    if (!std::getline(in, s.token))
        return std::nullopt;
    if (!std::getline(in, s.username))
        return std::nullopt;

    // strip CR (Windows) e espaços nas pontas
    auto trim = [](std::string& v) {
        const char* ws = " \t\r\n";
        auto a = v.find_first_not_of(ws);
        if (a == std::string::npos) { v.clear(); return; }
        auto b = v.find_last_not_of(ws);
        v = v.substr(a, b - a + 1);
    };
    trim(s.token);
    trim(s.username);

    if (s.token.empty() || s.username.empty())
        return std::nullopt;
    return s;
}

} // namespace thomaz::feed
```

- [ ] **Step 5: Rodar e confirmar PASS**

Run: `cd tests && make test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add source/core/feed/session_codec.hpp source/core/feed/session_codec.cpp tests/test_session_codec.cpp
git commit -m "feat(feed): session serialize/parse codec"
```

---

## FASE 2 — Contratos de plataforma + fakes

### Task 5: Contrato `IFeedClient`

**Files:**
- Create: `source/platform/feed/feed_client.hpp`

- [ ] **Step 1: Escrever o contrato**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/feed/feed_types.hpp"

namespace thomaz {

struct AuthResult {
    bool        ok = false;
    std::string token;   // preenchido quando ok
    std::string error;   // mensagem quando !ok (ex.: "username já existe")
};

struct ActionResult {
    bool        ok = false;
    std::string error;
};

// Contrato de rede do feed (auth + feed juntos). FakeFeedClient roda no desktop
// hoje; HttpFeedClient (futuro) pluga a API real sem tocar na UI. Todos os
// métodos são chamados de um worker thread (brls::async); não tocam na UI.
class IFeedClient {
  public:
    virtual ~IFeedClient() = default;

    // Conta
    virtual AuthResult registerUser(const std::string& user, const std::string& pass) = 0;
    virtual AuthResult login(const std::string& user, const std::string& pass) = 0;

    // Feed (cursor vazio = primeira página)
    virtual feed::FeedPage fetchFeed(const std::string& cursor) = 0;

    // Postar (bytes JPEG vindos do IAlbumSource + jogo já resolvido)
    virtual ActionResult createPost(const std::string& token,
                                    const std::vector<std::uint8_t>& jpeg,
                                    const std::string& caption,
                                    std::uint64_t gameTitleId,
                                    const std::string& gameName) = 0;

    // Curtir / descurtir
    virtual ActionResult setLike(const std::string& token,
                                 const std::string& postId, bool liked) = 0;

    // Comentários
    virtual std::vector<feed::Comment> fetchComments(const std::string& postId) = 0;
    virtual ActionResult addComment(const std::string& token,
                                    const std::string& postId, const std::string& text) = 0;
};

} // namespace thomaz
```

- [ ] **Step 2: Commit**

```bash
git add source/platform/feed/feed_client.hpp
git commit -m "feat(feed): IFeedClient network contract"
```

---

### Task 6: Contrato `IAlbumSource`

**Files:**
- Create: `source/platform/feed/album_source.hpp`

- [ ] **Step 1: Escrever o contrato**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz {

struct CaptureDate {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

struct AlbumEntry {
    std::string               id;        // chave opaca p/ loadFull
    std::uint64_t             titleId = 0; // application_id real (caps:a)
    CaptureDate               captured;
    std::vector<std::uint8_t> thumbnail; // JPEG p/ o grid
};

// Abstrai o Álbum do Switch. SwitchAlbumSource usa caps:a; FakeAlbumSource
// serve imagens de exemplo no desktop. list()/loadFull() são chamados de
// worker threads (podem ser lentos).
class IAlbumSource {
  public:
    virtual ~IAlbumSource() = default;
    virtual std::vector<AlbumEntry>   list() = 0;
    virtual std::vector<std::uint8_t> loadFull(const std::string& id) = 0;
};

} // namespace thomaz
```

- [ ] **Step 2: Commit**

```bash
git add source/platform/feed/album_source.hpp
git commit -m "feat(feed): IAlbumSource contract"
```

---

### Task 7: `auth_store` (persistência da sessão)

**Files:**
- Create: `source/platform/feed/auth_store.hpp`
- Create: `source/platform/feed/auth_store.cpp`

- [ ] **Step 1: Header**

```cpp
#pragma once
#include <optional>
#include "core/feed/feed_types.hpp"

namespace thomaz {

// Sessão persistida (mantém login entre execuções). Arquivo na SD no Switch,
// pasta de trabalho no desktop — mesmo padrão de app_settings.
std::optional<feed::Session> load_session();
void save_session(const feed::Session& s);
void clear_session();

} // namespace thomaz
```

- [ ] **Step 2: Implementação (reusa read/write_text_file + codec)**

```cpp
#include "platform/feed/auth_store.hpp"
#include "platform/cheat_store.hpp"      // read_text_file / write_text_file
#include "core/feed/session_codec.hpp"
#include <cstdio>

namespace thomaz {

namespace {
std::string session_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/session.txt";
#else
    return "thomaz-cache/session.txt";
#endif
}
} // namespace

std::optional<feed::Session> load_session()
{
    if (auto raw = read_text_file(session_file()))
        return feed::parse_session(*raw);
    return std::nullopt;
}

void save_session(const feed::Session& s)
{
    write_text_file(session_file(), feed::serialize_session(s));
}

void clear_session()
{
    std::remove(session_file().c_str());
}

} // namespace thomaz
```

- [ ] **Step 3: Build desktop pra garantir que compila/linka**

Run: `./scripts/build-desktop.sh`
Expected: build OK (o app ainda não usa auth_store, mas precisa compilar).

- [ ] **Step 4: Commit**

```bash
git add source/platform/feed/auth_store.hpp source/platform/feed/auth_store.cpp
git commit -m "feat(feed): persist session via auth_store"
```

---

### Task 8: `FakeAlbumSource`

**Files:**
- Create: `source/platform/feed/fake_album_source.hpp`
- Create: `source/platform/feed/fake_album_source.cpp`

- [ ] **Step 1: Header**

```cpp
#pragma once
#include "platform/feed/album_source.hpp"

namespace thomaz {

// Álbum falso para o desktop: gera N entradas com title IDs que o
// FakeTitleService resolve para nomes de mentira. As imagens são retângulos
// JPEG mínimos embutidos (não precisam ser bonitas — só carregar).
class FakeAlbumSource : public IAlbumSource {
  public:
    std::vector<AlbumEntry>   list() override;
    std::vector<std::uint8_t> loadFull(const std::string& id) override;
};

} // namespace thomaz
```

- [ ] **Step 2: Implementação**

Usa um JPEG 1x1 embutido (bytes constantes) para thumbnail e full — `brls::Image::setImageFromMem` aceita qualquer JPEG válido. Os `titleId` batem com os do `FakeTitleService` para a auto-marcação funcionar no desktop.

```cpp
#include "platform/feed/fake_album_source.hpp"

namespace thomaz {

namespace {
// JPEG válido 1x1 (fundo cinza). Suficiente para o decoder do Borealis.
const std::vector<std::uint8_t> kTinyJpeg = {
    0xFF,0xD8,0xFF,0xDB,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,
    0x07,0x09,0x09,0x08,0x0A,0x0C,0x14,0x0D,0x0C,0x0B,0x0B,0x0C,0x19,0x12,0x13,0x0F,
    0x14,0x1D,0x1A,0x1F,0x1E,0x1D,0x1A,0x1C,0x1C,0x20,0x24,0x2E,0x27,0x20,0x22,0x2C,
    0x23,0x1C,0x1C,0x28,0x37,0x29,0x2C,0x30,0x31,0x34,0x34,0x34,0x1F,0x27,0x39,0x3D,
    0x38,0x32,0x3C,0x2E,0x33,0x34,0x32,0xFF,0xC0,0x00,0x0B,0x08,0x00,0x01,0x00,0x01,
    0x01,0x01,0x11,0x00,0xFF,0xC4,0x00,0x14,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0xFF,0xC4,0x00,0x14,0x10,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
    0xDA,0x00,0x08,0x01,0x01,0x00,0x00,0x3F,0x00,0xD2,0xCF,0x20,0xFF,0xD9
};
} // namespace

std::vector<AlbumEntry> FakeAlbumSource::list()
{
    std::vector<AlbumEntry> out;
    // title IDs alinhados com FakeTitleService (jogos de mentira).
    const std::uint64_t fakeIds[] = { 0x0100000000010000ULL, 0x010000000E5EE000ULL,
                                      0x0100ABCDEF000000ULL };
    for (int i = 0; i < 6; ++i) {
        AlbumEntry e;
        e.id        = "fake-" + std::to_string(i);
        e.titleId   = fakeIds[i % 3];
        e.captured  = { 2026, 6, 1 + i, 12, i, 0 };
        e.thumbnail = kTinyJpeg;
        out.push_back(e);
    }
    return out;
}

std::vector<std::uint8_t> FakeAlbumSource::loadFull(const std::string&)
{
    return kTinyJpeg;
}

} // namespace thomaz
```

- [ ] **Step 3: Commit**

```bash
git add source/platform/feed/fake_album_source.hpp source/platform/feed/fake_album_source.cpp
git commit -m "feat(feed): FakeAlbumSource for desktop"
```

---

### Task 9: `SwitchAlbumSource` (caps:a)

**Files:**
- Create: `source/platform/feed/switch_album_source.hpp`
- Create: `source/platform/feed/switch_album_source.cpp`

> O corpo inteiro do `.cpp` fica sob `#ifdef __SWITCH__` (libnx só existe na toolchain do devkitPro). No desktop o arquivo compila vazio. Não é testável no desktop nem validável sem hardware — risco anotado na spec §9.

- [ ] **Step 1: Header**

```cpp
#pragma once
#include "platform/feed/album_source.hpp"

namespace thomaz {

// Lê o Álbum do sistema via caps:a (libnx). Só screenshots (ignora vídeos).
// init() deve ser chamado uma vez no startup (Switch); exit() no shutdown.
class SwitchAlbumSource : public IAlbumSource {
  public:
    bool init();
    void exit();
    std::vector<AlbumEntry>   list() override;
    std::vector<std::uint8_t> loadFull(const std::string& id) override;
};

} // namespace thomaz
```

- [ ] **Step 2: Implementação (caps:a)**

```cpp
#include "platform/feed/switch_album_source.hpp"

#ifdef __SWITCH__
#include <switch.h>
#include <cstring>
#include <vector>

namespace thomaz {

namespace {
// Codifica um CapsAlbumFileId em string hex para usar como AlbumEntry::id,
// e decodifica de volta em loadFull. Round-trip dos bytes crus do struct.
std::string encodeId(const CapsAlbumFileId& fid) {
    const auto* p = reinterpret_cast<const unsigned char*>(&fid);
    static const char* hex = "0123456789abcdef";
    std::string s; s.reserve(sizeof(fid) * 2);
    for (size_t i = 0; i < sizeof(fid); ++i) { s += hex[p[i] >> 4]; s += hex[p[i] & 0xF]; }
    return s;
}
bool decodeId(const std::string& s, CapsAlbumFileId& out) {
    if (s.size() != sizeof(out) * 2) return false;
    auto nyb = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    auto* p = reinterpret_cast<unsigned char*>(&out);
    for (size_t i = 0; i < sizeof(out); ++i) {
        int hi = nyb(s[i*2]), lo = nyb(s[i*2+1]);
        if (hi < 0 || lo < 0) return false;
        p[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}
} // namespace

bool SwitchAlbumSource::init()
{
    Result rc = capsaInitialize();
    return R_SUCCEEDED(rc);
}

void SwitchAlbumSource::exit()
{
    capsaExit();
}

std::vector<AlbumEntry> SwitchAlbumSource::list()
{
    std::vector<AlbumEntry> out;

    // Conta e lista entradas do storage do cartão SD (NAND tem poucas).
    for (auto storage : { CapsAlbumStorage_Sd, CapsAlbumStorage_Nand }) {
        u64 count = 0;
        if (R_FAILED(capsaGetAlbumFileCount(storage, &count)) || count == 0)
            continue;

        std::vector<CapsAlbumEntry> entries(count);
        u64 got = 0;
        if (R_FAILED(capsaGetAlbumFileList(storage, &got, entries.data(),
                                           entries.size() * sizeof(CapsAlbumEntry))))
            continue;

        for (u64 i = 0; i < got; ++i) {
            const CapsAlbumFileId& fid = entries[i].file_id;
            // Só screenshots (ignora vídeos/movies).
            if (fid.content != CapsAlbumFileContents_ScreenShot)
                continue;

            AlbumEntry e;
            e.id       = encodeId(fid);
            e.titleId  = fid.application_id;
            e.captured = { fid.datetime.year, fid.datetime.month, fid.datetime.day,
                           fid.datetime.hour, fid.datetime.minute, fid.datetime.second };

            // Thumbnail: decodifica o screenshot em RGBA reduzido. Para o grid
            // basta a imagem cheia reescalada pela UI; carregamos thumb sob
            // demanda no loadFull para não estourar memória aqui.
            out.push_back(std::move(e));
        }
    }
    return out;
}

std::vector<std::uint8_t> SwitchAlbumSource::loadFull(const std::string& id)
{
    CapsAlbumFileId fid;
    if (!decodeId(id, fid))
        return {};

    // Carrega o JPEG original do screenshot.
    // capsaLoadAlbumFile escreve o arquivo .jpg cru no buffer fornecido.
    std::vector<std::uint8_t> buf(4 * 1024 * 1024); // screenshots ~ até alguns MB
    u64 outSize = 0;
    Result rc = capsaLoadAlbumFile(&fid, &outSize, buf.data(), buf.size());
    if (R_FAILED(rc))
        return {};
    buf.resize(outSize);
    return buf;
}

} // namespace thomaz
#endif // __SWITCH__
```

> Nota de execução: se a build do Switch acusar nome de símbolo diferente (a API `caps:a` varia entre versões do libnx — ver `capsa.h` da versão instalada), ajustar os nomes (`capsaGetAlbumFileCount`/`capsaGetAlbumFileList`/`capsaLoadAlbumFile`) conforme o header local em `$DEVKITPRO/portlibs/switch/include/switch/services/capsa.h`. Os campos `CapsAlbumFileId::application_id`, `.content`, `.datetime` são estáveis.

- [ ] **Step 3: Build desktop (deve compilar como TU vazio)**

Run: `./scripts/build-desktop.sh`
Expected: build OK (corpo sob `__SWITCH__`, então no desktop é só o header incluído).

- [ ] **Step 4: Commit**

```bash
git add source/platform/feed/switch_album_source.hpp source/platform/feed/switch_album_source.cpp
git commit -m "feat(feed): SwitchAlbumSource via caps:a"
```

---

### Task 10: `FakeFeedClient`

**Files:**
- Create: `source/platform/feed/fake_feed_client.hpp`
- Create: `source/platform/feed/fake_feed_client.cpp`

- [ ] **Step 1: Header**

```cpp
#pragma once
#include <map>
#include <vector>
#include "platform/feed/feed_client.hpp"

namespace thomaz {

// Backend em memória para rodar/testar o app no desktop. Gera páginas de posts,
// aceita qualquer cadastro/login, e guarda curtidas/comentários/novos posts
// durante a sessão. Determinístico (sem rede, sem random).
class FakeFeedClient : public IFeedClient {
  public:
    FakeFeedClient();

    AuthResult registerUser(const std::string& user, const std::string& pass) override;
    AuthResult login(const std::string& user, const std::string& pass) override;
    feed::FeedPage fetchFeed(const std::string& cursor) override;
    ActionResult createPost(const std::string& token,
                            const std::vector<std::uint8_t>& jpeg,
                            const std::string& caption,
                            std::uint64_t gameTitleId,
                            const std::string& gameName) override;
    ActionResult setLike(const std::string& token,
                         const std::string& postId, bool liked) override;
    std::vector<feed::Comment> fetchComments(const std::string& postId) override;
    ActionResult addComment(const std::string& token,
                            const std::string& postId, const std::string& text) override;

  private:
    std::vector<feed::Post> posts; // mais novo primeiro
    std::map<std::string, std::vector<feed::Comment>> comments; // postId -> comments
    int nextId = 1000;
    std::string makeId();
};

} // namespace thomaz
```

- [ ] **Step 2: Implementação**

```cpp
#include "platform/feed/fake_feed_client.hpp"

namespace thomaz {

namespace {
constexpr int kPageSize = 8;
} // namespace

FakeFeedClient::FakeFeedClient()
{
    // Semente: 25 posts sintéticos, mais novos primeiro.
    const char* users[] = { "joao", "bea", "lucas", "mari", "kai" };
    const std::uint64_t games[] = { 0x0100000000010000ULL, 0x010000000E5EE000ULL,
                                    0x0100ABCDEF000000ULL };
    const char* names[] = { "Super Mario Odyssey", "8-BIT Demo", "Fake Quest" };
    for (int i = 0; i < 25; ++i) {
        feed::Post p;
        p.id          = "seed-" + std::to_string(i);
        p.author      = { std::string("u") + std::to_string(i % 5), users[i % 5] };
        p.imageUrl    = "fake://seed/" + std::to_string(i);
        p.caption     = "Post de exemplo #" + std::to_string(i);
        p.gameTitleId = games[i % 3];
        p.gameName    = names[i % 3];
        p.likeCount   = (i * 7) % 50;
        p.likedByMe   = false;
        p.commentCount = i % 4;
        p.createdAt   = 1780000000 - i * 3600;
        posts.push_back(p);
    }
}

std::string FakeFeedClient::makeId()
{
    return "p" + std::to_string(nextId++);
}

AuthResult FakeFeedClient::registerUser(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

AuthResult FakeFeedClient::login(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

feed::FeedPage FakeFeedClient::fetchFeed(const std::string& cursor)
{
    // cursor = índice de início (string). vazio = 0.
    size_t start = cursor.empty() ? 0 : static_cast<size_t>(std::stoul(cursor));
    feed::FeedPage page;
    size_t end = std::min(start + kPageSize, posts.size());
    for (size_t i = start; i < end; ++i)
        page.posts.push_back(posts[i]);
    page.hasMore    = end < posts.size();
    page.nextCursor = page.hasMore ? std::to_string(end) : "";
    return page;
}

ActionResult FakeFeedClient::createPost(const std::string&,
                                        const std::vector<std::uint8_t>&,
                                        const std::string& caption,
                                        std::uint64_t gameTitleId,
                                        const std::string& gameName)
{
    feed::Post p;
    p.id          = makeId();
    p.author      = { "me", "voce" };
    p.imageUrl    = "fake://new";
    p.caption     = caption;
    p.gameTitleId = gameTitleId;
    p.gameName    = gameName;
    p.createdAt   = 1780100000;
    posts.insert(posts.begin(), p); // mais novo no topo
    return { true, "" };
}

ActionResult FakeFeedClient::setLike(const std::string&,
                                     const std::string& postId, bool liked)
{
    for (auto& p : posts)
        if (p.id == postId) {
            if (liked && !p.likedByMe) { p.likedByMe = true; p.likeCount++; }
            else if (!liked && p.likedByMe) { p.likedByMe = false; p.likeCount--; }
            return { true, "" };
        }
    return { false, "post não encontrado" };
}

std::vector<feed::Comment> FakeFeedClient::fetchComments(const std::string& postId)
{
    auto it = comments.find(postId);
    if (it != comments.end())
        return it->second;
    return {};
}

ActionResult FakeFeedClient::addComment(const std::string&,
                                        const std::string& postId, const std::string& text)
{
    if (text.empty())
        return { false, "comentário vazio" };
    feed::Comment c;
    c.id        = makeId();
    c.author    = { "me", "voce" };
    c.text      = text;
    c.createdAt = 1780100000;
    comments[postId].push_back(c);
    for (auto& p : posts)
        if (p.id == postId) { p.commentCount++; break; }
    return { true, "" };
}

} // namespace thomaz
```

- [ ] **Step 3: Build desktop**

Run: `./scripts/build-desktop.sh`
Expected: build OK.

- [ ] **Step 4: Commit**

```bash
git add source/platform/feed/fake_feed_client.hpp source/platform/feed/fake_feed_client.cpp
git commit -m "feat(feed): in-memory FakeFeedClient"
```

---

## FASE 3 — i18n + Home (entrada do app)

### Task 11: Chaves de i18n

**Files:**
- Modify: `resources/i18n/en-US/thomaz.json`
- Modify: `resources/i18n/pt-BR/thomaz.json`

- [ ] **Step 1: Adicionar o bloco `module.feed` e as seções `feed`/`auth`/`composer` (en-US)**

Em `resources/i18n/en-US/thomaz.json`, dentro de `"module"` adicionar a chave `feed`, e no topo do objeto raiz adicionar as três seções novas:

```json
    "module": {
        "feed": {
            "title": "Community",
            "subtitle": "Share your Switch screenshots"
        },
        "cheats": {
            "title": "Cheats",
            "subtitle": "Manage your games' cheats"
        },
        "settings": {
            "title": "Settings"
        }
    },
    "feed": {
        "title": "Community",
        "compose": "+ Post",
        "empty": "No posts yet.",
        "error": "Couldn't load the feed.",
        "retry": "Try again",
        "loading_more": "Loading…",
        "like": "Like",
        "comments": "Comments",
        "add_comment": "Write a comment…",
        "send": "Send",
        "game_tag": "From: ",
        "login_required": "Log in to interact."
    },
    "auth": {
        "login_tab": "Log in",
        "register_tab": "Create account",
        "username": "Username",
        "password": "Password",
        "submit_login": "Log in",
        "submit_register": "Create account",
        "logout": "Log out",
        "err_empty": "Fill in username and password.",
        "err_failed": "Couldn't sign you in."
    },
    "composer": {
        "title": "New post",
        "pick": "Pick a capture",
        "album_empty": "Your album is empty.",
        "caption": "Write a caption…",
        "post": "Post",
        "posting": "Posting…",
        "post_failed": "Couldn't post — your caption was kept."
    },
```

- [ ] **Step 2: Mesmo para pt-BR**

Em `resources/i18n/pt-BR/thomaz.json`, espelhar:

```json
    "module": {
        "feed": {
            "title": "Comunidade",
            "subtitle": "Compartilhe suas screenshots do Switch"
        },
        "cheats": {
            "title": "Trapaças",
            "subtitle": "Gerencie os cheats dos seus jogos"
        },
        "settings": {
            "title": "Configurações"
        }
    },
    "feed": {
        "title": "Comunidade",
        "compose": "+ Postar",
        "empty": "Ainda não há posts.",
        "error": "Não foi possível carregar o feed.",
        "retry": "Tentar de novo",
        "loading_more": "Carregando…",
        "like": "Curtir",
        "comments": "Comentários",
        "add_comment": "Escreva um comentário…",
        "send": "Enviar",
        "game_tag": "De: ",
        "login_required": "Entre para interagir."
    },
    "auth": {
        "login_tab": "Entrar",
        "register_tab": "Criar conta",
        "username": "Usuário",
        "password": "Senha",
        "submit_login": "Entrar",
        "submit_register": "Criar conta",
        "logout": "Sair",
        "err_empty": "Preencha usuário e senha.",
        "err_failed": "Não foi possível entrar."
    },
    "composer": {
        "title": "Novo post",
        "pick": "Escolha uma captura",
        "album_empty": "Seu álbum está vazio.",
        "caption": "Escreva uma legenda…",
        "post": "Postar",
        "posting": "Postando…",
        "post_failed": "Falha ao postar — sua legenda foi mantida."
    },
```

- [ ] **Step 3: Validar JSON**

Run: `python3 -c "import json,sys; [json.load(open(p)) for p in ['resources/i18n/en-US/thomaz.json','resources/i18n/pt-BR/thomaz.json']]; print('ok')"`
Expected: `ok` (sem erro de parse — vírgulas no lugar certo).

- [ ] **Step 4: Commit**

```bash
git add resources/i18n/en-US/thomaz.json resources/i18n/pt-BR/thomaz.json
git commit -m "i18n(feed): add feed/auth/composer strings"
```

---

### Task 12: Reorganizar a Home (Feed vira hero, Cheats vira card menor)

**Files:**
- Modify: `resources/xml/activity/home.xml`
- Modify: `source/app/home_activity.cpp:18-39`

- [ ] **Step 1: Editar `home.xml` — hero passa a ser Feed**

No bloco do hero (`id="trapacasCard"`), renomear o id para `feedCard` e trocar os textos para o módulo feed:

```xml
            <brls:Box id="feedCard"
                      axis="column" grow="1.55" marginRight="18"
                      padding="34" cornerRadius="22"
                      backgroundColor="@theme/thomaz/tile_cheats"
                      focusable="true" highlightCornerRadius="22"
                      hideHighlightBackground="true">

                <brls:Box width="64" height="64" cornerRadius="18"
                          backgroundColor="#FFFFFF29"
                          justifyContent="center" alignItems="center">
                    <brls:Label text="@i18n/thomaz/icon/cheats"
                                fontSize="36" textColor="#FFFFFF"/>
                </brls:Box>

                <brls:Box grow="1.0"/>

                <brls:Label text="@i18n/thomaz/module/feed/title"
                            fontSize="40" textColor="#FFFFFF"/>
                <brls:Label text="@i18n/thomaz/module/feed/subtitle"
                            fontSize="17" textColor="#FFFFFFD9"
                            marginTop="8" width="360"/>
            </brls:Box>
```

- [ ] **Step 2: Editar `home.xml` — o card "Save Manager" vira Cheats (ativo)**

Substituir o bloco "Save Manager (coming soon)" por um card de Cheats focável com `id="cheatsCard"` (remove o badge "em breve" e o cadeado):

```xml
                <!-- Cheats (ativo) — antes era o slot do Save Manager -->
                <brls:Box id="cheatsCard"
                          axis="row" alignItems="center" grow="1.0" marginBottom="14"
                          paddingLeft="22" paddingRight="22" cornerRadius="16"
                          backgroundColor="@theme/thomaz/tile_saves"
                          focusable="true" highlightCornerRadius="16"
                          hideHighlightBackground="true">
                    <brls:Box width="44" height="44" cornerRadius="12"
                              backgroundColor="#FFFFFF29"
                              justifyContent="center" alignItems="center" marginRight="16">
                        <brls:Label text="@i18n/thomaz/icon/cheats"
                                    fontSize="24" textColor="#FFFFFF"/>
                    </brls:Box>
                    <brls:Label text="@i18n/thomaz/module/cheats/title"
                                fontSize="17" textColor="#FFFFFF" grow="1.0"/>
                </brls:Box>
```

(Os cards de "Settings (active)" e "Mods (coming soon)" ficam inalterados.)

- [ ] **Step 3: Editar `home_activity.cpp` — navegação dos cards**

Trocar o corpo de `onContentAvailable()` para apontar o hero ao Feed e o novo card ao GameList. Como o `FeedActivity` ainda não existe nesta task, este passo só será compilável após a Task 14 — então **adie a compilação**: faça a edição agora, mas o build/commit desta task é só do XML. A edição completa do `.cpp` entra na Task 14, Step de wiring.

> Para manter cada task compilável: nesta Task 12, **só** edite o `home.xml` (Steps 1–2) e commite. A reescrita do `home_activity.cpp` acontece na Task 14 junto com o include do `FeedActivity`.

- [ ] **Step 4: Build desktop (XML novo, .cpp ainda aponta `trapacasCard` que não existe mais — então pular build aqui)**

Não buildar nesta task (o `getView("trapacasCard")` retornaria nullptr e o `registerClickAction` quebraria em runtime). Seguir direto pro commit do XML; o app volta a funcionar na Task 14.

- [ ] **Step 5: Commit (só o XML)**

```bash
git add resources/xml/activity/home.xml
git commit -m "feat(home): make Feed the hero, Cheats a rail card (xml)"
```

---

## FASE 4 — AuthActivity (login/cadastro)

### Task 13: AuthActivity

**Files:**
- Create: `resources/xml/activity/auth.xml`
- Create: `source/app/auth_activity.hpp`
- Create: `source/app/auth_activity.cpp`

- [ ] **Step 1: XML**

```xml
<thomaz:SlideFrame title="@i18n/thomaz/auth/login_tab" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0"
              paddingTop="36" paddingBottom="28" paddingLeft="56" paddingRight="56"
              justifyContent="center" alignItems="center">

        <brls:Box id="authForm" axis="column" width="420" height="auto">

            <brls:Box id="tabsRow" axis="row" marginBottom="20" />

            <brls:InputCell id="usernameCell" />
            <brls:InputCell id="passwordCell" marginTop="10" />

            <brls:Box id="submitBtn"
                      axis="row" justifyContent="center" alignItems="center"
                      height="52" marginTop="20" cornerRadius="14"
                      backgroundColor="@theme/thomaz/tile_cheats"
                      focusable="true" highlightCornerRadius="14"
                      hideHighlightBackground="true">
                <brls:Label id="submitLabel" text="@i18n/thomaz/auth/submit_login"
                            fontSize="18" textColor="#FFFFFF"/>
            </brls:Box>

            <brls:Label id="authStatus" text="" fontSize="14"
                        marginTop="14" horizontalAlign="center"
                        textColor="@theme/thomaz/text_dim"/>
        </brls:Box>
    </brls:Box>
</thomaz:SlideFrame>
```

- [ ] **Step 2: Header**

```cpp
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <borealis.hpp>
#include "platform/feed/feed_client.hpp"

namespace thomaz {

// Tela de login/cadastro. Recebe o IFeedClient e um callback chamado ao
// autenticar com sucesso (a sessão já foi persistida) para a tela que pediu
// login retomar sua ação.
class AuthActivity : public brls::Activity {
  public:
    AuthActivity(IFeedClient* client, std::function<void()> onAuthed);
    ~AuthActivity() override;

    CONTENT_FROM_XML_RES("activity/auth.xml");
    void onContentAvailable() override;

  private:
    void refreshMode();                 // atualiza textos conforme login/cadastro
    void submit();                      // dispara register/login em async

    IFeedClient* client;
    std::function<void()> onAuthed;
    bool registerMode = false;
    bool busy = false;
    std::string username, password;
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Implementação**

```cpp
#include "app/auth_activity.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

AuthActivity::AuthActivity(IFeedClient* client, std::function<void()> onAuthed)
    : client(client), onAuthed(std::move(onAuthed)) {}

AuthActivity::~AuthActivity() { *this->alive = false; }

void AuthActivity::onContentAvailable()
{
    auto* userCell = (brls::InputCell*)this->getView("usernameCell");
    auto* passCell = (brls::InputCell*)this->getView("passwordCell");

    userCell->init("thomaz/auth/username"_i18n, "", [this](std::string v){ this->username = v; },
                   "thomaz/auth/username"_i18n, "", 32);
    passCell->init("thomaz/auth/password"_i18n, "", [this](std::string v){ this->password = v; },
                   "thomaz/auth/password"_i18n, "", 64);

    // Duas abas (Entrar | Criar conta) como dois botões que alternam o modo.
    auto* tabsRow = (brls::Box*)this->getView("tabsRow");
    auto makeTab = [this](const std::string& text, bool regMode) {
        auto* tab = new brls::Box(brls::Axis::ROW);
        tab->setFocusable(true);
        tab->setHeight(40.0f);
        tab->setGrow(1.0f);
        tab->setCornerRadius(10.0f);
        tab->setJustifyContent(brls::JustifyContent::CENTER);
        tab->setAlignItems(brls::AlignItems::CENTER);
        tab->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
        auto* lbl = new brls::Label();
        lbl->setText(text);
        lbl->setFontSize(15.0f);
        tab->addView(lbl);
        tab->registerClickAction([this, regMode](brls::View*) {
            this->registerMode = regMode;
            this->refreshMode();
            return true;
        });
        tab->addGestureRecognizer(new brls::TapGestureRecognizer(tab));
        return tab;
    };
    tabsRow->addView(makeTab("thomaz/auth/login_tab"_i18n, false));
    auto* spacer = new brls::Box(); spacer->setWidth(10.0f); tabsRow->addView(spacer);
    tabsRow->addView(makeTab("thomaz/auth/register_tab"_i18n, true));

    auto* submit = this->getView("submitBtn");
    submit->registerClickAction([this](brls::View*) { this->submit(); return true; });
    submit->addGestureRecognizer(new brls::TapGestureRecognizer(submit));

    this->refreshMode();
}

void AuthActivity::refreshMode()
{
    auto* submitLabel = (brls::Label*)this->getView("submitLabel");
    submitLabel->setText(this->registerMode ? "thomaz/auth/submit_register"_i18n
                                            : "thomaz/auth/submit_login"_i18n);
}

void AuthActivity::submit()
{
    if (this->busy) return;
    auto* status = (brls::Label*)this->getView("authStatus");

    if (this->username.empty() || this->password.empty()) {
        status->setText("thomaz/auth/err_empty"_i18n);
        return;
    }

    this->busy = true;
    status->setText("…");

    IFeedClient* c = this->client;
    auto alive     = this->alive;
    std::string u = this->username, p = this->password;
    bool reg = this->registerMode;

    brls::async([this, c, alive, u, p, reg, status]() {
        AuthResult r = reg ? c->registerUser(u, p) : c->login(u, p);
        brls::sync([this, alive, r, u, status]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                status->setText(r.error.empty() ? "thomaz/auth/err_failed"_i18n : r.error);
                return;
            }
            save_session(feed::Session{ r.token, u });
            auto cb = this->onAuthed;
            brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                           [cb]() { if (cb) cb(); });
        });
    });
}

} // namespace thomaz
```

- [ ] **Step 4: Build desktop**

Run: `./scripts/build-desktop.sh`
Expected: build OK (AuthActivity ainda não é referenciada por ninguém, mas compila).

- [ ] **Step 5: Commit**

```bash
git add resources/xml/activity/auth.xml source/app/auth_activity.hpp source/app/auth_activity.cpp
git commit -m "feat(feed): AuthActivity (login/register)"
```

---

## FASE 5 — FeedActivity (master-detail)

### Task 14: FeedActivity + wiring no main e na Home

**Files:**
- Create: `resources/xml/activity/feed.xml`
- Create: `source/app/feed_activity.hpp`
- Create: `source/app/feed_activity.cpp`
- Modify: `source/app/home_activity.cpp`
- Modify: `source/app/home_activity.hpp`
- Modify: `source/main.cpp`

- [ ] **Step 1: XML do feed (split: lista à esquerda, detalhe à direita)**

```xml
<thomaz:SlideFrame title="@i18n/thomaz/feed/title" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0" paddingTop="16"
              paddingLeft="40" paddingRight="40" paddingBottom="16">

        <!-- topo: botão postar -->
        <brls:Box axis="row" marginBottom="12">
            <brls:Box grow="1.0"/>
            <brls:Box id="composeBtn" axis="row" alignItems="center" justifyContent="center"
                      height="40" paddingLeft="18" paddingRight="18" cornerRadius="12"
                      backgroundColor="@theme/thomaz/tile_cheats"
                      focusable="true" highlightCornerRadius="12" hideHighlightBackground="true">
                <brls:Label text="@i18n/thomaz/feed/compose" fontSize="15" textColor="#FFFFFF"/>
            </brls:Box>
        </brls:Box>

        <!-- corpo: lista (esq) + detalhe (dir) -->
        <brls:Box axis="row" grow="1.0">

            <brls:ScrollingFrame width="auto" height="auto" grow="1.4" marginRight="14">
                <brls:Box id="feedListBox" width="auto" height="auto" axis="column"/>
            </brls:ScrollingFrame>

            <brls:Box id="detailPane" axis="column" grow="1.0"
                      cornerRadius="14" padding="16"
                      backgroundColor="@theme/thomaz/tile_saves"/>
        </brls:Box>

        <brls:ProgressSpinner id="feedSpinner" width="44" height="44"
                              marginTop="24" alignSelf="center"/>
        <brls:Label id="feedEmpty" visibility="gone" text="@i18n/thomaz/feed/empty"
                    fontSize="18" horizontalAlign="center" marginTop="24"/>
        <brls:Box id="feedError" visibility="gone" axis="column" alignItems="center" marginTop="24">
            <brls:Label text="@i18n/thomaz/feed/error" fontSize="16"/>
            <brls:Box id="feedRetry" axis="row" justifyContent="center" alignItems="center"
                      height="44" paddingLeft="20" paddingRight="20" marginTop="12" cornerRadius="12"
                      backgroundColor="@theme/thomaz/tile_settings"
                      focusable="true" highlightCornerRadius="12" hideHighlightBackground="true">
                <brls:Label text="@i18n/thomaz/feed/retry" fontSize="15" textColor="#FFFFFF"/>
            </brls:Box>
        </brls:Box>
    </brls:Box>
</thomaz:SlideFrame>
```

- [ ] **Step 2: Header**

```cpp
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <borealis.hpp>
#include "platform/feed/feed_client.hpp"
#include "platform/feed/album_source.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Feed master-detail: lista de posts (esq) + painel de detalhe/comentários (dir).
// Scroll infinito por cursor. Curtir/comentar exigem sessão (senão abre Auth).
class FeedActivity : public brls::Activity {
  public:
    FeedActivity(IFeedClient* client, IAlbumSource* album, ITitleService* titles);
    ~FeedActivity() override;

    CONTENT_FROM_XML_RES("activity/feed.xml");
    void onContentAvailable() override;

  private:
    void loadFirstPage();
    void loadNextPage();
    void renderNewRows(size_t fromIndex);     // adiciona linhas a partir do índice
    void showDetail(const std::string& postId);
    void onComposePressed();
    bool requireSession();                    // true se logado; senão abre Auth e retorna false

    IFeedClient*   client;
    IAlbumSource*  album;
    ITitleService* titles;

    std::vector<feed::Post> posts;            // acumulado (merge_feed_page)
    std::string nextCursor;
    bool hasMore = false;
    bool loading = false;
    std::string selectedId;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Implementação**

```cpp
#include "app/feed_activity.hpp"
#include "app/composer_activity.hpp"
#include "app/auth_activity.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include "core/feed/feed_pagination.hpp"
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

FeedActivity::FeedActivity(IFeedClient* client, IAlbumSource* album, ITitleService* titles)
    : client(client), album(album), titles(titles) {}

FeedActivity::~FeedActivity() { *this->alive = false; }

void FeedActivity::onContentAvailable()
{
    auto* compose = this->getView("composeBtn");
    compose->registerClickAction([this](brls::View*) { this->onComposePressed(); return true; });
    compose->addGestureRecognizer(new brls::TapGestureRecognizer(compose));

    auto* retry = this->getView("feedRetry");
    retry->registerClickAction([this](brls::View*) { this->loadFirstPage(); return true; });
    retry->addGestureRecognizer(new brls::TapGestureRecognizer(retry));

    this->loadFirstPage();
}

void FeedActivity::loadFirstPage()
{
    this->posts.clear();
    this->nextCursor.clear();
    if (auto* box = (brls::Box*)this->getView("feedListBox")) box->clearViews();
    this->getView("feedError")->setVisibility(brls::Visibility::GONE);
    this->getView("feedEmpty")->setVisibility(brls::Visibility::GONE);
    this->getView("feedSpinner")->setVisibility(brls::Visibility::VISIBLE);

    IFeedClient* c = this->client;
    auto alive     = this->alive;

    brls::async([this, c, alive]() {
        feed::FeedPage page = c->fetchFeed("");
        brls::sync([this, alive, page]() {
            if (!alive->load()) return;
            this->getView("feedSpinner")->setVisibility(brls::Visibility::GONE);

            bool transportFail = page.posts.empty() && !page.hasMore && page.nextCursor.empty();
            // No fake nunca falha; deixamos o caminho de erro para o HttpFeedClient.
            size_t before = this->posts.size();
            this->hasMore = merge_feed_page(this->posts, page);
            this->nextCursor = page.nextCursor;

            if (this->posts.empty()) {
                if (transportFail)
                    this->getView("feedError")->setVisibility(brls::Visibility::VISIBLE);
                else
                    this->getView("feedEmpty")->setVisibility(brls::Visibility::VISIBLE);
                return;
            }
            this->renderNewRows(before);
            this->showDetail(this->posts.front().id);
        });
    });
}

void FeedActivity::loadNextPage()
{
    if (this->loading || !this->hasMore) return;
    this->loading = true;

    IFeedClient* c = this->client;
    auto alive     = this->alive;
    std::string cur = this->nextCursor;

    brls::async([this, c, alive, cur]() {
        feed::FeedPage page = c->fetchFeed(cur);
        brls::sync([this, alive, page]() {
            if (!alive->load()) return;
            this->loading = false;
            size_t before = this->posts.size();
            this->hasMore = merge_feed_page(this->posts, page);
            this->nextCursor = page.nextCursor;
            this->renderNewRows(before);
        });
    });
}

void FeedActivity::renderNewRows(size_t fromIndex)
{
    auto* listBox = (brls::Box*)this->getView("feedListBox");
    if (!listBox) return;

    for (size_t i = fromIndex; i < this->posts.size(); ++i) {
        const feed::Post& post = this->posts[i];
        std::string id = post.id;

        auto* row = new brls::Box(brls::Axis::COLUMN);
        row->setFocusable(true);
        row->setMarginBottom(8.0f);
        row->setPadding(10.0f, 12.0f, 10.0f, 12.0f);
        row->setCornerRadius(12.0f);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23)); // surface_1

        auto* user = new brls::Label();
        user->setText("@" + post.author.username);
        user->setFontSize(15.0f);
        row->addView(user);

        if (!post.caption.empty()) {
            auto* cap = new brls::Label();
            cap->setText(post.caption);
            cap->setFontSize(14.0f);
            cap->setTextColor(nvgRGB(0xC9, 0xCA, 0xD1));
            cap->setMarginTop(4.0f);
            row->addView(cap);
        }

        auto* meta = new brls::Label();
        meta->setText("♥ " + std::to_string(post.likeCount) +
                      "   💬 " + std::to_string(post.commentCount));
        meta->setFontSize(13.0f);
        meta->setTextColor(nvgRGB(0x8b, 0x8d, 0x98));
        meta->setMarginTop(6.0f);
        row->addView(meta);

        // Focar/selecionar a linha atualiza o painel de detalhe.
        row->registerClickAction([this, id](brls::View*) { this->showDetail(id); return true; });
        row->getFocusEvent()->subscribe([this, id](brls::View*) { this->showDetail(id); });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));

        listBox->addView(row);

        // Pré-carrega a próxima página quando faltam poucas linhas (scroll infinito).
        if (i + 3 >= this->posts.size())
            this->loadNextPage();
    }
}

void FeedActivity::showDetail(const std::string& postId)
{
    if (this->selectedId == postId) return;
    this->selectedId = postId;

    auto* pane = (brls::Box*)this->getView("detailPane");
    if (!pane) return;
    pane->clearViews();

    feed::Post* post = find_post(this->posts, postId);
    if (!post) return;

    auto* user = new brls::Label();
    user->setText("@" + post->author.username);
    user->setFontSize(18.0f);
    pane->addView(user);

    if (post->gameTitleId != 0 && !post->gameName.empty()) {
        auto* game = new brls::Label();
        game->setText("thomaz/feed/game_tag"_i18n + post->gameName);
        game->setFontSize(13.0f);
        game->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
        game->setMarginTop(4.0f);
        pane->addView(game);
    }

    if (!post->caption.empty()) {
        auto* cap = new brls::Label();
        cap->setText(post->caption);
        cap->setFontSize(15.0f);
        cap->setMarginTop(10.0f);
        pane->addView(cap);
    }

    // Botão curtir (otimista).
    auto* likeBtn = new brls::Box(brls::Axis::ROW);
    likeBtn->setFocusable(true);
    likeBtn->setHeight(40.0f);
    likeBtn->setMarginTop(14.0f);
    likeBtn->setPadding(6.0f, 14.0f, 6.0f, 14.0f);
    likeBtn->setCornerRadius(10.0f);
    likeBtn->setAlignItems(brls::AlignItems::CENTER);
    likeBtn->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
    auto* likeLbl = new brls::Label();
    likeLbl->setText((post->likedByMe ? "♥ " : "♡ ") + std::to_string(post->likeCount));
    likeLbl->setFontSize(15.0f);
    likeBtn->addView(likeLbl);
    likeBtn->registerClickAction([this, postId, likeLbl](brls::View*) {
        if (!this->requireSession()) return true;
        feed::Post* p = find_post(this->posts, postId);
        if (!p) return true;
        bool target = !p->likedByMe;
        // otimista
        p->likedByMe = target; p->likeCount += target ? 1 : -1;
        likeLbl->setText((target ? "♥ " : "♡ ") + std::to_string(p->likeCount));

        auto sess = load_session();
        std::string token = sess ? sess->token : "";
        IFeedClient* c = this->client; auto alive = this->alive;
        brls::async([this, c, alive, token, postId, target, likeLbl]() {
            ActionResult r = c->setLike(token, postId, target);
            brls::sync([this, alive, r, postId, target, likeLbl]() {
                if (!alive->load()) return;
                if (!r.ok) { // reverte
                    feed::Post* p = find_post(this->posts, postId);
                    if (p) { p->likedByMe = !target; p->likeCount += target ? -1 : 1;
                             likeLbl->setText((p->likedByMe ? "♥ " : "♡ ") + std::to_string(p->likeCount)); }
                }
            });
        });
        return true;
    });
    likeBtn->addGestureRecognizer(new brls::TapGestureRecognizer(likeBtn));
    pane->addView(likeBtn);

    // Lista de comentários (carregada async) + botão adicionar.
    auto* commentsBox = new brls::Box(brls::Axis::COLUMN);
    commentsBox->setMarginTop(14.0f);
    pane->addView(commentsBox);

    IFeedClient* c = this->client; auto alive = this->alive;
    brls::async([this, c, alive, postId, commentsBox]() {
        auto list = c->fetchComments(postId);
        brls::sync([this, alive, list, commentsBox, postId]() {
            if (!alive->load()) return;
            if (this->selectedId != postId) return; // trocou de post enquanto carregava
            commentsBox->clearViews();
            for (const auto& cm : list) {
                auto* l = new brls::Label();
                l->setText("@" + cm.author.username + ": " + cm.text);
                l->setFontSize(13.0f);
                l->setMarginBottom(4.0f);
                commentsBox->addView(l);
            }
        });
    });

    auto* addBtn = new brls::Box(brls::Axis::ROW);
    addBtn->setFocusable(true);
    addBtn->setHeight(40.0f);
    addBtn->setMarginTop(10.0f);
    addBtn->setPadding(6.0f, 14.0f, 6.0f, 14.0f);
    addBtn->setCornerRadius(10.0f);
    addBtn->setAlignItems(brls::AlignItems::CENTER);
    addBtn->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
    auto* addLbl = new brls::Label(); addLbl->setText("thomaz/feed/add_comment"_i18n);
    addLbl->setFontSize(14.0f); addBtn->addView(addLbl);
    addBtn->registerClickAction([this, postId, commentsBox](brls::View*) {
        if (!this->requireSession()) return true;
        brls::Application::getImeManager()->openForText(
            [this, postId, commentsBox](std::string text) {
                if (text.empty()) return;
                auto sess = load_session();
                std::string token = sess ? sess->token : "";
                IFeedClient* c = this->client; auto alive = this->alive;
                brls::async([this, c, alive, token, postId, text, commentsBox]() {
                    ActionResult r = c->addComment(token, postId, text);
                    brls::sync([this, alive, r, postId, commentsBox]() {
                        if (!alive->load()) return;
                        if (r.ok && this->selectedId == postId) this->showDetail(postId);
                    });
                });
            },
            "thomaz/feed/add_comment"_i18n, "", 280);
        return true;
    });
    addBtn->addGestureRecognizer(new brls::TapGestureRecognizer(addBtn));
    pane->addView(addBtn);
}

bool FeedActivity::requireSession()
{
    if (load_session().has_value())
        return true;
    brls::Application::pushActivity(new AuthActivity(this->client, []() {}));
    return false;
}

void FeedActivity::onComposePressed()
{
    if (!this->requireSession()) return;
    auto alive = this->alive;
    brls::Application::pushActivity(new ComposerActivity(
        this->client, this->album, this->titles,
        [this, alive]() { if (alive->load()) this->loadFirstPage(); }));
}

} // namespace thomaz
```

- [ ] **Step 4: Reescrever `home_activity.cpp` para navegar Feed/Cheats**

Substituir o `onContentAvailable()` inteiro:

```cpp
#include "app/home_activity.hpp"
#include "app/game_list_activity.hpp"
#include "app/feed_activity.hpp"
#include "app/settings_activity.hpp"

#include <borealis.hpp>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http,
                           IFeedClient* feed, IAlbumSource* album)
    : titleService(titleService), http(http), feed(feed), album(album) {}

void HomeActivity::onContentAvailable()
{
    // Hero → Feed da Comunidade.
    if (brls::View* feedCard = this->getView("feedCard")) {
        feedCard->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(
                new FeedActivity(this->feed, this->album, this->titleService));
            return true;
        });
        feedCard->addGestureRecognizer(new brls::TapGestureRecognizer(feedCard));
    }

    // Card menor → Cheats (game list).
    if (brls::View* cheatsCard = this->getView("cheatsCard")) {
        cheatsCard->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new GameListActivity(this->titleService, this->http));
            return true;
        });
        cheatsCard->addGestureRecognizer(new brls::TapGestureRecognizer(cheatsCard));
    }

    // Configurações.
    if (brls::View* settings = this->getView("settingsCard")) {
        settings->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new SettingsActivity(this->http));
            return true;
        });
        settings->addGestureRecognizer(new brls::TapGestureRecognizer(settings));
    }
}

} // namespace thomaz
```

- [ ] **Step 5: Atualizar `home_activity.hpp` (novos membros injetados)**

Adicionar os includes e os parâmetros/membros `feed`/`album` ao construtor e à classe:

```cpp
// no topo, junto dos outros includes:
#include "platform/feed/feed_client.hpp"
#include "platform/feed/album_source.hpp"

// assinatura do construtor:
HomeActivity(ITitleService* titleService, IHttpClient* http,
             IFeedClient* feed, IAlbumSource* album);

// membros privados (junto de titleService/http):
IFeedClient*  feed;
IAlbumSource* album;
```

- [ ] **Step 6: Wire no `main.cpp`**

Adicionar includes e instanciar os serviços do feed; passar à HomeActivity. No bloco de seleção por plataforma:

```cpp
// includes novos no topo do main.cpp:
#include "platform/feed/fake_feed_client.hpp"
#include "platform/feed/fake_album_source.hpp"
#ifdef __SWITCH__
#include "platform/feed/switch_album_source.hpp"
#endif

// ... depois de criar titleService e httpClient:

    // Feed: FakeFeedClient por enquanto (a API real é futura) em ambas as
    // plataformas. Álbum: real no Switch, fake no desktop.
    auto feedClient = std::make_unique<thomaz::FakeFeedClient>();

#ifdef __SWITCH__
    auto albumSource = std::make_unique<thomaz::SwitchAlbumSource>();
    albumSource->init();
#else
    auto albumSource = std::make_unique<thomaz::FakeAlbumSource>();
#endif

    brls::Application::pushActivity(new thomaz::HomeActivity(
        titleService.get(), httpClient.get(), feedClient.get(), albumSource.get()));
```

E no shutdown (Switch), antes do `return`:

```cpp
#ifdef __SWITCH__
    albumSource->exit();
    titleService->exit();
#endif
```

(Remover o `titleService->exit();` antigo duplicado — deixar só este bloco.)

> A `ComposerActivity` referenciada por `feed_activity.cpp` é criada na Task 15. Para esta task compilar, implemente a Task 15 antes do build/commit, **ou** faça os Steps 1–6 e siga direto pra Task 15, buildando e commitando as duas juntas. Recomendado: implementar Task 15 e então buildar.

- [ ] **Step 7: (após Task 15) Build desktop e validar**

Run: `./scripts/build-desktop.sh && ./build_desktop/thomaz`
Expected: app abre na Home; hero "Comunidade" abre o feed com 8 posts; rolar carrega mais; focar um post atualiza o painel direito.

- [ ] **Step 8: Commit**

```bash
git add resources/xml/activity/feed.xml source/app/feed_activity.hpp source/app/feed_activity.cpp \
        source/app/home_activity.hpp source/app/home_activity.cpp source/main.cpp
git commit -m "feat(feed): FeedActivity master-detail + home/main wiring"
```

---

## FASE 6 — ComposerActivity (split)

### Task 15: ComposerActivity

**Files:**
- Create: `resources/xml/activity/composer.xml`
- Create: `source/app/composer_activity.hpp`
- Create: `source/app/composer_activity.cpp`

- [ ] **Step 1: XML (grid à esquerda, preview + legenda à direita)**

```xml
<thomaz:SlideFrame title="@i18n/thomaz/composer/title" iconInterpolation="linear">
    <brls:Box axis="row" grow="1.0" paddingTop="16"
              paddingLeft="40" paddingRight="40" paddingBottom="16">

        <!-- esquerda: grid do álbum -->
        <brls:Box axis="column" grow="1.1" marginRight="16">
            <brls:Label text="@i18n/thomaz/composer/pick" fontSize="16" marginBottom="10"/>
            <!-- Borealis Box não suporta flex-wrap; o grid é montado como uma
                 coluna de linhas (2 thumbs por linha) no loadAlbum(). -->
            <brls:ScrollingFrame width="auto" height="auto" grow="1.0">
                <brls:Box id="albumGrid" width="auto" height="auto" axis="column"/>
            </brls:ScrollingFrame>
            <brls:ProgressSpinner id="albumSpinner" width="40" height="40"
                                  marginTop="20" alignSelf="center"/>
            <brls:Label id="albumEmpty" visibility="gone"
                        text="@i18n/thomaz/composer/album_empty"
                        fontSize="15" marginTop="20" horizontalAlign="center"/>
        </brls:Box>

        <!-- direita: preview + legenda + postar -->
        <brls:Box axis="column" grow="1.0">
            <brls:Image id="previewImage" width="auto" height="200" cornerRadius="10"/>
            <brls:Label id="previewGame" text="" fontSize="13" marginTop="8"
                        textColor="@theme/thomaz/text_dim"/>
            <brls:InputCell id="captionCell" marginTop="10"/>
            <brls:Box id="postBtn" axis="row" justifyContent="center" alignItems="center"
                      height="50" marginTop="14" cornerRadius="12"
                      backgroundColor="@theme/thomaz/tile_cheats"
                      focusable="true" highlightCornerRadius="12" hideHighlightBackground="true">
                <brls:Label text="@i18n/thomaz/composer/post" fontSize="17" textColor="#FFFFFF"/>
            </brls:Box>
            <brls:Label id="composerStatus" text="" fontSize="14" marginTop="12"/>
        </brls:Box>
    </brls:Box>
</thomaz:SlideFrame>
```

- [ ] **Step 2: Header**

```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <borealis.hpp>
#include "platform/feed/feed_client.hpp"
#include "platform/feed/album_source.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Compositor split: grid do Álbum (esq) + preview/legenda/postar (dir).
// Ao selecionar uma captura, resolve o jogo (titleId → nome) via ITitleService.
class ComposerActivity : public brls::Activity {
  public:
    ComposerActivity(IFeedClient* client, IAlbumSource* album, ITitleService* titles,
                     std::function<void()> onPosted);
    ~ComposerActivity() override;

    CONTENT_FROM_XML_RES("activity/composer.xml");
    void onContentAvailable() override;

  private:
    void loadAlbum();
    void selectEntry(const AlbumEntry& entry);
    void doPost();
    std::string resolveGameName(std::uint64_t titleId);

    IFeedClient*   client;
    IAlbumSource*  album;
    ITitleService* titles;
    std::function<void()> onPosted;

    std::string selectedEntryId;
    std::uint64_t selectedTitleId = 0;
    std::string selectedGameName;
    std::string caption;
    bool busy = false;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Implementação**

```cpp
#include "app/composer_activity.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

ComposerActivity::ComposerActivity(IFeedClient* client, IAlbumSource* album,
                                   ITitleService* titles, std::function<void()> onPosted)
    : client(client), album(album), titles(titles), onPosted(std::move(onPosted)) {}

ComposerActivity::~ComposerActivity() { *this->alive = false; }

void ComposerActivity::onContentAvailable()
{
    auto* captionCell = (brls::InputCell*)this->getView("captionCell");
    captionCell->init("thomaz/composer/caption"_i18n, "",
                      [this](std::string v){ this->caption = v; },
                      "thomaz/composer/caption"_i18n, "", 280);

    auto* postBtn = this->getView("postBtn");
    postBtn->registerClickAction([this](brls::View*) { this->doPost(); return true; });
    postBtn->addGestureRecognizer(new brls::TapGestureRecognizer(postBtn));

    this->loadAlbum();
}

std::string ComposerActivity::resolveGameName(std::uint64_t titleId)
{
    if (titleId == 0) return "";
    for (const auto& t : this->titles->listInstalled())
        if (t.title_id == titleId)
            return t.name;
    return "";
}

void ComposerActivity::loadAlbum()
{
    this->getView("albumSpinner")->setVisibility(brls::Visibility::VISIBLE);

    IAlbumSource* a = this->album;
    auto alive      = this->alive;

    brls::async([this, a, alive]() {
        auto entries = a->list();
        brls::sync([this, alive, entries]() {
            if (!alive->load()) return;
            this->getView("albumSpinner")->setVisibility(brls::Visibility::GONE);
            auto* grid = (brls::Box*)this->getView("albumGrid");
            if (entries.empty()) {
                this->getView("albumEmpty")->setVisibility(brls::Visibility::VISIBLE);
                return;
            }
            // 2 thumbnails por linha (Box não tem flex-wrap).
            brls::Box* currentRow = nullptr;
            int col = 0;
            for (const auto& e : entries) {
                if (col % 2 == 0) {
                    currentRow = new brls::Box(brls::Axis::ROW);
                    currentRow->setWidth(brls::View::AUTO);
                    grid->addView(currentRow);
                }
                auto* cell = new brls::Image();
                cell->setWidth(120.0f);
                cell->setHeight(68.0f);
                cell->setCornerRadius(6.0f);
                cell->setMargins(6.0f, 6.0f, 6.0f, 6.0f);
                cell->setScalingType(brls::ImageScalingType::FILL);
                cell->setFocusable(true);
                if (!e.thumbnail.empty())
                    cell->setImageFromMem((unsigned char*)e.thumbnail.data(),
                                          (int)e.thumbnail.size());
                AlbumEntry entry = e;
                cell->registerClickAction([this, entry](brls::View*) {
                    this->selectEntry(entry); return true; });
                cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
                currentRow->addView(cell);
                col++;
            }
        });
    });
}

void ComposerActivity::selectEntry(const AlbumEntry& entry)
{
    this->selectedEntryId = entry.id;
    this->selectedTitleId = entry.titleId;

    // Mostra a imagem cheia no preview (carrega async) e resolve o jogo.
    IAlbumSource* a = this->album;
    auto alive      = this->alive;
    std::string id  = entry.id;
    std::uint64_t tid = entry.titleId;

    this->selectedGameName = this->resolveGameName(tid);
    if (auto* gameLbl = (brls::Label*)this->getView("previewGame")) {
        gameLbl->setText(this->selectedGameName.empty()
            ? "" : ("thomaz/feed/game_tag"_i18n + this->selectedGameName));
    }

    brls::async([this, a, alive, id]() {
        auto bytes = a->loadFull(id);
        brls::sync([this, alive, id, bytes]() {
            if (!alive->load()) return;
            if (this->selectedEntryId != id) return;
            if (auto* img = (brls::Image*)this->getView("previewImage"))
                if (!bytes.empty())
                    img->setImageFromMem((unsigned char*)bytes.data(), (int)bytes.size());
        });
    });
}

void ComposerActivity::doPost()
{
    if (this->busy) return;
    auto* status = (brls::Label*)this->getView("composerStatus");

    if (this->selectedEntryId.empty()) {
        status->setText("thomaz/composer/pick"_i18n);
        return;
    }

    this->busy = true;
    status->setText("thomaz/composer/posting"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";

    IFeedClient* c  = this->client;
    IAlbumSource* a = this->album;
    auto alive      = this->alive;
    std::string id  = this->selectedEntryId;
    std::string cap = this->caption;
    std::uint64_t tid = this->selectedTitleId;
    std::string game  = this->selectedGameName;

    brls::async([this, c, a, alive, token, id, cap, tid, game, status]() {
        std::vector<std::uint8_t> jpeg = a->loadFull(id);
        ActionResult r = c->createPost(token, jpeg, cap, tid, game);
        brls::sync([this, alive, r, status]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                status->setText("thomaz/composer/post_failed"_i18n);
                return;
            }
            auto cb = this->onPosted;
            brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                           [cb]() { if (cb) cb(); });
        });
    });
}

} // namespace thomaz
```

- [ ] **Step 4: Build desktop e validar o fluxo completo**

Run: `./scripts/build-desktop.sh && ./build_desktop/thomaz`
Expected:
- Home → "Comunidade" → feed carrega.
- "+ Postar" sem sessão → abre Auth → criar conta → volta.
- "+ Postar" logado → grid do álbum (6 thumbs) → selecionar → preview + "De: <jogo>" → digitar legenda → "Postar" → volta ao feed com o post novo no topo.
- Curtir e comentar atualizam contadores.

- [ ] **Step 5: Commit**

```bash
git add resources/xml/activity/composer.xml source/app/composer_activity.hpp source/app/composer_activity.cpp
git commit -m "feat(feed): ComposerActivity (album grid + caption, game auto-tag)"
```

---

## FASE 7 — Logout + verificação final

### Task 16: Logout em Configurações

**Files:**
- Modify: `source/app/settings_activity.cpp`

- [ ] **Step 1: Adicionar uma linha "Sair" que limpa a sessão**

Em `onContentAvailable()`, depois das outras rows, adicionar (só faz sentido quando há sessão, mas mostramos sempre — limpar sem sessão é no-op):

```cpp
    // --- Logout do feed ------------------------------------------------------
    auto* logoutRow = makeActionRow("thomaz/auth/logout"_i18n);
    logoutRow->registerClickAction([this, status](brls::View*) {
        clear_session();
        brls::Application::notify("thomaz/auth/logout"_i18n);
        return true;
    });
    logoutRow->addGestureRecognizer(new brls::TapGestureRecognizer(logoutRow));
    listBox->addView(logoutRow);
```

E adicionar o include no topo do arquivo:

```cpp
#include "platform/feed/auth_store.hpp"
```

- [ ] **Step 2: Build desktop**

Run: `./scripts/build-desktop.sh`
Expected: build OK; Configurações mostra "Sair".

- [ ] **Step 3: Commit**

```bash
git add source/app/settings_activity.cpp
git commit -m "feat(feed): log out clears the session in Settings"
```

---

### Task 17: Verificação final (todos os testes + build limpo)

- [ ] **Step 1: Rodar a suíte de testes**

Run: `cd tests && make clean && make test`
Expected: PASS — incluindo `test_feed_pagination.cpp` e `test_session_codec.cpp`.

- [ ] **Step 2: Build desktop limpo**

Run: `rm -rf build_desktop && ./scripts/build-desktop.sh`
Expected: build OK do zero.

- [ ] **Step 3: Smoke manual do fluxo completo**

Run: `./build_desktop/thomaz`
Checklist:
- Home: hero "Comunidade", card "Trapaças" à direita, "Configurações", "Mods (em breve)".
- "Trapaças" ainda abre a lista de jogos (regressão).
- Feed: scroll infinito (25 posts, 8 por página), painel de detalhe atualiza ao focar.
- Postar: Auth gate → álbum → preview com jogo → legenda → post no topo.
- Curtir/comentar: contadores sobem; curtida persiste ao rolar.
- Configurações → "Sair": limpa sessão (próxima ação de postar volta a pedir login).

- [ ] **Step 4: Commit final (se houver ajustes)**

```bash
git add -A
git commit -m "chore(feed): final verification pass"
```

---

## Notas para o Switch (não validável no desktop)

- `SwitchAlbumSource` usa `caps:a`. Confirmar os nomes/assinaturas em `$DEVKITPRO/portlibs/switch/include/switch/services/capsa.h` da versão do libnx instalada; ajustar se a build do Switch reclamar (campos `application_id`/`content`/`datetime` são estáveis).
- O thumbnail no grid hoje usa a imagem cheia (`loadFull`); se ficar pesado no console, trocar por `capsaLoadAlbumScreenShotThumbnailImage` (decodifica reduzido) numa task futura.
- O upload real (`createPost` com os bytes JPEG) só acontece quando existir o `HttpFeedClient` — fora do escopo desta entrega.
