# Quick Task 260605-rcu: Ocultar apps não-jogo da lista — Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Task Boundary

Ocultar da lista de jogos os apps que não são jogos (ex.: Sphaira e forwarders de
homebrew semelhantes). A lista vem de `ITitleService::listInstalled()` que, no
Switch, usa `nsListApplicationRecord` — retorna TODA application instalada,
incluindo forwarders. Não há filtro hoje.
</domain>

<decisions>
## Implementation Decisions (LOCKED — user chose "Híbrido")

### Abordagem: Híbrido (heurística automática + override manual)
- Heurística esconde forwarders por padrão; usuário pode forçar mostrar/ocultar caso a caso.

### Heurística (conservadora, para minimizar falso-positivo)
- Classifica como NÃO-JOGO somente quando `save_data_size == 0` **E** `startup_user_account == 0`.
  Esse é o perfil típico de forwarder/homebrew. Conservador de propósito: prefere
  NÃO esconder um jogo de verdade (que tenha save ou conta) e deixar o usuário
  ocultar manualmente os que escaparem.
- Requer adicionar `startup_user_account` (u8, do NACP) ao struct `InstalledTitle`
  e popular em `title_service_switch.cpp` (`control->nacp.startup_user_account`).

### Override manual (persistido)
- Dois conjuntos de title_id: `force_hidden` e `force_shown`.
- Visibilidade efetiva de um título:
  1. se id ∈ force_shown → visível
  2. senão se id ∈ force_hidden → oculto
  3. senão → oculto se a heurística disser NÃO-JOGO; caso contrário visível
- "Efetivamente oculto" (para badge/label da ação) segue a mesma regra,
  independente do toggle global.

### Toggle global "mostrar ocultos"
- Bool persistido `show_hidden`. Quando ON, a lista mostra TODOS os títulos
  (inclusive os ocultos, com um badge "Oculto"), para o usuário poder des-ocultar.
  Quando OFF, os efetivamente-ocultos são filtrados da lista.

### UI (dentro do GameListActivity — não mexer no settings_activity)
- Botão **X** (BUTTON_X) no frame `gameListFrame`: alterna `show_hidden` e
  reconstrói a lista. (`-`/BACK já é usado por install_help_action; X livre.)
- Botão **Y** (BUTTON_Y) por linha: alterna ocultar/mostrar aquele título
  (atualiza force_hidden/force_shown), persiste e reconstrói a lista.
- Reconstrução: guardar `std::vector<InstalledTitle>` em membro; um
  `rebuildList()` que faz `listBox->clearViews()` e re-adiciona as linhas filtradas.

### Lógica pura e testável
- Núcleo de classificação/visibilidade em arquivo próprio (ex.: `core/title_filter`),
  sem dependência de libnx, com teste de unidade no desktop (usa o FakeTitleService).

### Claude's Discretion
- Formato exato do arquivo de persistência (espelhar o padrão do app_settings/fs_util).
- Nomes exatos de arquivos/funções e chaves i18n.
- Cores/posição do badge "Oculto".
</decisions>

<specifics>
## Specific Ideas

- O FakeTitleService deve ganhar uma entrada estilo homebrew (ex.: "Sphaira",
  save 0, startup_user_account 0) e os jogos reais devem ter
  `startup_user_account = 1`, para o teste exercitar a heurística corretamente.
- Atenção: no fake atual, BotW está com saveSize 0 — bom caso de borda para o
  teste do override (jogo que a heurística erraria).
</specifics>

<canonical_refs>
## Canonical References

- `source/platform/title.hpp` — struct `InstalledTitle` (tem `save_data_size`; falta `startup_user_account`)
- `source/platform/title_service_switch.cpp` — popula NACP (`control->nacp.*`)
- `source/platform/title_service_fake.cpp` — dados de amostra desktop
- `source/app/game_list_activity.cpp` — `populate()` constrói as linhas; `gameListFrame` é o id do AppletFrame
- `source/app/cheat_detail_activity.cpp:131` — exemplo de `frame->registerAction(..., brls::BUTTON_X, ...)`
- `source/platform/app_settings.*` — padrão de persistência (fs_util) a espelhar
- `lib/borealis/.../box.hpp:105` — `Box::clearViews(bool free)` para reconstruir a lista
</canonical_refs>
