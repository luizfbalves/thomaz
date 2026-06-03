# Design — Feed da Comunidade (thomaz)

**Data:** 2026-06-03
**Status:** aprovado para planejamento
**Escopo:** parte do app (UI + camada de rede stubada). A API real é futura.

---

## 1. Resumo

Um feed social estilo Twitter dentro do thomaz, onde usuários postam **screenshots tiradas pelo Switch** com uma **legenda**, curtem e comentam. Usuários criam **username + senha**.

A API ainda não existe. Esta entrega cobre o **lado do app**: telas reais + uma camada de rede atrás de uma interface (`IFeedClient`), com uma **implementação fake** (dados sintéticos em memória), espelhando o padrão `title_service_fake` já usado no projeto. O app fica **rodável de ponta a ponta no desktop hoje**; quando a API existir, basta escrever `HttpFeedClient` e trocar a injeção no `main.cpp`.

### Decisões-chave

- **Entrega:** UI + camada de rede stubada (rodável), não só mockups.
- **Imagem do post:** galeria do **Álbum do Switch** (`caps:a` do libnx).
- **Acesso:** feed **livre para ver**; login só para **postar/interagir**.
- **Interações:** **curtir + comentar** (sem tela de perfil nesta versão).
- **Layout do feed:** **master-detail** — lista à esquerda, detalhe + comentários num painel à direita (sem trocar de tela).
- **Compositor:** **split numa tela só** — grid do Álbum à esquerda, preview + legenda + Postar à direita.
- **Auto-marcação do jogo:** cada captura carrega o **title ID real** (via `caps:a`); resolvido para **nome + ícone** pelo `ITitleService` já existente e anexado ao post.

---

## 2. Entrada no app (Home)

Reorganização da Home (`home.xml` + `home_activity.cpp`):

- **Hero (grande, à esquerda):** **Comunidade / Feed** — abre `FeedActivity`.
- **Rail (à direita):**
  - **Cheats** — assume o slot que era do "Save Manager" (card menor).
  - **Configurações** — inalterado.
  - **Mods** — continua "em breve" (bloqueado).

O wiring de Cheats (`GameListActivity`) é preservado; muda só o card que dispara.

---

## 3. Arquitetura em camadas

Segue o padrão do thomaz: `core` puro → `platform` com interface + impl por plataforma → `app` com Activity + XML.

### `core/feed/` — tipos e lógica puros (sem Borealis/curl, testáveis)

```cpp
struct User    { std::string id; std::string username; };
struct Post {
    std::string  id;
    User         author;
    std::string  imageUrl;        // ou ref para os bytes no fake
    std::string  caption;
    std::uint64_t gameTitleId;    // jogo de origem da captura (0 = desconhecido)
    std::string  gameName;        // resolvido via ITitleService
    int          likeCount;
    bool         likedByMe;
    int          commentCount;
    std::int64_t createdAt;       // epoch
};
struct Comment { std::string id; User author; std::string text; std::int64_t createdAt; };
struct FeedPage { std::vector<Post> posts; std::string nextCursor; bool hasMore; };
```

Lógica testável: paginação por cursor (append de páginas, dedup por `Post.id`, cálculo de `hasMore`).

### `platform/feed/` — contratos + implementações

**`IFeedClient`** (auth + feed juntos; cada chamada retorna status/erro no espírito de `HttpResponse::ok()`):

```cpp
struct AuthResult   { bool ok; std::string token; std::string error; };
struct ActionResult { bool ok; std::string error; };

class IFeedClient {
  public:
    virtual ~IFeedClient() = default;
    // Conta
    virtual AuthResult registerUser(const std::string& user, const std::string& pass) = 0;
    virtual AuthResult login(const std::string& user, const std::string& pass) = 0;
    // Feed (scroll infinito por cursor; cursor vazio = primeira página)
    virtual FeedPage   fetchFeed(const std::string& cursor) = 0;
    // Postar (bytes JPEG vindos do IAlbumSource + jogo resolvido)
    virtual ActionResult createPost(const std::string& token,
                                    const std::vector<std::uint8_t>& jpeg,
                                    const std::string& caption,
                                    std::uint64_t gameTitleId,
                                    const std::string& gameName) = 0;
    // Curtir / descurtir
    virtual ActionResult setLike(const std::string& token,
                                 const std::string& postId, bool liked) = 0;
    // Comentários
    virtual std::vector<Comment> fetchComments(const std::string& postId) = 0;
    virtual ActionResult addComment(const std::string& token,
                                    const std::string& postId, const std::string& text) = 0;
};
```

- **`FakeFeedClient`** — dados sintéticos em memória: gera páginas de posts (com title IDs falsos → nomes de mentira), aceita qualquer login, guarda posts/curtidas/comentários em memória durante a sessão. Roda no desktop hoje.
- **`HttpFeedClient`** — **futuro** (quando a API existir). Não implementado nesta entrega; o contrato fica pronto.

**`IAlbumSource`** (abstrai o Álbum do Switch):

```cpp
struct CaptureDate { int year, month, day, hour, minute, second; };
struct AlbumEntry {
    std::string  id;
    std::uint64_t titleId;            // application_id real (caps:a)
    CaptureDate  captured;
    std::vector<std::uint8_t> thumbnail; // JPEG para o grid
};

class IAlbumSource {
  public:
    virtual ~IAlbumSource() = default;
    virtual std::vector<AlbumEntry> list() = 0;                       // grid de capturas
    virtual std::vector<std::uint8_t> loadFull(const std::string& id) = 0; // bytes para upload
};
```

- **`SwitchAlbumSource`** — `caps:a` do libnx: lista entradas (com `application_id` + datetime) e carrega a imagem cheia. Só screenshots (ignora vídeos).
- **`FakeAlbumSource`** — imagens de exemplo no desktop, com title IDs falsos que resolvem para nomes de mentira via `FakeTitleService`.

**`auth_store`** (igual ao `app_settings`, arquivo na SD/working dir):

```cpp
struct Session { std::string token; std::string username; };
std::optional<Session> load_session();   // mantém login entre sessões
void save_session(const Session&);
void clear_session();                     // logout (em Configurações)
```

### `app/` — activities + XML

- **`FeedActivity`** — master-detail (lista + painel de detalhe/comentários).
- **`ComposerActivity`** — split (grid do Álbum + preview/legenda).
- **`AuthActivity`** — login/cadastro.

### Injeção (`main.cpp`)

Mesmo esquema do `titleService`/`httpClient` atuais:

- **Desktop:** `FakeFeedClient` + `FakeAlbumSource`.
- **Switch:** `SwitchAlbumSource` real + `FakeFeedClient` **por enquanto** (até a API existir).

`FeedActivity` recebe `IFeedClient*`, `IAlbumSource*` e o `ITitleService*` já existente (para resolver title ID → nome/ícone).

Toda chamada de rede/IO roda em `brls::async` e volta para a UI com `brls::sync`, com o guard `alive` (padrão do `GameListActivity`).

---

## 4. Fluxos

### Abrir o feed (`FeedActivity`, master-detail)

1. `onContentAvailable` → spinner → `async fetchFeed("")` → renderiza os cards na coluna esquerda; foca/seleciona o primeiro → painel direito mostra imagem grande + legenda + jogo + curtir + `async fetchComments`.
2. Rolar perto do fim → `fetchFeed(nextCursor)` → anexa próxima página (scroll infinito); spinner no rodapé da lista enquanto carrega.
3. Mudar o foco de card → painel direito atualiza.

### Postar (`ComposerActivity`)

1. Tocar **"+ Postar"** → sem sessão, abre `AuthActivity` primeiro; com sessão, abre o compositor.
2. `async album->list()` → grid de thumbnails (cada uma já traz `titleId`). Selecionar → `async loadFull(id)` → preview à direita; resolve `titleId → nome/ícone` via `ITitleService` e mostra o jogo marcado.
3. Escrever legenda → **Postar** → `async createPost(...)` (com `gameTitleId` + `gameName`) → sucesso: fecha o compositor e dá refresh no feed (volta ao topo).

### Curtir / comentar

Exigem sessão. Sem sessão → abre `AuthActivity`; ao voltar logado, refaz a ação. **Curtida é otimista** (atualiza na hora, reverte se a chamada falhar).

### Login / cadastro (`AuthActivity`)

Alternância **Entrar | Criar conta**, campos username + senha, submit. Sucesso → `save_session` (fica logado entre sessões) → fecha e continua a ação pendente. **Logout** vai em Configurações.

---

## 5. Estados (reaproveitando o padrão do `game_list`)

- **Carregando:** `ProgressSpinner`.
- **Vazio:** label ("ainda não há posts" / "seu Álbum está vazio").
- **Erro de rede:** label + botão **Tentar de novo** (sem cache offline nesta versão).
- **Erros de auth/post:** mensagem inline ("username já existe", "senha incorreta", "falha ao postar — a legenda é mantida para tentar de novo").

---

## 6. i18n

Novas chaves em `resources/i18n/pt-BR/thomaz.json` e `en-US/thomaz.json` sob:

- `thomaz/feed/*` — título, vazio, erro, ação postar, contadores.
- `thomaz/auth/*` — entrar/criar conta, labels e erros.
- `thomaz/composer/*` — título, "escolha a captura", placeholder de legenda, postar.

E o tile da Home (`thomaz/module/feed/*` ou equivalente) para o novo hero "Comunidade".

---

## 7. Testes

- **Unitários** (`tests/`): `core/feed/` — paginação por cursor (append, dedup por `id`, `hasMore`), construção de `Post`/`FeedPage`.
- **Manual no desktop:** `FakeFeedClient` + `FakeAlbumSource` dirigem o fluxo completo (login → escolher captura → postar com jogo auto-marcado → curtir → comentar → scroll infinito).

---

## 8. Fora de escopo (YAGNI)

- Tela de perfil, seguir usuários, notificações.
- Edição/filtros/recorte de imagem.
- Deletar/editar post, denúncia/moderação.
- `HttpFeedClient` real (API futura) — só o contrato fica pronto.
- Cache offline do feed.

---

## 9. Riscos / pontos de atenção

- **`caps:a` em hardware real:** acesso ao Álbum e carregamento de imagem cheia ainda não validado no console (o projeto inteiro ainda não foi validado em hardware). Risco de permissões/inicialização do serviço.
- **Tamanho do upload:** screenshots cheias são JPEGs grandes; o contrato `createPost` recebe os bytes — o `HttpFeedClient` futuro precisará lidar com upload (a API ainda não existe; só registrado aqui).
- **Foco/navegação no master-detail:** dois painéis com navegação por controle (esquerda/direita entre painéis, cima/baixo na lista) exige atenção no modelo de foco do Borealis.
- **Resolução title ID → nome:** capturas de jogos não instalados não resolvem nome (cai em `gameTitleId` sem `gameName`); tratar como "jogo desconhecido".
