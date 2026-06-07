# thomaz

🌐 **Site:** [luizfbalves.github.io/thomaz](https://luizfbalves.github.io/thomaz)

**Hub de homebrew para Nintendo Switch** — um app com interface moderna que reúne, em um só lugar, **trapaças (cheats), mods, temas e backups de saves na nuvem** dos seus jogos. UI bonita e bilíngue (PT-BR / EN), pensada para **toque e controle**.

Cada recurso usa a melhor fonte open-source disponível: cheats vêm da [**switch-cheats-db**](https://github.com/HamletDuFromage/switch-cheats-db) (aplicados pelo [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) no boot), mods vêm do **GameBanana** e temas vêm do **Themezer**. Os backups de saves sincronizam com a API na nuvem do próprio projeto.

> ⚠️ **Status:** em desenvolvimento. O app compila e roda na Switch, com lógica testada, mas o fluxo completo **ainda não foi validado 100% em hardware real**. Use por sua conta e risco.

---

## ✨ Funcionalidades

### 🧩 Trapaças (cheats)
- **Lista os jogos instalados** com ícone, versão e badges: **tem cheat** (coberto pela switch-cheats-db) e **ativo** (já existe arquivo de cheats salvo).
- **Tela de trapaças por jogo** — baixa o cheat certo para o `build_id` da versão instalada, lista cada um como um **toggle** e salva os ativados na SD (aplicados pelo Atmosphère no próximo boot).
- **Limpar trapaças** — selecione um, vários ou todos os jogos e remova seus arquivos de cheat (com confirmação).

### 🎨 Temas
- Navegue e aplique **temas da nuvem via [Themezer](https://themezer.net/)** direto no console.
- Extração **on-device** dos layouts do firmware (engine vendorizada do [exelix11/SwitchThemeInjector](https://github.com/exelix11/SwitchThemeInjector) + hactool) para desbloquear a aplicação de temas **sem precisar fornecer suas próprias `prod.keys`**.

### 🔧 Mods
- Navegue e instale **mods do [GameBanana](https://gamebanana.com/)** por jogo, com **extração automática** dos pacotes baixados (zip / 7z / etc., via libarchive).

### ☁️ Saves (backup + nuvem)
- **Backup dos saves locais** e **sincronização na nuvem** com locking otimista, através da API HTTP do projeto. **Requer login** (a sincronização é por conta).

### ⚙️ Configurações
- **Idioma** — Automático (sistema), Português (Brasil) ou English.
- **Buscar atualizações** — auto-update do app via GitHub Releases.
- **Atualizar base de cheats** — re-baixa o índice da switch-cheats-db.

### 🧰 Recursos transversais
- **Conta / login** — tela de boot com entrada por login ou como convidado; o login é necessário para saves na nuvem.
- **Filtro de visibilidade de títulos** — oculta automaticamente forwarders que não são jogos, com overrides por título e toggle global.
- **UI bilíngue** PT-BR / EN, com traduções parciais em outros idiomas (fr, ru, zh) · 👆 toque + controle · 🎨 tema escuro com acento violeta.

---

## 📦 Requisitos

- Nintendo Switch com **Atmosphère** (CFW) e um menu de homebrew (hbmenu).
- Conexão com a internet no console (para baixar cheats, mods, temas, atualizações e sincronizar saves).
- Espaço no cartão SD.

---

## 🚀 Instalação

1. Baixe o **`thomaz.nro`** na página de [**Releases**](https://github.com/luizfbalves/thomaz/releases/latest).
2. Copie o arquivo para a pasta **`/switch/`** do cartão SD:
   ```
   sd:/switch/thomaz.nro
   ```
3. No Switch, abra o **Álbum** (segurando R para entrar no hbmenu) e inicie o **thomaz**.

> Atualizações futuras podem ser feitas pelo próprio app, em **Configurações → Buscar atualizações** (ele baixa o `.nro` mais recente e substitui o atual; basta reiniciar).

---

## 🕹️ Como usar

A home é um hub: cada card leva a um recurso. Abaixo, o fluxo de cada um.

### Trapaças (cheats)
1. Abra o **thomaz** com os jogos **fechados**.
2. Toque no card **Trapaças** → escolha um jogo na lista.
3. O app baixa as trapaças disponíveis e mostra os **toggles**. Ative o que quiser — as mudanças **salvam automaticamente** na SD.
4. **Feche o thomaz e abra o jogo.** O Atmosphère aplica os cheats salvos no boot.

Para **remover** cheats: home → Trapaças → **Limpar trapaças** → marque os jogos → **Limpar selecionados**.

#### Onde os cheats são gravados
```
sd:/atmosphere/contents/<TITLE_ID>/cheats/<BUILD_ID>.txt
```
Esse é o caminho padrão que o Atmosphère lê. Se um jogo não tem cheats para a versão exata instalada, o thomaz usa os da versão anterior mais próxima (e avisa na tela).

### Mods
Home → **Mods** → escolha um jogo → navegue pelos mods do GameBanana e instale; o thomaz baixa e extrai o pacote para você.

### Temas
Home → **Temas** → navegue pelos temas do Themezer e aplique direto no console (a extração dos layouts do firmware acontece on-device, sem `prod.keys`).

### Saves
Home → **Saves** (requer login) → faça backup dos saves locais e sincronize com a nuvem.

---

## 🛠️ Compilar do código-fonte

O projeto usa **CMake** + o fork mantido do **[Borealis](https://github.com/xfangfang/borealis)** (UI) sobre **libnx/devkitPro**.

### Nintendo Switch (.nro)
A forma recomendada é o script **`scripts/build-switch.sh`**. Por padrão ele compila dentro da imagem oficial **`devkitpro/devkita64`** (o mesmo caminho do CI via **GitHub Actions** — só precisa de Docker, nenhum `dkp-pacman`). Se você já tem o **devkitPro instalado localmente**, defina `DEVKITPRO` e ele compila nativo, sem Docker.

```bash
git clone --recursive https://github.com/luizfbalves/thomaz.git
cd thomaz
./scripts/build-switch.sh                          # via Docker (devkitpro/devkita64)
# ou, com devkitPro local:
DEVKITPRO=/opt/devkitpro ./scripts/build-switch.sh # build nativo
```

Saída: **`build_switch/thomaz.nro`**. (Pela CI, baixe o artefato `thomaz-nro` da execução.)

**Instalar a partir do build:** copie `build_switch/thomaz.nro` para o cartão SD em `/switch/thomaz.nro`, ou envie direto para o hbmenu de um Switch em CFW com `nxlink build_switch/thomaz.nro`.

### Verificação (dois gates de target único)
O milestone Switch-only se verifica por dois gates, ambos de um único alvo:

```bash
make -C tests test          # 1. doctest no host — lógica pura (compila o test double saves/fake_cloud_save_client.*)
./scripts/build-switch.sh   # 2. build da Switch — produz build_switch/thomaz.nro
```

Os dois verdes juntos são o fluxo de verificação do projeto (não há mais smoke run de desktop).

### Backend local (auth + saves na nuvem)

A API HTTP (Fastify + PostgreSQL) fica em [`api/`](api/) e cuida de **autenticação e dos saves na nuvem**. Requer Docker para o banco:

```bash
cd api
docker compose up -d
cp .env.example .env
npm install
npx prisma migrate deploy
npm run dev
```

Detalhes em [`api/README.md`](api/README.md). Contrato REST: [`docs/superpowers/specs/2026-06-03-thomaz-api.md`](docs/superpowers/specs/2026-06-03-thomaz-api.md).

---

## 🧱 Estrutura

```
source/
├── core/       # lógica pura, testável, por recurso:
│               #   cheat_* (parse, build_id, índice), mods/ (gamebanana),
│               #   themes/ (themezer), saves/ (sync) + update
├── platform/   # libnx/IO: listagem de títulos, HTTP/curl, gravação na SD,
│               #   settings, self-update, mods/ (extração libarchive),
│               #   themes/ (hactool/exelix + extração de firmware),
│               #   saves/ (backup + cliente da nuvem)
└── app/        # telas Borealis: home (hub), cheats, mods, temas, saves,
                #   configurações, boot/login
api/            # backend Fastify (auth + saves na nuvem)
resources/      # i18n (pt-BR/en-US completos) + XML das telas + ícone
tests/          # suíte host (doctest)
docs/           # specs de design e planos das fases
```

---

## 🗺️ Roadmap

- [x] Núcleo de cheats testado + build .nro verde
- [x] Hub bento, lista de jogos, ícones e badges
- [x] Trapaças com toggles + limpar (seleção múltipla)
- [x] Mods do GameBanana (browse + instalação + extração)
- [x] Temas do Themezer (browse + aplicação + extração on-device do firmware)
- [x] Saves na nuvem (backup + sincronização com locking otimista)
- [x] Configurações (idioma) + auto-update + atualizar base de cheats
- [x] v1.0 hardening (segurança da API, refatoração de concorrência)
- [x] v1.1 Switch-only (target desktop removido, árvore só-Switch)
- [ ] **Validação completa em hardware real** (listagem, gravação, aplicação pelo Atmosphère, sync de saves)
- [ ] Carregamento lazy de ícones para bibliotecas grandes
- [ ] Verificação TLS com `cacert.pem` embarcado
- [ ] PERF-01 — evitar dupla travessia na extração de arquivos
- [ ] PERF-02 — cache de `CloudStatus` para pular uploads desnecessários

---

## 🙏 Créditos

- [switch-cheats-db](https://github.com/HamletDuFromage/switch-cheats-db) — base de cheats.
- [Themezer](https://themezer.net/) — fonte dos temas.
- [exelix11/SwitchThemeInjector](https://github.com/exelix11/SwitchThemeInjector) — engine de aplicação de temas.
- [GameBanana](https://gamebanana.com/) — fonte dos mods.
- [Borealis (xfangfang)](https://github.com/xfangfang/borealis) — biblioteca de UI.
- [devkitPro](https://devkitpro.org/) / [libnx](https://github.com/switchbrew/libnx) — toolchain.
- [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) — CFW que aplica os cheats.

Inspirado por ferramentas como EdiZon-SE, Breeze e AIO-switch-updater — com o objetivo de uma interface mais moderna e amigável.

---

## ⚖️ Aviso

thomaz é uma ferramenta para uso pessoal em jogos que você possui. Cheats e mods podem causar comportamento inesperado; **nunca use trapaças em jogos online**, sob risco de banimento. Use por sua conta e risco.

---

## 📄 Licença

Distribuído sob a **GNU GPL v2** — veja [`LICENSE`](LICENSE) e
[`THIRD_PARTY.md`](THIRD_PARTY.md). Este projeto era MIT até a v0.4; passou a
GPLv2 ao incorporar a engine de temas do
[exelix11/SwitchThemeInjector](https://github.com/exelix11/SwitchThemeInjector).
