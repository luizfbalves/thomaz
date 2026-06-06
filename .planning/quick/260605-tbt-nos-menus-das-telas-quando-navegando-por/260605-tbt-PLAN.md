---
phase: quick-260605-tbt
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - source/app/thomaz_activity.hpp
  - source/app/game_list_activity.cpp
  - source/app/save_manager_activity.cpp
  - source/app/mod_browser_activity.cpp
  - source/app/mod_detail_activity.cpp
  - source/app/clear_cheats_activity.cpp
autonomous: true
requirements: [260605-tbt]

must_haves:
  truths:
    - "Ao abrir qualquer tela com lista construída async por gamepad, o highlight de foco aparece dentro da nova tela (no primeiro item), não preso no card da tela anterior"
    - "O guard initialFocusClaimed_ impede que rebuilds de lista (toggles X/Y) roubem o foco do item atual"
    - "claimInitialFocus é no-op quando a lista está vazia (getDefaultFocus retorna null)"
  artifacts:
    - path: "source/app/thomaz_activity.hpp"
      provides: "Membro initialFocusClaimed_ + método protegido claimInitialFocus(brls::View*)"
      contains: "initialFocusClaimed_"
    - path: "source/app/game_list_activity.cpp"
      provides: "Chamada claimInitialFocus(listBox) no fim de rebuildList()"
      contains: "claimInitialFocus"
    - path: "source/app/save_manager_activity.cpp"
      provides: "Chamada claimInitialFocus(listBox) no fim de populate()"
      contains: "claimInitialFocus"
    - path: "source/app/mod_browser_activity.cpp"
      provides: "Chamada claimInitialFocus(resultsBox) no fim de populate()"
      contains: "claimInitialFocus"
    - path: "source/app/mod_detail_activity.cpp"
      provides: "Chamada claimInitialFocus(filesBox) no fim de populate()"
      contains: "claimInitialFocus"
    - path: "source/app/clear_cheats_activity.cpp"
      provides: "Chamada claimInitialFocus(listBox) no fim de populate()"
      contains: "claimInitialFocus"
  key_links:
    - from: "game_list_activity.cpp rebuildList()"
      to: "ThomazActivity::claimInitialFocus"
      via: "this->claimInitialFocus(listBox)"
      pattern: "claimInitialFocus"
---

<objective>
Corrigir o highlight de foco (borda animada do Borealis) que fica preso no card da tela anterior ao navegar por gamepad para telas cujo conteúdo focável é construído de forma assíncrona.

Root cause (D-01 via CONTEXT.md): pushActivity chama giveFocus(getDefaultFocus()), mas getDefaultFocus retorna null enquanto o spinner está visível → giveFocus é no-op → currentFocus permanece no card anterior → highlight desenhado por cima da nova tela.

Fix: adicionar helper claimInitialFocus(container) em ThomazActivity, chamado ao final do build de cada lista async. O guard initialFocusClaimed_ garante idempotência: rebuilds (toggles X/Y de visibilidade de jogos) não roubam o foco.

Output: thomaz_activity.hpp modificado + 5 activity .cpp modificados.
</objective>

<execution_context>
@$HOME/.claude/gsd-core/workflows/execute-plan.md
@$HOME/.claude/gsd-core/templates/summary.md
</execution_context>

<context>
@.planning/quick/260605-tbt-nos-menus-das-telas-quando-navegando-por/260605-tbt-CONTEXT.md
@source/app/thomaz_activity.hpp
@source/app/game_list_activity.cpp
@source/app/save_manager_activity.cpp
@source/app/mod_browser_activity.cpp
@source/app/mod_detail_activity.cpp
@source/app/clear_cheats_activity.cpp
</context>

<tasks>

<task type="auto">
  <name>Task 1: Adicionar claimInitialFocus em ThomazActivity + aplicar em game_list e save_manager</name>
  <files>
    source/app/thomaz_activity.hpp,
    source/app/game_list_activity.cpp,
    source/app/save_manager_activity.cpp
  </files>
  <action>
Em source/app/thomaz_activity.hpp:

1. Adicionar membro privado logo antes do fechamento da class (antes do `}`):
   `bool initialFocusClaimed_ = false;`

2. Adicionar método protegido (seção `protected:`, após cancelledFlag):
   ```
   void claimInitialFocus(brls::View* container)
   {
       if (initialFocusClaimed_ || !container) return;
       brls::View* f = container->getDefaultFocus();
       if (!f) return;
       brls::Application::giveFocus(f);
       initialFocusClaimed_ = true;
   }
   ```
   Sem include extra necessário: borealis.hpp já está incluído no header.
   Sem thomaz_activity.cpp novo — inline no header é suficiente e mais simples.

Em source/app/game_list_activity.cpp, método rebuildList():
- Localizar o bloco final que ajusta a visibilidade do emptyLabel/listBox (linhas ~263-273).
  Há também a chamada this->loadCheatIndexAsync() no fim.
- Inserir `this->claimInitialFocus(listBox);` APÓS a visibilidade ser definida e ANTES de
  loadCheatIndexAsync(). O listBox já tem rows quando rebuildList chegou até ali (ou está
  vazio com emptyLabel visível — nesse caso getDefaultFocus retorna null e o helper é no-op).
- O guard initialFocusClaimed_ garante que as chamadas subsequentes de rebuildList() via
  toggle X/Y não reposicionem o foco.

Em source/app/save_manager_activity.cpp, método populate():
- Localizar o ponto após o loop que faz `listBox->addView(row)` para cada title (a última
  linha funcional do método, linha ~127).
- Inserir `this->claimInitialFocus(listBox);` ao final do método, após o loop de rows.
  (O retorno antecipado para titles.empty() ocorre antes — se chegou ao loop, há rows.)
  </action>
  <verify>
    <automated>
grep -n "initialFocusClaimed_" /home/solid/www/personal/playground/thomas/source/app/thomaz_activity.hpp
grep -n "claimInitialFocus" /home/solid/www/personal/playground/thomas/source/app/thomaz_activity.hpp
grep -n "claimInitialFocus" /home/solid/www/personal/playground/thomas/source/app/game_list_activity.cpp
grep -n "claimInitialFocus" /home/solid/www/personal/playground/thomas/source/app/save_manager_activity.cpp
    </automated>
  </verify>
  <done>
    - thomaz_activity.hpp contém `initialFocusClaimed_ = false` (membro privado) e método `claimInitialFocus` implementado inline com o guard if(initialFocusClaimed_ || !container).
    - game_list_activity.cpp contém `this->claimInitialFocus(listBox)` ao final de rebuildList(), antes de loadCheatIndexAsync().
    - save_manager_activity.cpp contém `this->claimInitialFocus(listBox)` ao final de populate().
    - Compilação pelo toolchain Switch (verificação pelo usuário) — não testável aqui (sem devkitPro, e main.cpp inclui switch.h que quebra o build desktop).
  </done>
</task>

<task type="auto">
  <name>Task 2: Aplicar claimInitialFocus em mod_browser, mod_detail e clear_cheats</name>
  <files>
    source/app/mod_browser_activity.cpp,
    source/app/mod_detail_activity.cpp,
    source/app/clear_cheats_activity.cpp
  </files>
  <action>
Em source/app/mod_browser_activity.cpp, método populate():
- Container: `resultsBox` (brls::Box*, obtido via `this->getView("resultsBox")`).
- Localizar o fim do método, após o bloco que adiciona a "Load more" row (ou após o
  early-return de resultado vazio).
- Inserir `this->claimInitialFocus(resultsBox);` como última linha antes do fechamento
  do método. Colocar APÓS todos os addView (incluindo o moreRow, se presente), para que
  getDefaultFocus() encontre o primeiro elemento. Se result.records.empty() == true, o
  método retorna cedo antes dessa linha — ok, lista vazia, no-op de qualquer forma.
  Portanto: inserir a chamada após o bloco `!this->lastPage.is_complete` (load more).
- Note: populate() é chamada também por runGameSearch e runGlobalSearch (novos fetches).
  O guard initialFocusClaimed_ garante que apenas a primeira carga reivindica o foco;
  buscas subsequentes não reposicionam.

Em source/app/mod_detail_activity.cpp, método populate():
- Container: `filesBox` (brls::Box*, `this->getView("filesBox")`).
- Localizar o fim do loop que faz `filesBox->addView(row)` para cada file (~linha 162).
- Inserir `this->claimInitialFocus(filesBox);` como última linha após o loop.
  Os early-returns para NetworkError / NotFound / empty ocorrem antes — nesse caminho,
  a lista tem rows e o primeiro row focável receberá o foco.

Em source/app/clear_cheats_activity.cpp, método populate():
- Container: `listBox` (brls::Box*, `this->getView("clearListBox")`).
- O primeiro elemento focável adicionado ao listBox é o selectAll (BooleanCell), então
  o foco será claim no selectAll quando a lista terminar de montar.
- Inserir `this->claimInitialFocus(listBox);` como última linha do método, após
  `listBox->addView(clearBtn)`.
  </action>
  <verify>
    <automated>
grep -n "claimInitialFocus" /home/solid/www/personal/playground/thomas/source/app/mod_browser_activity.cpp
grep -n "claimInitialFocus" /home/solid/www/personal/playground/thomas/source/app/mod_detail_activity.cpp
grep -n "claimInitialFocus" /home/solid/www/personal/playground/thomas/source/app/clear_cheats_activity.cpp
    </automated>
  </verify>
  <done>
    - mod_browser_activity.cpp contém `this->claimInitialFocus(resultsBox)` ao final de populate(), após todos os addView.
    - mod_detail_activity.cpp contém `this->claimInitialFocus(filesBox)` ao final do loop de files em populate().
    - clear_cheats_activity.cpp contém `this->claimInitialFocus(listBox)` como última linha de populate(), após clearBtn.
    - Compilação pelo toolchain Switch (verificação pelo usuário) — não testável aqui (sem devkitPro).
  </done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Descrição |
|----------|-----------|
| UI thread → Borealis focus API | giveFocus chamado somente da UI thread (onSync callback) — sem risco de race |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation |
|-----------|----------|-----------|-------------|------------|
| T-260605tbt-01 | Tampering | initialFocusClaimed_ reset | accept | Flag é instância por activity; cada push/pop cria nova instância — sem risco de estado compartilhado |
| T-260605tbt-02 | Denial of Service | claimInitialFocus chamado fora da UI thread | accept | runAsync garante que onSync (onde as chamadas vivem) executa na UI thread; invariante já mantido pelo padrão existente |
</threat_model>

<verification>
Verificação realista dado que o toolchain Switch não está disponível e main.cpp inclui &lt;switch.h&gt;:

1. Grep de presença (automático, nas tasks acima):
   - initialFocusClaimed_ e claimInitialFocus existem em thomaz_activity.hpp
   - Cada um dos 5 .cpp contém exatamente uma chamada this->claimInitialFocus

2. Inspeção manual do posicionamento das chamadas:
   - game_list: após visibilidade ajustada, antes de loadCheatIndexAsync
   - save_manager, mod_detail, clear_cheats: última linha de populate()
   - mod_browser: após todos os addView em populate() (incluindo load-more row)

3. Lógica do guard:
   - initialFocusClaimed_ começa false, vira true na primeira chamada bem-sucedida
   - Rebuilds seguintes de game_list (toggle X/Y) → early-return no guard → foco não roubado

4. Compile + hardware test (responsabilidade do usuário):
   - Build com devkitPro: `make` no ambiente Switch
   - Executar no hardware: navegar por d-pad da Home → game list → confirm que o highlight
     aparece na nova tela e não no card anterior
</verification>

<success_criteria>
- thomaz_activity.hpp: membro `bool initialFocusClaimed_ = false;` presente (private) + método `claimInitialFocus(brls::View*)` implementado inline (protected) com guard duplo (initialFocusClaimed_ + null-check + getDefaultFocus null-check)
- 5 activity .cpp modificados com chamada `this->claimInitialFocus(&lt;container&gt;)` no ponto correto (ao final do build da lista)
- Containers corretos por tela: gameListBox (game_list), saveListBox (save_manager), resultsBox (mod_browser), filesBox (mod_detail), clearListBox (clear_cheats)
- Nenhuma regressão nos testes desktop existentes (cd tests && make && ./run — as 6 falhas pré-existentes de refactor Switch-only permanecem, mas não pioram)
- Compilação e teste no hardware Switch: highlight de foco aparece corretamente na nova tela ao navegar por gamepad
</success_criteria>

<output>
Create `.planning/quick/260605-tbt-nos-menus-das-telas-quando-navegando-por/260605-tbt-SUMMARY.md` when done
</output>
