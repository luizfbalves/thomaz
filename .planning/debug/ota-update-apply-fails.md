---
slug: ota-update-apply-fails
status: resolved
trigger: corrigir a atualizacao OTA — baixa o arquivo mas logo em seguida aparece "falha ao atualizar"
created: 2026-06-05
updated: 2026-06-05
---

# Debug Session: ota-update-apply-fails

## Symptoms

<!-- DATA_START -->
- **expected_behavior:** A atualização OTA aplica com sucesso e o app passa para a nova versão (auto-update via GitHub Releases).
- **actual_behavior:** O download ocorre (baixa o arquivo), mas logo em seguida a tela mostra "falha ao atualizar".
- **error_messages:** Apenas a mensagem genérica "falha ao atualizar". Nenhum código de erro ou log detalhado conhecido pelo usuário ainda. nxlink/stdout ainda não capturado.
- **timeline:** NUNCA funcionou. Desde que o self-update foi adicionado, sempre falhou ao aplicar — nenhuma atualização OTA já completou com sucesso. Forte sinal de bug sistemático (não-ambiental).
- **reproduction:** Abrir o app → Configurações/Buscar atualizações → app detecta nova versão → baixa → mostra "falha ao atualizar".
<!-- DATA_END -->

## Investigation Targets (orchestrator hint, not yet verified)

Suspeitos clássicos para "baixa mas falha ao aplicar", a confirmar com evidência:
1. **Redirect do GitHub Releases** — o asset responde 302 para `objects.githubusercontent.com`. Se o `http_client` não segue o redirect (ou perde host/range), o "download" grava o corpo de redirect/HTML em vez do `.nro`, e a validação/aplicação rejeita.
2. **Validação pós-download** em `source/platform/self_update.cpp` — Content-Length vs bytes gravados, magic/header do NRO, checksum.
3. **Escrita/troca do binário na SD** — caminho de destino, permissão, substituição do `.nro` em execução.
4. **Comparação de versão/hash** em `source/core/update.cpp` — semver/tag ou checksum que rejeita o arquivo recém-baixado.

Relevant code:
- `source/core/update.cpp` / `source/core/update.hpp` (lógica pura)
- `source/platform/self_update.cpp` / `source/platform/self_update.hpp` (download + apply, libnx)
- `source/platform/http_client.hpp` (HTTP/TLS, redirects)
- `tests/test_update.cpp` (testes existentes)

## Current Focus

- hypothesis: Switch FAT (Horizon OS RenameFile) does not support renaming over an existing file
- next_action: RESOLVED — fix applied
- expecting: OTA update succeeds after remove+rename

## Evidence

- timestamp: 2026-06-05
  source: code_review / git_log
  content: |
    The original OTA implementation (e66cb98) used client->get(url) via http_client_curl.cpp
    which has CURLOPT_TIMEOUT=30L. For a 7.3 MB NRO asset, this always times out. Fixed in 6c2bcaf.

- timestamp: 2026-06-05
  source: code_review
  content: |
    Current apply_downloaded_update flow (after 6c2bcaf fix):
    1. download_file(url, tmp, nullptr, err, cancelled) — streams to .tmp via mod_download.cpp
    2. stat check — rejects zero-byte files
    3. std::rename(tmp, target) — HERE IS THE BUG
    
    On Switch FAT (Horizon OS), RenameFile requires the destination to NOT exist.
    Since thomaz.nro always exists (it is the running executable), rename() returns
    an error every single time, causing systematic "falha ao atualizar".
    
    The err string was also a local lambda variable (never shared with onSync),
    so the real error was silently discarded — only the generic failure message appeared.

- timestamp: 2026-06-05
  source: github_api
  content: |
    Release asset: thomaz.nro, size=7,624,845 bytes (~7.3 MB)
    browser_download_url: https://github.com/luizfbalves/thomaz/releases/download/v0.5.1/thomaz.nro
    download_file uses FOLLOWLOCATION=1L — redirect correctly followed.
    TLS should work (same CA chain as api.github.com which works successfully).

## Eliminated

- TLS certificate failure: api.github.com works fine (user sees update dialog), same CA chain as objects.githubusercontent.com
- Redirect handling: CURLOPT_FOLLOWLOCATION=1L is set in mod_download.cpp
- Hard timeout: mod_download.cpp uses LOW_SPEED_LIMIT/TIME, not CURLOPT_TIMEOUT
- WR-06 Content-Length check: download_file returns true when transfer completes fully
- cancelled flag: only set when activity is destroyed (user navigating away)

## Resolution

- root_cause: |
    Switch FAT (Horizon OS `RenameFile`) does NOT support renaming over an existing
    destination file. `std::rename(tmp, target)` in `apply_downloaded_update` always
    fails with an error because `thomaz.nro` exists on the SD card as the running
    executable. The `err` string was also a local variable inside the worker lambda
    and was never propagated to the UI, so the user only saw the generic "falha ao
    atualizar" message with no diagnosis possible.

- fix: |
    Two changes in source/platform/self_update.cpp:
    1. Call std::remove(target) before std::rename — removes the existing NRO first
       so Horizon OS's RenameFile can succeed. ENOENT is tolerated (first install).
       The running NRO is already in RAM so the brief absence on SD is safe.
    2. Added brls::Logger::info/error calls throughout apply_downloaded_update
       so errors appear in nxlink output for future debugging.
    
    One change in source/app/settings_activity.cpp (installUpdate):
    - err is now a shared_ptr<string> captured by both the worker and onSync lambdas;
      the error detail is appended to the "falha ao atualizar" status label so the
      user and developer can see what went wrong.
