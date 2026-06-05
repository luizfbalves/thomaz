---
phase: quick-260605-rcu
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - source/platform/title.hpp
  - source/platform/title_service_switch.cpp
  - source/platform/title_service_fake.cpp
  - source/core/title_filter.hpp
  - source/core/title_filter.cpp
  - source/platform/title_visibility_store.hpp
  - source/platform/title_visibility_store.cpp
  - source/app/game_list_activity.hpp
  - source/app/game_list_activity.cpp
  - resources/i18n/pt-BR/thomaz.json
  - resources/i18n/en-US/thomaz.json
  - resources/i18n/fr/thomaz.json
  - resources/i18n/ru/thomaz.json
  - resources/i18n/zh-Hans/thomaz.json
  - tests/test_title_filter.cpp
  - tests/Makefile
autonomous: true
requirements: [260605-rcu]

must_haves:
  truths:
    - "Forwarders/homebrew (save_data_size==0 && startup_user_account==0) são ocultados por padrão da lista de jogos"
    - "Botão X no gameListFrame alterna show_hidden e reconstrói a lista"
    - "Botão Y por linha alterna ocultar/mostrar aquele título individualmente e persiste"
    - "Quando show_hidden=true, linhas efetivamente ocultas exibem badge 'Oculto'"
    - "Overrides force_shown e force_hidden são persistidos e sobrevivem a restart"
    - "O filtro funciona em modo Cheats e em modo Mods (mesmo code-path rebuildList)"
    - "Teste de unidade desktop cobre classify, effectively_hidden e os casos de borda"
  artifacts:
    - path: "source/core/title_filter.hpp"
      provides: "classify() e effectively_hidden() — lógica pura sem libnx"
    - path: "source/core/title_filter.cpp"
      provides: "implementações de classify e effectively_hidden"
    - path: "source/platform/title_visibility_store.hpp"
      provides: "TitleVisibilityStore — load/save de force_hidden, force_shown, show_hidden"
    - path: "source/platform/title_visibility_store.cpp"
      provides: "implementação do store (lê/grava thomaz-cache/title_visibility.txt)"
    - path: "tests/test_title_filter.cpp"
      provides: "testes doctest para as funções puras do title_filter"
  key_links:
    - from: "source/app/game_list_activity.cpp rebuildList()"
      to: "source/core/title_filter.hpp effectively_hidden()"
      via: "filtra allTitles_ com effectively_hidden antes de criar rows"
    - from: "source/app/game_list_activity.cpp botão X"
      to: "source/platform/title_visibility_store.cpp"
      via: "chama store_.toggle_show_hidden() + store_.save()"
    - from: "source/platform/title_visibility_store.cpp"
      to: "thomaz-cache/title_visibility.txt (Switch: /switch/thomaz/config/title_visibility.txt)"
      via: "read_text_file / write_text_file (padrão app_settings)"
---

<objective>
Implementar o sistema híbrido de ocultação de apps não-jogos da lista de títulos instalados.

Purpose: Forwarders de homebrew (ex.: Sphaira) aparecem como jogos na lista porque
nsListApplicationRecord não os distingue. A heurística automática
(save_data_size==0 && startup_user_account==0) os esconde por padrão; overrides manuais
por título e um toggle global permitem ao usuário ajustar.

Output:
- source/core/title_filter.{hpp,cpp} — lógica pura + testável
- source/platform/title_visibility_store.{hpp,cpp} — persistência de prefs
- title.hpp + title_service_switch/fake.cpp — campo startup_user_account adicionado
- game_list_activity — rebuildList(), botão X (frame), botão Y (por linha), badge Oculto
- i18n em 5 locales
- tests/test_title_filter.cpp + entrada no Makefile
</objective>

<execution_context>
@$HOME/.claude/gsd-core/workflows/execute-plan.md
@$HOME/.claude/gsd-core/templates/summary.md
</execution_context>

<context>
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/quick/260605-rcu-da-pra-ocultar-apps-que-nao-sao-jogos-da/260605-rcu-CONTEXT.md
@source/platform/title.hpp
@source/platform/title_service_switch.cpp
@source/platform/title_service_fake.cpp
@source/app/game_list_activity.hpp
@source/app/game_list_activity.cpp
@source/platform/app_settings.cpp
@source/platform/cheat_store.hpp
@tests/Makefile
@tests/test_api_base_url.cpp
@resources/i18n/pt-BR/thomaz.json
@resources/i18n/en-US/thomaz.json
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: Campo startup_user_account + lógica pura title_filter + persistência TitleVisibilityStore</name>
  <files>
    source/platform/title.hpp,
    source/platform/title_service_switch.cpp,
    source/platform/title_service_fake.cpp,
    source/core/title_filter.hpp,
    source/core/title_filter.cpp,
    source/platform/title_visibility_store.hpp,
    source/platform/title_visibility_store.cpp,
    tests/test_title_filter.cpp,
    tests/Makefile
  </files>
  <behavior>
    - classify(title) retorna TitleKind::Game quando save_data_size > 0 OU startup_user_account > 0; retorna TitleKind::NonGame somente quando ambos são 0.
    - effectively_hidden(title, force_hidden, force_shown, show_hidden_global) — note: show_hidden_global não entra nessa função; a função apenas resolve oculto/visível independente do toggle global:
        * id em force_shown → false (visível)
        * id em force_hidden → true (oculto)
        * senão → classify(title) == NonGame
    - Teste "SMO" (save 32MB, acct 1) → Game, effectively_hidden=false.
    - Teste "Sphaira" (save 0, acct 0) → NonGame, effectively_hidden=true sem overrides.
    - Teste BotW (save 0, acct 0) com force_shown contendo seu id → effectively_hidden=false.
    - Teste título qualquer em force_hidden → effectively_hidden=true independente de classify.
    - TitleVisibilityStore: construtor padrão carrega do arquivo (se ausente, defaults: force_hidden={}, force_shown={}, show_hidden=false). save() escreve o arquivo usando write_text_file(path, body) onde path é "thomaz-cache/title_visibility.txt" em desktop e "/switch/thomaz/config/title_visibility.txt" em __SWITCH__. Formato simples: linhas de texto, uma por entrada, prefixo "H:" para force_hidden id, "S:" para force_shown id, "SHOW_HIDDEN:1" ou "SHOW_HIDDEN:0". Exemplo:
        H:0x01234567890ABCDE
        S:0x0100000000010000
        SHOW_HIDDEN:0
    - toggle_show_hidden() inverte o bool show_hidden interno. toggle_title(uint64_t id) faz: se id em force_shown → remove de force_shown, add a force_hidden; se id em force_hidden → remove de force_hidden, add a force_shown; senão (estado default) → se effectively_hidden(id via classify) → add a force_shown; senão → add a force_hidden.
  </behavior>
  <action>
    1. Em source/platform/title.hpp adicionar `std::uint8_t startup_user_account = 0;` ao struct InstalledTitle, logo após a linha `std::uint64_t save_data_size = 0;`.

    2. Em source/platform/title_service_switch.cpp, dentro do bloco `if (R_SUCCEEDED(crc) && controlSize >= sizeof(control->nacp))`, adicionar após a linha que atribui save_data_size:
       `t.startup_user_account = control->nacp.startup_user_account;`

    3. Em source/platform/title_service_fake.cpp, alterar o lambda mk para aceitar um 7º parâmetro `std::uint8_t acct` e atribuir `t.startup_user_account = acct;`. Atualizar todas as chamadas mk existentes adicionando `1` como último argumento (SMO, SSBU, BotW, MK8, ACNH — todos com startup_user_account=1). Adicionar uma nova entrada no vetor de retorno: `mk(0x0500000000000001ULL, "Sphaira", "Sphaira Team", 0, "1.0.0", 0ULL, 0)` — o id 0x05xxxxxxx é o range típico de homebrew; save 0, acct 0.

    4. Criar source/core/title_filter.hpp com namespace thomaz::core. Definir enum class TitleKind { Game, NonGame }. Declarar:
       - `TitleKind classify(const InstalledTitle& t);`
       - `bool effectively_hidden(const InstalledTitle& t, const std::set<std::uint64_t>& force_hidden, const std::set<std::uint64_t>& force_shown);`
       Incluir somente <cstdint> e <set>; NÃO incluir headers libnx. Incluir "platform/title.hpp".

    5. Criar source/core/title_filter.cpp implementando:
       - classify: retorna NonGame somente se t.save_data_size==0 && t.startup_user_account==0; caso contrário Game.
       - effectively_hidden: se force_shown.count(t.title_id) return false; se force_hidden.count(t.title_id) return true; return classify(t)==TitleKind::NonGame.

    6. Criar source/platform/title_visibility_store.hpp no namespace thomaz. A classe TitleVisibilityStore tem membros privados: `std::set<uint64_t> force_hidden_`, `std::set<uint64_t> force_shown_`, `bool show_hidden_ = false`. Métodos públicos:
       - `void load();` — carrega do arquivo (silencia erros de arquivo ausente)
       - `void save() const;` — grava o arquivo; usa write_text_file + ensure_parent_dirs
       - `bool show_hidden() const;`
       - `void toggle_show_hidden();`
       - `const std::set<uint64_t>& force_hidden() const;`
       - `const std::set<uint64_t>& force_shown() const;`
       - `void toggle_title(const InstalledTitle& t);` — lógica descrita no behavior acima; recebe o título para poder chamar classify
       Include "platform/title.hpp" e "core/title_filter.hpp".

    7. Criar source/platform/title_visibility_store.cpp. A função anônima `visibility_file()` retorna `"/switch/thomaz/config/title_visibility.txt"` em __SWITCH__ e `"thomaz-cache/title_visibility.txt"` no desktop — espelhando o padrão de locale_file() em app_settings.cpp. load() chama read_text_file e parseia linha a linha: prefixo "H:" → stoul(linha+2, nullptr, 16) → insert em force_hidden_; "S:" → force_shown_; "SHOW_HIDDEN:1" → show_hidden_=true; "SHOW_HIDDEN:0" → show_hidden_=false. save() constrói a string com um loop sobre force_hidden_ (linhas "H:0x..."), depois force_shown_ ("S:0x..."), depois a linha "SHOW_HIDDEN:N", e chama write_text_file. Formatar os ids com std::hex usando snprintf (ex.: `snprintf(buf, sizeof(buf), "H:0x%016lX\n", id)`). toggle_title: se force_shown_.count(id) { force_shown_.erase(id); force_hidden_.insert(id); } else if (force_hidden_.count(id)) { force_hidden_.erase(id); force_shown_.insert(id); } else { if (effectively_hidden(t, {}, {})) force_shown_.insert(id); else force_hidden_.insert(id); }. Inclui "platform/title_visibility_store.hpp", "platform/cheat_store.hpp" (para write_text_file/read_text_file), "core/title_filter.hpp".

    8. Criar tests/test_title_filter.cpp com os casos doctest descritos no behavior. Inclui "doctest.h", "platform/title.hpp", "core/title_filter.hpp". Cria InstalledTitle inline (sem FakeTitleService). Não usa libnx.

    9. Em tests/Makefile, na variável SRCS, adicionar ao final da lista de fontes APENAS: `../source/platform/title_visibility_store.cpp`.
       ⚠️ NÃO adicionar `../source/core/title_filter.cpp` — o SRCS já contém `$(wildcard ../source/core/*.cpp)`, que pega title_filter.cpp automaticamente. Adicioná-lo explicitamente o compila DUAS vezes → erro de link "multiple definition / duplicate symbol". Os arquivos em ../source/platform/ NÃO são globbed (são listados um a um), por isso só o title_visibility_store.cpp precisa ser adicionado. O test_title_filter.cpp novo é pego pelo `$(wildcard *.cpp)` do tests/, então também não precisa de entrada.
  </action>
  <verify>
    <automated>
      cd /home/solid/www/personal/playground/thomas/tests && make clean && make && ./run --test-case="title_filter*"
    </automated>
  </verify>
  <done>
    - `make` compila sem erros em tests/
    - `./run --test-case="title_filter*"` passa todos os casos: classify Game/NonGame, effectively_hidden sem overrides, com force_shown, com force_hidden
    - source/core/title_filter.hpp e .cpp existem sem includes libnx
    - source/platform/title_visibility_store.hpp e .cpp existem
    - title.hpp tem o campo startup_user_account
  </done>
</task>

<task type="auto">
  <name>Task 2: GameListActivity — rebuildList(), botão X, botão Y, badge Oculto</name>
  <files>
    source/app/game_list_activity.hpp,
    source/app/game_list_activity.cpp
  </files>
  <action>
    O objetivo é substituir o fluxo populate()-único pelo fluxo allTitles_ + store_ + rebuildList(), adicionando os dois botões e o badge.

    1. Em game_list_activity.hpp adicionar membros privados:
       - `std::vector<InstalledTitle> allTitles_;`
       - `TitleVisibilityStore store_;`
       - `void rebuildList();`
       Adicionar includes necessários: "core/title_filter.hpp" e "platform/title_visibility_store.hpp".

    2. Em game_list_activity.cpp, em onContentAvailable(), após install_help_action, registrar os dois botões no frame ANTES de disparar o async (o frame deve existir; getView retorna ponteiro nulo se chamado antes do XML estar pronto, mas onContentAvailable já é post-XML, então ok):

       ```
       store_.load();

       if (auto* frame = this->getView("gameListFrame")) {
           frame->registerAction(
               "thomaz/games/toggle_show_hidden"_i18n, brls::BUTTON_X,
               [this](brls::View*) {
                   store_.toggle_show_hidden();
                   store_.save();
                   this->rebuildList();
                   return true;
               }, false);
       }
       ```

       O botão Y por linha será registrado dentro de rebuildList() (ver abaixo), não aqui.

    3. Manter o async existente em onContentAvailable() mas no callback da UI thread (lambda que chama populate) — renomear para: salvar os títulos em allTitles_ e então chamar rebuildList(). Ou seja:
       ```
       [this, titles]() {
           allTitles_ = *titles;
           this->rebuildList();
       }
       ```
       A função populate() pode ser mantida como wrapper ou inlined em rebuildList(); prefira transformar populate() em rebuildList() diretamente (renomear e ajustar assinatura para void rebuildList()).

    4. rebuildList() começa com:
       ```
       auto* listBox  = dynamic_cast<brls::Box*>(this->getView("gameListBox"));
       auto* emptyLabel = dynamic_cast<brls::Label*>(this->getView("emptyLabel"));
       if (!listBox || !emptyLabel) return;

       // limpa o spinner na primeira chamada (idempotente: setVisibility(GONE) em algo já GONE é no-op)
       if (auto* spinner = this->getView("spinner"))
           spinner->setVisibility(brls::Visibility::GONE);

       listBox->clearViews(true);  // Box::clearViews(bool free=true) — libera os filhos
       this->hasCheatBadges.clear();
       ```

    5. Construir a lista visível: iterar sobre allTitles_, determinar efetivamente_hidden via `core::effectively_hidden(title, store_.force_hidden(), store_.force_shown())`, e:
       - Se !store_.show_hidden() && efetivamente_hidden → continue (pula)
       - Caso contrário → adicionar a row (código de row existente de populate(), movido aqui)

    6. Para cada row adicionada, se efetivamente_hidden && store_.show_hidden(), adicionar badge logo após o nameLabel (antes do versionLabel):
       ```
       row->addView(makeBadge("thomaz/games/badge_hidden"_i18n,
                              nvgRGBA(0x80, 0x80, 0x80, 0x40), nvgRGB(0xC0, 0xC0, 0xC0)));
       ```
       (cinza neutro para não colidir com os outros badges)

    7. Para cada row adicionada, registrar o botão Y com caption dinâmico: se efetivamente_hidden usar "thomaz/games/unhide"_i18n, senão "thomaz/games/hide"_i18n:
       ```
       InstalledTitle rowTitleCopy = title;  // captura por valor
       bool isHidden = eh;  // effectively_hidden calculado acima
       row->registerAction(
           isHidden ? "thomaz/games/unhide"_i18n : "thomaz/games/hide"_i18n,
           brls::BUTTON_Y,
           [this, rowTitleCopy](brls::View*) {
               store_.toggle_title(rowTitleCopy);
               store_.save();
               this->rebuildList();
               return true;
           }, false);
       ```

    8. O entry "clear cheats" (Target::Cheats) deve ser re-inserido no topo do listBox igual ao populate() original — mover o bloco inteiro para rebuildList().

    9. Ao final de rebuildList(), se a lista visível for vazia mostrar emptyLabel; caso contrário ocultá-lo — igual à lógica existente em populate().

    10. loadCheatIndexAsync() é chamado ao final de rebuildList() da mesma forma que hoje em populate(). Atenção: hasCheatBadges foi limpo no início de rebuildList(), então o novo vetor será preenchido durante a iteração das rows nesta chamada. Só chamar loadCheatIndexAsync() se target == Target::Cheats e !hasCheatBadges.empty() (já é o que a função verifica internamente).

    Não criar nenhum thread novo. rebuildList() é chamado SOMENTE a partir da UI thread (callbacks registerAction e o lambda da runAsync já rodam na UI thread).
  </action>
  <verify>
    <automated>
      cd /home/solid/www/personal/playground/thomas && cmake -B build-desktop -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5 && cmake --build build-desktop --target thomaz -- -j$(nproc) 2>&1 | tail -20
    </automated>
  </verify>
  <done>
    - cmake --build conclui sem erros para o alvo thomaz
    - game_list_activity.cpp usa rebuildList() em vez de populate() one-shot
    - Botão X registrado no frame "gameListFrame" altera show_hidden
    - Botão Y registrado por row alterna hide/unhide do título
    - Badge "Oculto" inserido nas rows efetivamente ocultas quando show_hidden=true
    - store_.load() chamado em onContentAvailable(); store_.save() chamado após cada toggle
  </done>
</task>

<task type="auto">
  <name>Task 3: i18n — 5 locales com chaves de visibilidade em thomaz/games/</name>
  <files>
    resources/i18n/pt-BR/thomaz.json,
    resources/i18n/en-US/thomaz.json,
    resources/i18n/fr/thomaz.json,
    resources/i18n/ru/thomaz.json,
    resources/i18n/zh-Hans/thomaz.json
  </files>
  <action>
    Adicionar 5 chaves dentro do objeto "games" em pt-BR e en-US (que já têm o objeto "games"); criar o objeto "games" nos outros 3 locales (que não o têm).

    Chaves a adicionar (em cada locale):
    - "hide"             — ação de ocultar um título
    - "unhide"           — ação de mostrar um título oculto
    - "toggle_show_hidden" — legenda do botão X no frame (alternância do toggle global)
    - "badge_hidden"     — texto do badge na row

    Textos por locale:

    pt-BR (dentro do "games" existente):
    ```json
    "hide": "Ocultar",
    "unhide": "Mostrar",
    "toggle_show_hidden": "Ocultos",
    "badge_hidden": "Oculto"
    ```

    en-US (dentro do "games" existente):
    ```json
    "hide": "Hide",
    "unhide": "Show",
    "toggle_show_hidden": "Hidden",
    "badge_hidden": "Hidden"
    ```

    fr (criar objeto "games" no JSON — o arquivo já tem "saves", "tls", "boot"):
    ```json
    "games": {
        "hide": "Masquer",
        "unhide": "Afficher",
        "toggle_show_hidden": "Masqués",
        "badge_hidden": "Masqué"
    }
    ```

    ru:
    ```json
    "games": {
        "hide": "Скрыть",
        "unhide": "Показать",
        "toggle_show_hidden": "Скрытые",
        "badge_hidden": "Скрыто"
    }
    ```

    zh-Hans:
    ```json
    "games": {
        "hide": "隐藏",
        "unhide": "显示",
        "toggle_show_hidden": "隐藏项目",
        "badge_hidden": "已隐藏"
    }
    ```

    Atenção: em pt-BR e en-US, as novas chaves devem ser inseridas dentro do objeto "games" já existente, preservando todas as chaves existentes (title, empty, coming_soon, badge_has_cheats, badge_active). Usar vírgulas JSON corretamente.
  </action>
  <verify>
    <automated>
      python3 -c "
import json, sys
files = [
    'resources/i18n/pt-BR/thomaz.json',
    'resources/i18n/en-US/thomaz.json',
    'resources/i18n/fr/thomaz.json',
    'resources/i18n/ru/thomaz.json',
    'resources/i18n/zh-Hans/thomaz.json',
]
required = ['hide','unhide','toggle_show_hidden','badge_hidden']
ok = True
for f in files:
    with open(f) as fh:
        d = json.load(fh)
    games = d.get('games', {})
    missing = [k for k in required if k not in games]
    if missing:
        print(f'MISSING in {f}: {missing}')
        ok = False
    else:
        print(f'OK {f}')
sys.exit(0 if ok else 1)
" && cd /home/solid/www/personal/playground/thomas && cmake --build build-desktop --target thomaz -- -j$(nproc) 2>&1 | tail -5
    </automated>
  </verify>
  <done>
    - python3 valida que todos os 5 arquivos JSON são válidos e contêm as 4 chaves em "games"
    - cmake --build continua passando (i18n não quebra o build)
    - Chaves existentes em pt-BR e en-US ("badge_has_cheats", "badge_active", etc.) são preservadas
  </done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| SD card → app | Arquivo title_visibility.txt lido do SD (ou working dir); conteúdo pode estar corrompido ou ausente |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-rcu-01 | Tampering | title_visibility.txt (SD card) | mitigate | Parsear linha a linha com validação: id deve ser hex 16 dígitos; linhas inválidas são ignoradas silenciosamente (não causam crash) |
| T-rcu-02 | Denial of Service | listBox->clearViews em rebuildList | accept | clearViews(true) é operação de UI thread, não exposta externamente; sem surface de ataque |
| T-rcu-03 | Information Disclosure | force_hidden/force_shown ids no arquivo | accept | Apenas title_ids já presentes na lista instalada (não dados sensíveis); arquivo local no SD |
</threat_model>

<verification>
Sequência de verificação completa:

1. `cd tests && make clean && make && ./run` — todos os testes passam incluindo test_title_filter*
2. `cmake --build build-desktop --target thomaz` — sem erros de compilação
3. `python3` valida JSON dos 5 locales
4. Execução manual no desktop: lista exibe SMO/SSBU/MK8/ACNH/BotW; Sphaira fica oculta por padrão; pressionar X revela Sphaira com badge cinza "Oculto"; pressionar Y em Sphaira força visibilidade permanente; pressionar Y novamente a oculta novamente
</verification>

<success_criteria>
- Build desktop compila sem warnings novos
- Todos os testes existentes continuam passando (make && ./run em tests/)
- Novos testes test_title_filter.cpp cobrem os 4 casos (Game, NonGame, force_shown, force_hidden)
- Os 5 locales têm as 4 novas chaves em thomaz/games/ com JSON válido
- Na execução desktop: Sphaira (save=0, acct=0) não aparece por padrão; X mostra/esconde ocultos com badge; Y por linha persiste override
</success_criteria>

<output>
Criar `.planning/quick/260605-rcu-da-pra-ocultar-apps-que-nao-sao-jogos-da/260605-rcu-SUMMARY.md` ao concluir
</output>
