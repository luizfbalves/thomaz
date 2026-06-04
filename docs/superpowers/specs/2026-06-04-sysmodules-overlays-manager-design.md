# Gerenciador de Sysmodules e Overlays ("Sistema") — Design

**Data:** 2026-06-04
**Status:** Aprovado para planejamento
**Origem:** Pesquisa na comunidade de Switch CFW (GBAtemp, GitHub, ConsoleMods) apontou como dor de QoL nº 1 o gerenciamento manual de sysmodules e overlays — baixar releases do GitHub, extrair `.zip` na raiz do SD, editar arquivos de config e reiniciar, além de instalar um overlay Tesla separado só para ligar/desligar módulos.

## Objetivo

Adicionar ao thomaz um card **"Sistema"** que transforma o fluxo manual de gerenciar sysmodules e overlays em **"abrir uma lista, tocar num botão, reiniciar"**. É qualidade de vida para usuários de CFW (Atmosphère); não facilita pirataria.

### O que o usuário consegue fazer

- **Ligar/desligar** sysmodules já instalados (sys-clk, MissionControl, sys-ftpd, emuiibo, etc.) com um toque, sem editar arquivos nem instalar um overlay Tesla separado.
- **Instalar** sysmodules e overlays novos a partir de um catálogo curado — o thomaz baixa, valida, extrai e posiciona os arquivos sozinho.
- **Atualizar** itens instalados quando o catálogo traz uma versão mais nova.
- **Reiniciar** o console direto do app quando uma mudança exige reboot.

## Decisões travadas (brainstorming)

- **Escopo:** visão completa (toggle + instalar + overlays), implementada em **3 fases incrementais**.
- **Arquitetura:** espelha o módulo de mods existente (`core/` puro + `platform/` IO + `app/` activities). Card no hub nomeado **"Sistema"**.
- **Catálogo:** índice **curado pela API thomaz** (não GitHub direto, não índice de terceiros).
- **Reboot:** **avisar + oferecer reiniciar** via `reboot_to_payload`, com fallback para "reinicie manualmente".

## Arquitetura e organização de módulos

```
source/
├── core/sysmod/           # lógica pura, testável no host (doctest)
│   ├── sysmod_types.hpp       # Sysmodule, CatalogItem, enums (Kind, InstallState)
│   ├── toolbox_json.{hpp,cpp} # parse de toolbox.json (name, tid, requires_reboot)
│   ├── sysmod_scan.{hpp,cpp}  # listing de /atmosphere/contents -> vector<Sysmodule>
│   ├── sysmod_paths.{hpp,cpp} # contents_dir, flags/boot2.flag, .overlays dir
│   └── catalog_json.{hpp,cpp} # parse do índice curado da API
├── platform/sysmod/       # IO real (libnx/SD/HTTP/reboot)
│   ├── sysmod_store.{hpp,cpp}     # escanear SD, ler/criar/remover flag boot2 (toggle)
│   ├── sysmod_store_fake.{hpp,cpp}# lista fictícia para o build desktop
│   ├── sysmod_install.{hpp,cpp}   # baixar+validar+extrair do catálogo (Fases 2/3)
│   └── system_reboot.{hpp,cpp}    # reboot_to_payload com fallback
└── app/
    ├── system_activity.{hpp,cpp}        # tela "Sistema": lista, toggles, estado de reboot, catálogo
    └── system_detail_activity.{hpp,cpp} # detalhe/instalar de um item do catálogo (Fase 2/3)
```

**Princípio de fronteira:** toda lógica de "o que está instalado, o que ligar/desligar, o que o catálogo oferece" vive em `core/` e roda nos testes do host **sem Switch**. `platform/` só faz IO. No desktop, `sysmod_store_fake` devolve sysmodules fictícios (como o `FakeTitleService` faz hoje), permitindo iterar a UI sem hardware.

**Mecanismo de toggle:** um sysmodule é uma pasta `/atmosphere/contents/<ProgramID>/`. Está **ativo** quando existe `flags/boot2.flag` dentro dela; ligar = criar o flag, desligar = remover. O `toolbox.json` (quando presente) fornece nome amigável e `requires_reboot`. É o mesmo mecanismo do ovl-sysmodules, exposto numa tela cheia com UX melhor.

## Fases

### Fase 1 — Detecção + Toggle + Reboot (entregável sozinho)

Funciona **100% offline** — não depende da API.

**Fluxo de dados:**
1. `platform/sysmod_store` lista as pastas em `/atmosphere/contents/` e, para cada uma, lê `toolbox.json` (se houver) e checa a existência de `flags/boot2.flag`.
2. `core/sysmod_scan` (puro) transforma o listing em `vector<Sysmodule>` — nome amigável (do toolbox, ou ProgramID como fallback), `requires_reboot`, `enabled`.
3. `system_activity` renderiza a lista com toggles. Toque no toggle → `sysmod_store` cria/remove `flags/boot2.flag`.
4. Se algum item alterado tem `requires_reboot`, o estado vira "pendente de reboot" → banner + botão **"Reiniciar agora"** (`system_reboot`).

**Casos de borda:**
- Pasta sem `toolbox.json`: mostra ProgramID + marca "sem metadados".
- ProgramID inválido (nome de pasta não-hex): pulado/ignorado.
- `/atmosphere/contents` inexistente: lista vazia + dica.
- Sysmodule não-boot2 (sem mecanismo de flag): listado como "gerenciado externamente", read-only.
- Falha de escrita do flag (SD cheio/read-only): reverte o toggle na UI e avisa.

### Fase 2 — Instalar de catálogo

**Fluxo:**
1. `core/catalog_json` faz parse do índice curado servido pela API thomaz.
2. `system_activity` ganha uma seção "Catálogo" → lista o que dá para instalar, marcando o que já está instalado e se há **atualização** (compara `version`).
3. `system_detail_activity`: tela do item → botão Instalar. `platform/sysmod_install` baixa o asset (`http_client`), **valida `sha256`**, e extrai (libarchive, com zip-slip guard) para as pastas certas em `/atmosphere/contents/`.
4. Pós-install: o item aparece na lista da Fase 1, pronto para ligar. Aviso de reboot.

**Reaproveita:** `http_client`, `libarchive_extractor`, e o padrão de callback de progresso `(int, int)` do módulo de mods.

### Fase 3 — Overlays Tesla

Mesmo motor da Fase 2, com `kind: "overlay"` no catálogo. Diferenças:
- Instalar o **nx-ovlloader** (carregador) se ainda não estiver presente — detectado por arquivo conhecido na raiz do SD.
- `.ovl` vai para `sd:/switch/.overlays/` em vez de `/atmosphere/contents/`.
- Overlays não têm toggle boot2 — são listados como "instalado / atualizável", sem liga/desliga.

## Contrato do catálogo (API thomaz)

Novo endpoint versionável, ex. `GET /catalog/system`:

```json
{
  "version": 1,
  "items": [
    {
      "id": "sys-clk",
      "kind": "sysmodule",
      "name": "sys-clk",
      "description_en": "Per-game CPU/GPU/memory overclock.",
      "description_pt": "Overclock de CPU/GPU/memória por jogo.",
      "program_id": "00FF0000636C6BFF",
      "version": "2.0.0",
      "asset_url": "https://.../sys-clk.zip",
      "sha256": "…",
      "requires_reboot": true,
      "min_ams": "1.7.0",
      "homepage": "https://github.com/retronx-team/sys-clk"
    }
  ]
}
```

- **`kind`** (`sysmodule` | `overlay`): um só endpoint serve as Fases 2 e 3.
- **`sha256`**: verifica integridade do download antes de extrair.
- **`description_en` / `description_pt`**: textos curados saem bilíngues da API (não cabem no i18n do app).
- **`program_id`**: casa o item do catálogo com o que está instalado (estado instalado/atualizável na Fase 1).
- **`min_ams`**: habilita aviso de incompatibilidade de versão do Atmosphère.
- O app **cacheia** o catálogo localmente (padrão do índice de cheats) e funciona offline com o último cache.

## Tratamento de erros

- **Sem internet:** catálogo usa cache; sem cache, mensagem clara. **Fase 1 funciona offline** (não depende da API).
- **Download/hash:** falha de rede ou `sha256` divergente → aborta antes de extrair, nada é gravado.
- **Extração:** zip-slip guard (reusado dos mods); falha → limpa o staging parcial.
- **Toggle:** falha de escrita → reverte o toggle na UI e avisa.
- **Reboot:** `reboot_to_payload` indisponível/falha → fallback "reinicie manualmente".
- **Catálogo malformado:** parse defensivo, item inválido é pulado (não derruba a lista).

## Testes (doctest no host, sem hardware)

- `toolbox_json`: parse válido, campos faltando, JSON quebrado, `requires_reboot` ausente (default).
- `sysmod_scan`: listing → lista correta; pasta sem toolbox; flag presente/ausente → `enabled`; ProgramID inválido.
- `catalog_json`: parse, `kind` desconhecido pulado, item incompleto pulado, casamento por `program_id`, comparação de `version` (instalado vs atualizável).
- `sysmod_paths`: caminhos corretos (contents, `flags/boot2.flag`, `.overlays`).
- `is_safe_archive_path`: reusa/estende os testes de zip-slip dos mods.
- IO real (`platform/`, reboot) fica fora do host — validado em hardware, coerente com o resto do projeto.

## i18n

- Strings de UI (botões, banners, títulos, estados) nas 5 locales existentes (`en-US`, `pt-BR`, `fr`, `ru`, `zh-Hans`); mínimo `en-US` + `pt-BR` completos, demais herdam EN se faltar.
- Descrições dos itens do catálogo vêm da API (`description_en` / `description_pt`), não do i18n.

## Riscos e itens a confirmar em hardware

- **`reboot_to_payload`:** confiabilidade varia conforme o setup (Hekate registrado). Tratado como caminho preferido com fallback; confirmar em hardware real.
- **Detecção de não-boot2 sysmodules:** garantir que itens sem mecanismo de flag sejam read-only e não quebrem a lista.
- **Segurança da instalação:** índice curado + `sha256` mitigam supply-chain; nenhuma escrita fora de `/atmosphere/contents/` e `sd:/switch/.overlays/`.
- **Detecção do nx-ovlloader:** definir o arquivo-âncora exato que indica presença do carregador (Fase 3).

## Fora de escopo (YAGNI)

- Edição de `config.ini` por título (ex.: presets de overclock do sys-clk) — é a feature separada de "overclock por título"; este design só liga/desliga e instala o módulo.
- Criação/edição de sysmodules.
- Catálogo editável pelo usuário ou índices de terceiros.
