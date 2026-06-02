# thomaz — Design (v1)

**Data:** 2026-06-02
**Tipo:** Homebrew para Nintendo Switch (aplicativo NRO de tela cheia)
**Status:** Design aprovado, pronto para planejamento de implementação

---

## 1. Visão geral

**thomaz** é um homebrew para Nintendo Switch (CFW Atmosphère) que funciona como um
**hub de utilitários**. A home é uma grade estilo *bento* (cards modernos), e cada
funcionalidade é um **módulo** plugável.

Na v1, existe **um** módulo funcional: **Trapaças** (gerenciador de cheats). Os demais
slots da home aparecem como cards **"Em breve" desativados**, comunicando a visão de
plataforma sem prometer funcionalidades.

O objetivo do projeto é entregar uma alternativa aos cheat managers existentes
(Breeze, EdiZon-SE, AIO-switch-updater) com **UX bonita, amigável e fácil de usar** —
o diferencial declarado, já que as ferramentas atuais são funcionais mas têm interfaces
datadas.

### O que o módulo Trapaças faz

Permite ao usuário, **com os jogos fechados**:
1. Ver todos os jogos instalados (nome, ícone, versão).
2. Baixar cheats prontos para um jogo a partir de uma base online open-source.
3. Ligar/desligar cheats individualmente por jogo.

O Atmosphère aplica os cheats automaticamente no próximo boot do jogo. Nenhum jogo
precisa estar aberto durante a configuração.

---

## 2. Escopo

### Dentro da v1
- Hub com home **bento** e registro de módulos.
- Módulo **Trapaças**: listar jogos instalados, baixar cheats da base online, ligar/desligar por jogo.
- Gerenciamento dos cheats já presentes no SD (ler estado atual).
- Interface **bilíngue PT-BR + EN** com seletor de idioma (padrão detectado do console).
- Suporte a **toque + controle**.
- Cards **"Em breve" desativados** na home.

### Fora da v1 (futuro)
- Edição de saves dos jogos.
- Criação de cheats via busca de memória em tempo real (overlay Tesla / `dmnt:cht`).
- Colar/importar cheats manuais escritos pelo usuário.
- Qualquer módulo além de Trapaças.

### Não-objetivos
- Pirataria ou distribuição de conteúdo protegido. O app lida apenas com **códigos de
  cheat** (texto) e software próprio.
- Suporte a firmwares customizados além do Atmosphère na v1 (detecta e avisa se ausente).

---

## 3. Decisões tomadas

| Tema | Decisão |
|---|---|
| Nome do app | **thomaz** |
| Formato | Aplicativo **NRO** de tela cheia (rodado pelo hbmenu) — *não* overlay |
| Coração da v1 | Gerenciador de cheats puro |
| Fonte dos cheats | Baixar da base online **switch-cheats-db** + gerenciar o que já está no SD |
| Interação | Toque + controle |
| Tecnologia de UI | **Borealis** (natinusala/borealis) com tema customizado |
| Idiomas | Bilíngue PT-BR + EN (i18n do Borealis); código/identificadores em inglês |
| Estrutura | Hub (AppShell) + módulos plugáveis; v1 = 1 módulo (Trapaças) |
| Home | Grade **bento**; card grande "Trapaças" + slots "Em breve" desativados |
| Modelo de ativação | Arquivo `.txt` no SD contém **só os cheats ligados** (+ master code) |

---

## 4. Arquitetura

### Camadas

| Camada | Conteúdo | Depende de |
|---|---|---|
| **AppShell** | Home bento, registro de módulos, navegação raiz, configurações globais (idioma, sobre, aviso de ban) | Borealis |
| **Módulo: Cheats** | `TitleService`, `BuildIdResolver`, `CheatRepository`, `CheatStore` + telas Lista/Detalhe | libnx, rede, FS |
| **Compartilhado** | `I18n`, design system (tema, cores), componentes reutilizáveis (card, toggle, toast, estados vazios) | Borealis |

### Princípio de design

Separar **lógica pura** (testável no PC, sem console) da **borda dependente do libnx**
(hardware). Acesso a `ns`, rede e filesystem ficam atrás de **interfaces**, permitindo
mocks nos testes e mantendo a regra de negócio desacoplada do hardware.

### Registro de módulos (extensibilidade)

Cada módulo se declara como:

```
Module {
  id:        string         // "cheats"
  title:     i18n key        // "module.cheats.title"
  icon:      asset
  color:     theme color
  enabled:   bool            // false => card "Em breve" desativado
  entry:     () -> Activity  // tela de entrada do módulo
}
```

A home renderiza um card por módulo registrado. Adicionar um módulo futuro = registrar
mais uma entrada e escrever sua `Activity`, **sem alterar a home**.

### Módulos detalhados (módulo Cheats)

- **`TitleService`** — lista jogos instalados (id, nome, ícone, versão) via serviço `ns`
  (`nsListApplicationRecord` + `nsGetApplicationControlData`). Cacheia ícones/metadados
  (o `nsGetApplicationControlData` pode levar ~500 ms/jogo em HOS 20.0.0+).
- **`BuildIdResolver`** — descobre o `build_id` do jogo instalado **sem abri-lo**.
  Estratégia primária: usar a **versão** do jogo (do `ns`) e o mapeamento
  **versão → build_id** mantido pela switch-cheats-db. ⚠️ Risco principal (ver §7).
- **`CheatRepository`** — cliente da switch-cheats-db. Baixa (via internet) os cheats de
  **um jogo específico** por vez (nunca a base inteira de ~59k cheats) e cacheia. Usa
  libcurl (`switch-curl`).
- **`CheatStore`** — lê/escreve/parseia os arquivos
  `/atmosphere/contents/<title_id>/cheats/<build_id>.txt`. Responsável por ligar/desligar
  cheats reescrevendo o arquivo. Lógica pura (parser/serializador) testável no PC.

---

## 5. Telas e fluxo

### ① Home (bento)

```
┌────────────────────────────────────────────┐
│  thomaz                              ⚙   ℹ   │
│  O que vamos fazer hoje?                      │
│  ┌────────────────────────┐ ┌─────────────┐ │
│  │  🎯                      │ │  ➕          │ │
│  │  TRAPAÇAS                │ │  Em breve    │ │
│  │  Gerenciar cheats        │ │ (desativado) │ │
│  │  dos seus jogos          │ │             │ │
│  └────────────────────────┘ └─────────────┘ │
│  ┌─────────────┐ ┌─────────────┐             │
│  │ ➕ Em breve  │ │ ➕ Em breve  │             │
│  │ (desativado) │ │ (desativado) │             │
│  └─────────────┘ └─────────────┘             │
└────────────────────────────────────────────┘
```

### ② Lista de jogos (entrada do módulo Trapaças)

```
┌──────────────────────────────────────┐
│  Meus Jogos          🔍 [buscar]  ⚙   │
├──────────────────────────────────────┤
│  [🎮] Zelda: TOTK         ● 2 ativos  │
│  [🎮] Super Mario Wonder  ⬇ disponível│
│  [🎮] Metroid Dread       — sem cheats │
│  [🎮] Hades                ⬇ disponível│
└──────────────────────────────────────┘
```

Ícones reais (cacheados, carregados *lazy*), busca, e um badge de estado por jogo:
`● N ativos` / `⬇ disponível` / `— sem cheats`.

### ③ Detalhe do jogo

```
┌──────────────────────────────────────┐
│ ◀  Zelda: TOTK      v1.2.1   ⬇ Baixar │
├──────────────────────────────────────┤
│  ☑  Moedas infinitas                   │
│  ☑  Vida infinita                      │
│  ☐  Stamina infinita                   │
│  ☐  Andar rápido                       │
│  ───────────────────────────────────  │
│  Alterações salvam automaticamente ✓   │
└──────────────────────────────────────┘
```

Lista de cheats com toggles. "Baixar" busca da switch-cheats-db. Ligar/desligar grava o
arquivo na hora (auto-save).

### ④ Configurações
- Idioma (PT-BR / EN)
- Caminho/detecção do CFW (Atmosphère)
- Aviso de risco de ban
- Sobre

### Fluxo de dados (jornada principal)

1. **Abrir** → `TitleService` lista jogos (rápido; ícones *lazy*).
2. **Tocar no jogo** → `BuildIdResolver` resolve o `build_id` → `CheatStore` lê os cheats
   já ativos no SD + `CheatRepository` checa o que há disponível online.
3. **Tocar "Baixar"** → baixa os cheats daquele jogo/versão → guarda o **conjunto
   completo** em cache local.
4. **Ligar/desligar toggle** → `CheatStore` reescreve o `.txt` contendo **só os cheats
   ligados** (+ master code obrigatório). Como o conjunto completo está em cache, alternar
   não exige novo download.

O arquivo no SD **é** o estado; o Atmosphère aplica no próximo boot do jogo. Por isso
nada precisa estar aberto durante a configuração.

---

## 6. Tratamento de erros e casos de borda

1. **Sem cheats para a versão instalada** → mensagem clara ("os cheats são da v1.2.0,
   você está na v1.2.1") e **não** grava arquivo incompatível.
2. **Sem internet** → app funciona offline com cache/SD; ações de download mostram
   "sem conexão".
3. **Master code** → cheats com código mestre `{...}` obrigatório: o `CheatStore` sempre
   o inclui ao reescrever o arquivo.
4. **Risco de ban** → aviso amigável de primeira vez ao ativar um cheat: cheats + jogo
   online em NAND real = risco de ban da Nintendo; sugere emuMMC/offline.
5. **CFW ausente/errado** → detecta a presença do Atmosphère; avisa se não encontrar a
   estrutura esperada.
6. **`nsGetApplicationControlData` lento** (até ~500 ms/jogo em HOS 20.0.0+) → cache de
   metadados/ícones; lista aparece rápido com carregamento progressivo.

### Modelo de "ligado = ativo"

O app grava no `.txt` **apenas os cheats ligados** (+ master code). Assim "presente no
arquivo = ativo no jogo", sem depender da configuração
`atmosphere!dmnt_cheats_enabled_by_default`. Comportamento simples e previsível.

---

## 7. Risco principal: resolver `build_id` sem abrir o jogo

O nome do arquivo de cheat **precisa** bater com o `build_id` do executável principal do
jogo (primeiros 8 bytes do build id, em hex), senão o Atmosphère ignora o arquivo.

**Decisão:** não tentar ler o build_id do conteúdo instalado (caminho complexo via
`ncm`/`ldr`). Em vez disso, obter a **versão** do jogo (trivial via `ns`) e usar o
mapeamento **versão → build_id** que a switch-cheats-db mantém — abordagem já usada pelo
AIO-switch-updater.

**Ação:** validar essa abordagem com um **spike** no início da implementação (ler o
formato real do índice da switch-cheats-db e confirmar o mapeamento versão→build_id). É o
"vai ou não vai" técnico do projeto. Plano B, se o mapeamento da base for insuficiente:
ler o build_id localmente do conteúdo instalado (escopo maior, avaliado só se necessário).

---

## 8. Estratégia de testes

- **Testes unitários no PC (sem console):** lógica pura — parser/serializador do formato
  de cheat, lógica de ligar/desligar do `CheatStore`, matching versão→build_id, parsing do
  índice da base, `I18n`. **TDD** aqui.
- **Seams por interface:** `TitleService`, `CheatRepository` (rede) e o filesystem ficam
  atrás de interfaces → mockáveis nos testes.
- **Integração manual:** listagem real (`ns`), download e **aplicação** do cheat só são
  100% validáveis em **hardware real com Atmosphère**. O Ryujinx ajuda com build/UI, mas
  não é confiável para aplicação de cheat. Limitação explícita.

---

## 9. Stack técnico

- **Toolchain:** devkitPro + devkitA64, libnx (via `dkp-pacman -S switch-dev`).
- **UI:** Borealis (natinusala/borealis) — toque + controle + i18n + scroll prontos.
- **Rede:** libcurl (`switch-curl`) para baixar da switch-cheats-db.
- **Fonte de dados:** [switch-cheats-db](https://github.com/HamletDuFromage/switch-cheats-db)
  (open-source, releases diárias, organizada por title_id + build_id).
- **Formato de cheat:** formato de código do cheat VM do Atmosphère
  (`Atmosphere/docs/features/cheats.md`).
- **Build:** `.nro` via Makefile padrão do devkitPro.
- **Referências de estudo:** AIO-switch-updater (pipeline de dados + uso do Borealis),
  Breeze/EdiZon-SE (UX de seleção de cheat).

---

## 10. Aspectos legais/éticos

Homebrew (software próprio em hardware próprio) é legítimo; o app lida apenas com códigos
de cheat (texto), não com conteúdo pirateado. O uso de CFW com jogo **online** em NAND
real carrega risco de ban da Nintendo — o app exibe aviso e sugere a prática da comunidade
(emuMMC, conta/console separado, offline).
