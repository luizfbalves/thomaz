# Extração de Layouts — Modo Aplicação via Title Takeover

Para extrair os layouts base do firmware, o thomaz precisa ser executado em **modo Aplicação**
(title takeover do hbloader). Em modo Applet (lançado pela Galeria/Álbum), os serviços
privilegiados FS/SPL não estão disponíveis e o app exibe o aviso de relançamento.

---

## Passos — Hold-R Title Override

1. **A partir do menu Home do Switch**, mantenha o botão **R** pressionado.
2. **Com o R ainda pressionado**, abra qualquer **JOGO instalado**
   — NÃO use o Álbum/Galeria. O jogo em si não importa; apenas precisa ser um título
   de aplicação (Application-type) instalado no console.
3. **Continue segurando R durante o logotipo da Nintendo.**
   O Homebrew Menu (hbloader) abre em **modo Aplicação**, com acesso completo à memória
   e permissões amplas de FS/SPL.
4. **No Homebrew Menu, abra o thomaz.**
5. **Execute a opção "Extrair layouts do firmware"** no thomaz.

O thomaz detecta o modo de execução automaticamente:

- **Modo Aplicação** (via hold-R): extração prossegue normalmente.
- **Modo Applet** (lançado pela Galeria): o app exibe um aviso pedindo o relançamento via
  title takeover e retorna sem tentar acessar nenhum serviço privilegiado
  (requisito TAKEOVER-01).

---

## Nota sobre `override_config.ini` (Suposição A4)

Normalmente, nenhuma alteração no `override_config.ini` do Atmosphère é necessária para o
hold-R padrão funcionar — o hbloader usa esse mecanismo por padrão para qualquer título.

Caso o seu `override_config.ini` bloqueie a substituição padrão de títulos (por exemplo,
com `override_any_app = false` ou restrições por título), pode ser necessário adicionar uma
entrada manual para o jogo que você usa como carrier. Esta etapa é configuração do lado do
usuário e está fora do escopo do thomaz.

**Status:** não testado em hardware ainda — a ser confirmado na primeira execução no console
(ver Proveniência abaixo).

---

## Proveniência (D-07)

Esta seção registra as fontes de chave SPL e a versão de firmware validadas pelo spike de
hardware da Fase 1.

### Fontes de chave SPL — Atmosphère

| Campo            | Valor                                                                 |
|------------------|-----------------------------------------------------------------------|
| Projeto          | Atmosphère-NX (SciresM et al.)                                        |
| Release          | **1.7.1**                                                             |
| Commit           | **b39e29d**                                                           |
| URL da release   | https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.7.1       |
| Tipo de dado     | Fontes de chave **PÚBLICAS** (`header_kek_source`, `header_key_source`, `key_area_key_application_source`) copiadas da configuração criptográfica do FS do Nintendo firmware e espelhadas publicamente pelo Atmosphère. Não são chaves secretas. |
| Arquivo no código | `source/platform/themes/key_loader_switch.cpp` linhas 17-42         |
| Por que públicas | O SPL (Secure Partition Layer) do hardware usa essas fontes públicas junto com as key slots internas do console para derivar a chave de cabeçalho NCA por console — sem `prod.keys` necessários (EXTRACT-04). |

### Versão de firmware validada

| Campo | Valor |
|-------|-------|
| Firmware (major.minor.micro) | **Pendente — a registrar após a primeira execução em hardware (`setsysGetFirmwareVersion`)** |
| Como será capturado | O código em `firmware_extract_switch.cpp` chama `setsysGetFirmwareVersion(&fw)` e imprime `fw.major.minor.micro` via `printf` durante a extração. Registre o valor aqui e em `THIRD_PARTY.md` após o primeiro teste no console. |

---

## Referências Técnicas

- Mecanismo de title takeover do hbloader: [GBAtemp — What is title override?](https://gbatemp.net/threads/what-in-the-nine-hells-is-title-override.605707/)
- Documentação NCA e fontes de chave: [switchbrew NCA wiki](https://switchbrew.org/wiki/NCA)
- Código de derivação de chave SPL: `source/platform/themes/key_loader_switch.cpp` (exelix fork @ `2618b0c`, portado fielmente)
- Detecção de modo applet: `appletGetAppletType() != AppletType_Application` em `firmware_extract_switch.cpp`
