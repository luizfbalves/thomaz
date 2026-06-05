# Quick Task 260605-s2h: Indicadores de sistema na navbar — Context

**Gathered:** 2026-06-05
**Status:** Ready for planning

<domain>
## Task Boundary

No header (navbar = `brls/applet_frame/hint_box`, lado direito do AppletFrame) de
TODAS as telas, exibir: espaço do cartão SD (barrinha horizontal de uso), espaço
do console/NAND (barrinha), e conexão WiFi (ícone + nível de sinal 0–3).
</domain>

<decisions>
## Implementation Decisions (LOCKED — confirmadas com o usuário)

### Verificação (decisão: "Implementar — eu compilo")
- NÃO há devkitPro/libnx neste ambiente; o build desktop do app está quebrado
  (main.cpp agora inclui <switch.h> sem guard — milestone Switch-Only).
- Só os testes desktop (doctest) compilam, e só pegam core/* + um subconjunto de platform/*.
- Portanto: código Switch (ns + nifm) é escrito contra a libnx mas NÃO é
  compile-checado aqui. A lógica PURA (formatar bytes, ratio da barra) vai num
  arquivo core/ e É coberta por teste desktop. O usuário compila no toolchain Switch.
- O executor NÃO deve afirmar que o build Switch passou — apenas rodar os testes
  desktop dos helpers puros e revisar o código.

### Onde (decisão: "Header de todas as telas")
- Novo helper `install_system_status(brls::Activity*)` (em app_header.hpp/.cpp,
  junto de install_header_username) injeta os indicadores no `hint_box`.
- Chamar em cada activity que já chama install_header_username (13 arquivos).
- Ordem no hint_box (row): adicionar os indicadores ANTES do username, então fica
  [SD][NAND][WiFi][@usuario]. (chamar install_system_status antes de install_header_username)

### WiFi (decisão: "Ícone + nível de sinal")
- Sem SSID. Mostrar nível 0–3 via nifm. Renderizar como 3 barrinhas verticais
  crescentes (Boxes): as primeiras `strength` acesas, o resto apagadas. Sem
  dependência de fonte de ícones (não há uso de glyph font no projeto hoje).
- Desconectado: barras apagadas / cor dim.

### APIs Switch (codificar da libnx — não há header local para checar)
- Espaço: `nsGetTotalSpaceSize(NcmStorageId, s64*)` e `nsGetFreeSpaceSize(NcmStorageId, s64*)`.
  SD = `NcmStorageId_SdCard`; Console/NAND = `NcmStorageId_BuiltInUser`. O serviço `ns`
  já é inicializado no startup (NsTitleService chama nsInitialize) e vive o app todo.
- WiFi: `nifmInitialize(NifmServiceType_User)` → `nifmGetInternetConnectionStatus(&type,&strength,&status)`
  → conectado se status == NifmInternetConnectionStatus_Connected; strength 0–3 → `nifmExit()`.
- O repo já usa fs/ncm libnx (key_loader_switch.cpp usa fsOpenBisFileSystem,
  NcmStorageId_BuiltInSystem), então perms/patterns existem.

### Barrinha horizontal (storage)
- Track (Box arredondado, fundo dim) + fill (Box bright, largura = used_ratio).
- Label compacto ao lado/acima: ex. "SD 45.2/64 GB" (fonte pequena ~11–12).
- used_ratio = (total-free)/total, clampeado 0..1; total==0 → 0 (evita div/0).

### Claude's Discretion
- Larguras/alturas/cores exatas das barras; formato exato do label.
- Nomes de arquivos/funções e chaves i18n.
- Se nifm é init por chamada (simples) ou uma vez (deixar por-chamada é aceitável).
</decisions>

<specifics>
## Specific Ideas

- Helper deve ser robusto fora do Switch: a função de query fica em platform/
  com `#ifdef __SWITCH__` real e `#else` retornando zeros (stub barato), para não
  virar landmine se o build desktop for restaurado.
- query_system_status() retorna struct com sd{total,free}, nand{total,free},
  wifi_connected(bool), wifi_strength(int 0–3).
</specifics>

<canonical_refs>
## Canonical References

- `lib/borealis/library/lib/views/applet_frame.cpp` — header XML embarcado: hint_box id `brls/applet_frame/hint_box` (row, à direita)
- `source/app/app_header.cpp` — install_header_username adiciona Label ao hint_box (padrão a espelhar)
- `source/platform/title_service_switch.cpp` — padrão de TU Switch-only (#ifdef __SWITCH__) + nsInitialize
- `source/platform/themes/key_loader_switch.cpp` — uso existente de fs/ncm libnx (perms ok)
- `tests/Makefile` — globa tests/*.cpp e ../source/core/*.cpp (helper puro auto-incluído; NÃO adicionar core/* manualmente → símbolo duplicado)
- 13 call sites de install_header_username(this): cheat_detail, clear_cheats, game_list, home, mod_browser, mod_detail, mod_manager, save_detail, save_manager, settings, system, theme_browser, theme_detail
</canonical_refs>
