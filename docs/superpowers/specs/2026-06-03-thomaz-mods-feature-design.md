# thomaz — Feature de Mods (GameBanana + LayeredFS)

**Data:** 2026-06-03
**Status:** Design aprovado — pronto para planejamento
**Autor:** brainstorming session

## Resumo

Adicionar ao thomaz um **hub completo de mods** para jogos de Switch: descobrir e
baixar mods do GameBanana dentro do app, e gerenciar (ativar/desativar/desinstalar)
os mods instalados via LayeredFS do Atmosphère. Espelha o modelo **file-based** já
usado pelos cheats (o thomaz grava arquivos na SD; o Atmosphère aplica no boot do
jogo), reaproveitando o split `core`/`platform`/`app` e os padrões existentes.

## Decisões (travadas no brainstorming)

| Decisão | Escolha | Motivo |
|---------|---------|--------|
| Escopo | **Hub completo** (download + gerenciamento) | Objetivo do produto |
| Armazenamento / toggle | **Staging + cópia** | Fonte da verdade clara; vários mods baixados; uninstall = apagar staging |
| Descoberta | **Híbrida**: mapeamento curado + fallback de busca livre | "Seus jogos → mods" quando mapeado; busca quando não |
| Tipos de mod | **Só romfs (assets)** | Maioria dos mods; independe de build_id; não pisa nos cheats |
| Mods ativos por jogo | **Um por jogo** | Elimina manifesto por-arquivo e resolução de conflito |
| Casa do mapeamento | **Backend `api/`** (Fastify + Postgres) | Curável via admin; auto-deploy já existente |

### Não-objetivos (YAGNI)
- Code patches exefs/IPS (60FPS, resolução) — território dos cheats; extensão futura.
- Múltiplos mods empilhados por jogo + resolução de conflito por-arquivo — futuro.
- Mapeamento curado no MVP de rede (entra na fase M3).
- RAR como formato de primeira classe (best-effort; foco em zip/7z).

## Arquitetura

Segue o split existente: `core/` (lógica pura, host-testável, fetcher injetado),
`platform/` (Switch vs fake/desktop), `app/` (activities Borealis).

### Novos módulos `core/mods/` (puro, testável)
Espelham `cheat_repository` / `fetch_cheat_set` (orquestração com `UrlFetcher` injetado).

- **`mod_types.hpp`** — structs: `Mod` (id, nome, autor, thumb, contagem de downloads,
  `std::vector<ModFile>`), `ModFile` (filename, size, download_url, md5, formato),
  `StagedMod` (title_id, nome, caminho de staging, ativo?), `ModSource` (mapeado | busca).
- **`gamebanana_json.{hpp,cpp}`** — parse das respostas da Core API
  (`Core/Item/Data`): registros de mod, array `_aFiles`, `Url().sDownloadUrl()`.
  Lida com o formato de **array plano ordenado por fields**. Testado com fixtures.
- **`mod_index.{hpp,cpp}`** — parse do mapeamento `title_id → gamebanana_game_id`
  servido pelo backend (espelha `cheat_db`).
- **`mod_repository.{hpp,cpp}`** — orquestra: dado `title_id` → resolve `game_id`
  (mapeamento; senão busca por nome) → lista mods → resolve URL de download.
  `UrlFetcher` injetado; sem libcurl; host-testable. Status análogo a `FetchStatus`
  (Ok / NotMapped / NetworkError).
- **`mod_install.{hpp,cpp}`** — lógica pura: dada a **lista de entradas** de um
  arquivo, valida que é um mod romfs e computa o layout de staging
  (normaliza um `romfs/` ou `contents/<program_id>/romfs/` solto). Testável sem disco.

### Novos módulos `platform/mods/` (Switch + fake)
- **`mod_store.{hpp,cpp}`** — staging em `sd:/thomaz/mods/<title_id>/<nome_do_mod>/`.
  Operações: listar staged; ler/escrever o marker de **mod ativo** (1 por jogo);
  **enable** (copia a árvore romfs → `/atmosphere/contents/<program_id>/romfs/`,
  desativando o anterior antes); **disable** (apaga o romfs daquele jogo de
  `contents/`); **uninstall** (apaga do staging). Reusa `read_text_file`/
  `write_text_file` quando aplicável.
- **`archive_extractor.{hpp,cpp}`** — wrapper libarchive com **streaming pra SD**
  (não carrega o arquivo inteiro em RAM). Switch: `switch-libarchive`; desktop:
  libarchive do host. API: extrair `arquivo → dir de staging`, com callback de
  progresso. Registra formatos via `archive_read_support_format_all` /
  `..._filter_all`. zip/7z first-class; rar best-effort.

### Reuso de plataforma existente
- `http_client_curl` para downloads + **nova variante download-pra-arquivo com
  callback de progresso** (mods são grandes; não bufferizar em memória).
- `ITitleService` para a lista de jogos instalados (entrada da descoberta).

### Novas activities `app/` (Borealis)
- **`mod_browser_activity`** — entra pela lista de jogos instalados (reuso) +
  caixa de **busca livre**. Lista mods do jogo selecionado.
- **`mod_detail_activity`** — arquivos do mod, botão baixar, barra de progresso.
- **`mod_manager_activity`** — mods staged por jogo; toggle de ativo (1/jogo);
  desinstalar. Espelha `game_list_activity` / `clear_cheats_activity`.

i18n (pt-BR / en-US) e XML das telas seguem o padrão atual em `resources/`.

### Backend (`api/`)
- Novo endpoint **`GET /mods/index`** servindo o JSON de mapeamento
  `title_id → gamebanana_game_id`, curável via admin. Tabela + seed Prisma,
  migração nova. Segue os padrões do backend; auto-deploy (push em `api/**`) cobre.

## Fluxo de dados

1. **Descoberta** — usuário abre Mods → lista de jogos instalados (`ITitleService`).
   Por jogo, resolve `gamebanana_game_id` via `/mods/index`. Mapeado → "mods deste
   jogo". Não mapeado → fallback de **busca livre** por nome.
2. **Browse** — `mod_repository` chama a Core API do GameBanana → lista mods
   (parse em `gamebanana_json`).
3. **Download** — usuário escolhe mod/arquivo → download **streaming** pra
   `sd:/thomaz/mods/<title_id>/<mod>/<arquivo>` com progresso. Ideal: app em
   full application mode (heap completo).
4. **Extração** — `archive_extractor` extrai o arquivo pro staging
   `sd:/thomaz/mods/<title_id>/<nome_do_mod>/`, normalizando pra árvore romfs.
   `mod_install` valida que é um mod romfs.
5. **Enable** — copia a árvore romfs → `/atmosphere/contents/<program_id>/romfs/`;
   desativa o mod anterior daquele jogo antes. Grava o marker de ativo.
6. **Disable / Uninstall** — disable apaga o romfs daquele jogo de `contents/`;
   uninstall apaga do staging.

## Dependências novas & riscos

- 🔴 **`switch-libarchive`** — dependência de build nova. Ajustar `CMakeLists.txt`
  e a imagem/steps de CI. **Confirmar disponibilidade na `devkitpro/devkita64`**
  (via `dkp-pacman` se necessário). Maior item técnico novo.
- 🟠 **TLS** — GameBanana é HTTPS-only. O roadmap já lista "verificação TLS com
  cacert.pem embarcado" como pendente → vira **requisito** desta feature.
  Confirmar que o `switch-curl` linka um backend TLS.
- 🟠 **Memória / modo applet** — arquivos grandes exigem streaming (download e
  extração) e idealmente **full application mode** (title-takeover) pro heap
  completo. MVP: stream + cap de tamanho com aviso ao usuário.
- 🟡 **GameBanana API** — rate limits/auth não verificados; considerar a apiv11
  além da Core API legada. **RAR instável no libarchive** → zip/7z first-class,
  rar best-effort.

## Faseamento (M1 → M3)

- **M1 — Pipeline de install local.** `core/mods/mod_types`, `mod_install`;
  `platform/mods/archive_extractor` (libarchive) + `mod_store` (staging,
  enable/disable 1-por-jogo, uninstall); `mod_manager_activity`. Testa com um
  `.zip` colocado à mão. **Entrega gerenciamento de mods funcionando, sem rede.**
  Front-load do maior risco técnico (libarchive + CI).
- **M2 — Download GameBanana.** `gamebanana_json`, `mod_repository` (busca livre);
  download streaming com progresso; `mod_browser_activity` + `mod_detail_activity`.
  **Entrega baixar-e-instalar do app.**
- **M3 — Mapeamento curado.** Endpoint backend `/mods/index` + seed; `mod_index`
  + cache; experiência "seus jogos → mods" com fallback de busca. **Entrega a
  descoberta híbrida completa.**

## Estratégia de testes

- `core/mods/*` em doctest (host), espelhando `tests/` atual:
  - `gamebanana_json` — parsing com respostas reais gravadas (fixtures).
  - `mod_index` — parsing do mapeamento.
  - `mod_repository` — orquestração com `UrlFetcher` fake (Ok / NotMapped /
    NetworkError).
  - `mod_install` — listas de entradas de arquivo → layout de staging + validação
    romfs (incl. casos de árvore solta vs `contents/<program_id>/`).
- `platform/mods/*` com implementações fake no desktop (extração e staging contra
  o filesystem do host).

## Critérios de sucesso

- Um `.zip`/`.7z` de mod romfs colocado/baixado é extraído, fica staged, e ao ser
  ativado aparece em `/atmosphere/contents/<program_id>/romfs/` com a árvore correta.
- Ativar um segundo mod do mesmo jogo desativa o primeiro (1-por-jogo).
- Desativar remove o romfs daquele jogo de `contents/`; desinstalar remove do staging.
- Busca livre no GameBanana lista mods e baixa via streaming com progresso.
- Jogos mapeados mostram "seus mods" direto; não mapeados caem na busca.
- Lógica `core/mods/*` coberta por testes host verdes; build .nro verde com a nova
  dependência libarchive.
