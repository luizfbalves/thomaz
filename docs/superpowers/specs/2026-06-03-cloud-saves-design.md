# Cloud Saves — Consumo real da API no gerenciamento de saves

**Data:** 2026-06-03
**Status:** Aprovado (design)

## Objetivo

Dar à tela de detalhe de save dois botões — **Enviar pra nuvem** e **Baixar da
nuvem** — falando com a API real (`api/`, rotas `GET/PUT /saves/:titleId`), com
status da nuvem mostrado ao abrir o detalhe e resolução de conflito por
`revision`. O sync é **sempre manual** (o usuário clica); não há reconciliação
automática em background.

## Contexto

Hoje o gerenciamento de saves do app é **100% local**:

- `ISaveService` (libnx no Switch, fake no desktop) lê/escreve o save real do
  jogo e faz backup pra pastas no SD (`/switch/thomaz/saves/<titleId>/<timestamp>/`
  com dados por profile + `manifest.json`).
- `SaveManagerActivity` lista jogos + data do último backup; `SaveDetailActivity`
  tem "back up now" e restaura de backups locais.
- **Não existe nenhum cliente de rede para saves.**

A API já expõe saves na nuvem (requer JWT):

| Rota | Uso |
|------|-----|
| `GET /saves` | lista slots (metadados) do usuário |
| `GET /saves/:titleId` | metadados de um slot; `?includeData=1` inclui o blob em base64 |
| `PUT /saves/:titleId` | multipart (`data` = arquivo, `label`, `revision`); concorrência otimista por `revision` (409 em conflito; 413 se > 16 MB) |
| `DELETE /saves/:titleId` | remove o slot |

Cada slot é **um blob opaco por (user, titleId)** com `{label, revision,
updatedAt}` — **sem histórico no servidor**, uma única revisão corrente.

## Decisões (do brainstorming)

1. **Sync manual**, não automático — usuário clica em enviar/baixar.
2. **Status ao abrir o detalhe:** 1 `GET /saves/:titleId` (sem blob) ao entrar
   na tela, pra mostrar o estado da nuvem. Nenhum upload/download sem clique.
3. **Upload lê o save ativo na hora** (via `ISaveService`), não um backup local.
4. **Download grava como backup local e pergunta:** baixa o blob → vira um novo
   backup local → se mais novo que o save ativo, diálogo pergunta se restaura
   agora. Gravar no save ativo nunca é automático.
5. **Conflito → pergunta ao usuário.** Conflito = nuvem mudou em outro lugar
   (`revision` da nuvem > revisão sincronizada localmente) **e** o usuário pediu
   upload. Diálogo: *Manter da nuvem* (baixa o blob como backup local) ou
   *Enviar o meu* (sobe o save ativo, sobrescrevendo a nuvem). Nada é perdido.
6. **Auth:** saves na nuvem exigem login, reusando a sessão/`auth_store` da
   feature de feed. Sem login → aviso + botão que abre `AuthActivity`. Em `401`
   → pedir login de novo (sem auto-refresh de token nesta versão).
7. **`DELETE` da nuvem está fora do escopo** desta versão (YAGNI).

## Modelo de sync

Cada título guarda localmente a **última `revision` sincronizada**. A situação
é uma função pura de `(nuvem existe?, revisão da nuvem, revisão sincronizada)`:

| Situação | Condição |
|----------|----------|
| `NoCloud`    | nuvem não tem slot pra esse título |
| `InSync`     | `revisão nuvem == revisão sincronizada` |
| `CloudAhead` | `revisão nuvem > revisão sincronizada` (alguém enviou de outro lugar) |

**Enviar (push):**
- Lê o save ativo → empacota em blob.
- Se `CloudAhead` ⇒ diálogo de conflito (*Manter da nuvem* / *Enviar o meu*).
  - *Enviar o meu* ⇒ `PUT` com `revision = revisão da nuvem` (vence).
  - *Manter da nuvem* ⇒ executa o fluxo de baixar.
- Se `NoCloud` ⇒ `PUT` com `revision = 0`. Se `InSync` ⇒ `PUT` com a revisão
  corrente.
- Sucesso ⇒ atualiza a revisão sincronizada com a `revision` retornada.

**Baixar (pull):**
- `NoCloud` ⇒ nada a baixar (mensagem).
- Senão `GET ...?includeData=1` → grava o blob como **novo backup local** →
  atualiza a revisão sincronizada → se o backup é mais novo que o save ativo,
  diálogo *"restaurar agora?"* (reusa o restore local existente).

## Componentes

### Core (puro, testável com doctest)

- **`core/save_package.{hpp,cpp}`** — `pack`/`unpack` do blob. Serializa as
  pastas de profile + arquivos do save num único blob opaco e de volta.
  Formato binário simples com entradas length-prefixed (`uid_hex`, caminho
  relativo, bytes). Testes de round-trip (vazio, 1 profile, múltiplos profiles,
  blob corrompido → `nullopt`).
- **`core/save_sync_state.{hpp,cpp}`** — serialize/parse do mapa
  `titleId → revisão sincronizada` (texto, uma linha `titleIdHex revision` por
  título). Lido/gravado em `config/save_sync.txt`. Helpers get/set/remove.
- **`core/save_sync.{hpp,cpp}`** — função pura
  `classify(cloudExists, cloudRevision, syncedRevision) → SyncSituation` e a
  decisão de push (`PushPlan { revision, isConflict }`). Sem rede, sem I/O.

### Platform

- **`platform/saves/cloud_save_client.hpp`** — interface `ICloudSaveClient`:
  - `CloudStatus getStatus(token, titleId)` → `{ok, exists, revision, label, updatedAt, error}` (GET sem blob).
  - `CloudPull pull(token, titleId)` → `{ok, revision, label, updatedAt, blob, error}` (GET `includeData=1`).
  - `CloudPush push(token, titleId, blob, label, revision)` → `{ok, conflict, newRevision, error}` (PUT multipart; `conflict=true` em 409).
  - Todos chamados de worker thread (`brls::async`); não tocam a UI.
- **`platform/saves/http_cloud_save_client.{hpp,cpp}`** — impl real sobre o
  `IHttpClient` (curl) já existente. Monta multipart no `PUT`, parseia JSON
  (nlohmann), mapeia `401`/`409`/`413`/rede. Funciona no desktop (contra
  `localhost:3000`) e no Switch (API real).
- **`platform/saves/fake_cloud_save_client.{hpp,cpp}`** — fake em memória, pros
  testes e uso offline no desktop.
- **Base URL** vira config em `app_settings` (default `http://localhost:3000`).

### ISaveService (2 métodos novos)

- `std::vector<std::uint8_t> packageActiveSave(title_id, std::string* err)` —
  lê o save ativo (todos os profiles) e devolve o blob (usa `save_package`).
  Vazio + `*err` em falha.
- `bool importPackageAsBackup(title_id, const std::vector<std::uint8_t>& blob,
  std::string* err)` — desempacota o blob e grava um backup local
  timestampado; o `restore(BackupEntry, ...)` existente cuida de gravar no jogo.

A impl fake (desktop) implementa os dois sobre o sistema de arquivos local
usado pelos backups fake atuais, pra o fluxo rodar end-to-end sem console.

### UI — `SaveDetailActivity`

- Nova seção **"Nuvem"**.
- Ao abrir, se logado: 1 `GET` de status → texto:
  - *"Nuvem: rev N · enviado há X"* (InSync)
  - *"Nada na nuvem"* (NoCloud)
  - *"Nuvem mais nova que o local"* (CloudAhead)
- Botões **Enviar pra nuvem** / **Baixar da nuvem** com os fluxos da seção
  "Modelo de sync".
- Diálogos de **conflito** e de **restaurar agora?** via `brls::Dialog`.
- Erros (offline / `401` / `413` / `500`) viram mensagem na seção.
- Sem login → aviso *"Faça login pra usar saves na nuvem"* + botão que abre
  `AuthActivity` (mesmo padrão `requireSession` do feed).

### Wiring

- `main.cpp` constrói `HttpCloudSaveClient` (base URL do config) e o injeta via
  `HomeActivity` → `SaveManagerActivity` → `SaveDetailActivity`. O token vem do
  `auth_store` (sessão do feed). No desktop, dá pra trocar pelo
  `FakeCloudSaveClient` em build de teste.

## Tratamento de erros

| Cenário | Comportamento |
|---------|---------------|
| Sem rede / timeout | mensagem *"Sem conexão com a nuvem"*; nenhum estado local muda |
| `401` | mensagem *"Sessão expirada"* + botão de login |
| `409` no push | trata como conflito → diálogo (não é erro fatal) |
| `413` (save > 16 MB) | mensagem *"Save grande demais pra nuvem"* |
| `5xx` / JSON inválido | mensagem genérica *"Falha na nuvem, tente de novo"* |
| Blob corrompido no pull | `unpack` retorna `nullopt` → mensagem; nada é gravado |

## Testes

- **Core unit (doctest):**
  - `save_package`: round-trip vazio / 1 profile / N profiles; corrompido → `nullopt`.
  - `save_sync_state`: serialize/parse, get/set/remove, arquivo malformado.
  - `save_sync`: `classify` nos 3 estados; `PushPlan` (NoCloud→rev0, InSync→rev,
    CloudAhead→conflict).
- **Fake client:** push/pull/getStatus em memória; conflito (revisão stale → 409).
- **Build:** `tests/Makefile` inclui os novos `core/*` ; build desktop limpo
  (`./scripts/build-desktop.sh`) + smoke run (exit 124 = saudável).

## Limitação conhecida (v1)

O blob carrega os profiles pelo `uid_hex` da conta. Restaurar um save feito em
**outro console** (UIDs de conta diferentes) segue a mesma lógica do restore
local atual — o mapeamento de profile entre consoles **não** é resolvido nesta
versão. Anotado como limitação; revisitar se virar problema real.

## Fora de escopo

- `DELETE` de save na nuvem.
- Auto-refresh de token JWT (re-login manual no `401`).
- Sync automático / em background.
- Mapeamento de profile entre consoles distintos.
