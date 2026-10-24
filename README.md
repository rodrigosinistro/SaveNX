# SaveNX

Homebrew para Nintendo Switch com Atmosphère que mantém backups locais de saves e envia cópias diretamente ao Google Drive do usuário.

**Versão 0.3.0 — candidata a testes.** O código-fonte foi recuperado do pacote SaveNX v0.1.0. O pacote instalável v0.2.17 foi preservado como referência, mas não contém fontes; esta versão não deve ser tratada como uma recompilação dele.

## Instalação

1. Baixe o pacote `SaveNX-v0.3.0-switch.zip` no artefato da compilação em Actions.
2. Extraia a pasta `switch` na raiz do cartão SD. O executável fica em `sdmc:/switch/SaveNX/SaveNX.nro`.
3. Abra o Homebrew Menu com memória completa, segurando **R** ao iniciar um jogo.
4. O SaveNX abre na tela inicial sem consultar saves e sem iniciar login. Escolha **Jogos e perfis** para carregar os dados.
5. Feche o jogo antes de criar ou restaurar seu backup.

A instalação limpa cria suas próprias pastas e configurações. Backups, fila, logs, cache e autorização ficam sob `/switch/SaveNX`. Não é necessário ter JSON, token ou cache de uma instalação anterior para abrir ou usar backups locais.

## Google Drive

Selecione **Conectar Google Drive**, abra o endereço oficial exibido no celular ou computador e informe o código temporário. **B** cancela a autorização.

O mantenedor pode incorporar seu cliente OAuth no build usando os Actions secrets `SAVENX_GOOGLE_CLIENT_ID` e `SAVENX_GOOGLE_CLIENT_SECRET`. Builds sem cliente incorporado continuam funcionando localmente e oferecem **Configurar cliente Google** para informar o ID e o client secret no próprio Switch. O modo de compatibilidade por JSON está descrito em [Configuração Google](docs/GOOGLE_DRIVE_SETUP_PT-BR.md).

A senha da conta Google é digitada exclusivamente no site oficial do Google. O NRO usa o fluxo para **TVs and Limited Input devices**, com escopo `drive.file` e renovação de sessão. Um cliente OAuth distribuído dentro de um aplicativo não deve ser considerado um segredo inacessível.

## Backups e envios

- O padrão cria ZIPs locais e mantém uma cópia independente na fila de envio. Upload automático inicia habilitado e respeita quando o usuário o desliga.
- Uma falha de internet preserva o ZIP e a pendência. Para reenviar, conecte o Drive ou escolha **Reenviar backups pendentes**.
- Os reenvios desta versão acontecem com o aplicativo aberto, ao conectar ou por comando. Não há serviço de sincronização em segundo plano.
- Cada upload usa uma sessão resumable nova por tentativa. São feitas até três tentativas em falhas transitórias. Uma propriedade privada do Drive identifica o trabalho para recuperar envios cuja resposta foi perdida.
- O trabalho só sai da fila após resposta concluída com ID e tamanho esperado. O ZIP local original permanece disponível.
- O remoto mantém `SaveNX/USER_<UID>/TITLE_<TITLE_ID>/...`. Tipos compartilhados usam identificadores próprios.

## Restauração

Selecione o perfil, jogo e backup. Use a ação de restaurar e confirme conforme a interface. O motor herdado da base JKSV valida identidade do container e caminhos/CRC do ZIP antes de escrever; o backup de segurança anterior à restauração permanece habilitado por padrão. Saves de sistema continuam protegidos por padrão.

Faça o primeiro teste com um jogo e save descartáveis. [Validação](docs/VALIDACAO.md) distingue os testes de computador dos testes ainda necessários no console.

## Compilação

O workflow [Build SaveNX](.github/workflows/build.yml) executa testes host e compila com devkitPro/devkitA64, libnx e bibliotecas Switch. Os fontes de FsLib e SDLLib acompanham o repositório, com os avisos de licença originais.

```bash
python3 tools/run_host_tests.py
python3 tools/generate_oauth_header.py
make -j2
python3 tools/package_switch.py
```

O build requer o toolchain e as portlibs listadas no workflow. A primeira linha, com g++, também pode ser executada sem Switch SDK.

## Origem e licença

Obra derivada da branch `rewrite` do [JKSV](https://github.com/J-D-K/JKSV), commit `d39fd80a208b7c802d12edb12f362506a0addb47`, recuperada do código SaveNX v0.1.0. Licença GNU GPLv3, com avisos em [NOTICE.md](NOTICE.md).

[NPShop](https://github.com/NPShopOrg/npshop) e [NPShopSwitch](https://github.com/NPShopOrg/NPShopSwitch) foram usados nesta retomada como referência de arquitetura. O SaveNX usa conexão direta ao Google, sem depender do servidor antigo desses projetos.

[Arquitetura](docs/ARCHITECTURE.md) · [Formato dos backups](docs/BACKUP_FORMAT.md) · [Mudanças](CHANGELOG.md)
