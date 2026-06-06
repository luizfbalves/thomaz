# Quick Task 260605-tbt: Borda de foco (highlight) presa ao navegar por botão — Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Bug

Navegando por gamepad/d-pad (não touch), ao abrir uma tela cujo conteúdo focável
é construído de forma ASSÍNCRONA (listas de jogos/cheats, saves, mods), a borda
animada de foco (highlight) do botão da tela ANTERIOR (ex.: card da Home) fica
desenhada por cima da nova tela. Só acontece com botão, não com touch.
</domain>

<root_cause>
- O Borealis desenha o highlight como overlay GLOBAL em volta de `currentFocus`,
  e SÓ quando o input não é touch (application.cpp:788 — por isso só no botão).
- `pushActivity` chama `giveFocus(activity->getDefaultFocus())`. Mas `giveFocus`
  é NO-OP quando o argumento resolve para nullptr (application.cpp:884: só troca
  o foco se newFocus != nullptr).
- Telas cujo conteúdo focável é criado num callback async (onSync) têm, no momento
  do push, apenas um spinner (XML sem nenhum elemento focável). Então
  getDefaultFocus()==null → giveFocus(null) é no-op → `currentFocus` continua no
  card da tela anterior (agora escondida) → seu highlight é desenhado por cima.
- lib/borealis É SUBMÓDULE → NÃO corrigir no core; corrigir no app.
</root_cause>

<decisions>
## Implementation Decisions (LOCKED)

### Fix: helper compartilhado em ThomazActivity
Adicionar em `source/app/thomaz_activity.hpp` (base de todas as activities):
```cpp
// Dá foco ao primeiro descendente focável de `container`, mas SOMENTE na primeira
// vez (rebuilds de lista — ex.: toggles de ocultar apps — não devem roubar o foco
// de volta). Corrige o highlight preso da activity anterior quando o conteúdo
// focável é construído de forma assíncrona.
void claimInitialFocus(brls::View* container);
private: bool initialFocusClaimed_ = false;
```
Implementação (no .hpp inline ou novo thomaz_activity.cpp):
```cpp
void claimInitialFocus(brls::View* container) {
    if (initialFocusClaimed_ || !container) return;
    brls::View* f = container->getDefaultFocus(); // primeiro focável descendente
    if (!f) return;                               // lista ainda vazia → tenta depois
    brls::Application::giveFocus(f);
    initialFocusClaimed_ = true;
}
```
Nota: hoje ThomazActivity só tem header em .hpp (sem .cpp). Pode implementar inline
no header (precisa incluir <borealis.hpp> que já vem). Se criar thomaz_activity.cpp,
ele é pego pelo GLOB_RECURSE do app.

### Onde chamar (telas com conteúdo focável construído async, SEM focável no XML)
Após construir a lista/grid focável, chamar `this->claimInitialFocus(<container>)`:
- game_list_activity.cpp — ao final de rebuildList(), container = getView("gameListBox") (reportado pelo usuário)
- save_manager_activity.cpp — após montar a lista
- mod_browser_activity.cpp — após montar o grid/linhas
- mod_detail_activity.cpp — após montar as linhas de arquivos
- clear_cheats_activity.cpp — após montar o botão/conteúdo

O planner deve LER cada arquivo e achar o container correto (o Box onde as linhas
são addView'd) e o ponto pós-build (dentro do onSync callback). Só chamar quando
houver conteúdo (se vazio, getDefaultFocus null → no-op, ok).

### Guard de "primeira vez"
Essencial no game_list: rebuildList() é chamado também nos toggles X/Y (feature de
ocultar apps). Sem o guard, cada toggle jogaria o foco pro primeiro item. O
initialFocusClaimed_ garante que o foco só é reivindicado uma vez (na carga
inicial), preservando a posição do usuário nos rebuilds.

### Fora de escopo (não tocar agora)
- cheat_detail_activity (sem build focável dinâmico claro — usa cells/frame actions)
- theme_detail_activity (tem 1 focável no XML; theme_browser já dá giveFocus)
- settings/system/mod_manager/home (conteúdo focável já presente no push — não bugados)
Se o usuário ainda ver o bug nessas, follow-up.

### Verificação
- NÃO dá pra compilar aqui (main.cpp inclui <switch.h>; sem devkitPro). Verificação
  = revisão de código + o usuário compila. NÃO é lógica unit-testável (depende de
  views Borealis). NÃO rodar `cmake --build ... thomaz` (falha pelo switch.h).
- Conferir que os testes desktop existentes NÃO regridem (não devem ser afetados).
</decisions>

<canonical_refs>
- lib/borealis/library/lib/core/application.cpp:788 (highlight só desenha se input != touch)
- lib/borealis/library/lib/core/application.cpp:884 (giveFocus no-op se newFocus==null)
- source/app/theme_browser_activity.cpp:240 — padrão de referência: `if (firstCard) giveFocus(firstCard);`
- source/app/save_detail_activity.cpp:100 — giveFocus(bb)
- source/app/boot_activity.cpp:58 — giveFocus(loginBtn)
- source/app/thomaz_activity.hpp — base de todas as activities (onde vai o helper)
- source/app/game_list_activity.cpp rebuildList() — container "gameListBox"
</canonical_refs>
