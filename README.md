# SaveNX

SaveNX é um homebrew para Nintendo Switch com Atmosphère que cria backups completos dos saves no cartão SD e no Google Drive. Os backups são separados pelo UID real de cada perfil do console e pelo Title ID do jogo, e podem ser restaurados pelo próprio aplicativo.

> Status: versão de desenvolvimento (`0.1.4`). Faça testes com um save sem valor antes de depender do aplicativo. Feche o jogo antes de copiar ou restaurar dados e mantenha ao menos uma cópia fora do console.

Consulte também o [estado de validação](docs/VALIDACAO.md), incluindo o que ainda exige o toolchain e um console real.

## O que esta versão faz

- Lê os containers de save disponíveis pelo `libnx`, incluindo saves de conta, dispositivo e outros tipos que o usuário habilitar.
- Cria ZIPs com todos os arquivos montados do save, metadados binários do container e um manifesto JSON legível.
- Cria automaticamente a pasta `SaveNX` no Google Drive.
- Organiza o Drive por UID de usuário e Title ID, sem depender do apelido do perfil ou do nome traduzido do jogo.
- Usa o OAuth 2.0 Device Authorization Grant: a autorização acontece no celular ou computador; o Switch nunca recebe a senha do Gmail.
- Em builds privados, incorpora o cliente OAuth do SaveNX e elimina a criação ou cópia manual de JSON pelos testadores.
- Solicita o escopo limitado `drive.file`, usado apenas para arquivos criados pelo aplicativo.
- Verifica TLS usando uma coleção de autoridades certificadoras, em vez de desabilitar a validação HTTPS.
- Antes do restore, verifica a identidade do usuário, jogo e slot, lê todo o ZIP para validar CRCs e rejeita caminhos inseguros.
- Mantém ativo, por padrão, o backup automático do save atual antes de restaurar outro.
- Mantém a escrita em saves de sistema/NAND desabilitada por padrão.

## Organização dos backups

No Google Drive:

```text
SaveNX/
└── USER_<UID de 128 bits>/
    └── TITLE_<Title ID de 64 bits>/
        └── <data e hora>.zip
```

No cartão SD:

```text
sdmc:/switch/SaveNX/
├── SaveNX.nro
├── backups/
│   └── USER_<UID de 128 bits>/
│       └── <nome seguro do jogo>/
│           └── <perfil - data e hora>.zip
├── cache/
├── config/
├── logs/
└── temp/
```

Saves compartilhados ou especiais usam chaves estáveis como `DEVICE`, `BCAT`, `CACHE` e `SYSTEM`. Consulte [o formato do backup](docs/BACKUP_FORMAT.md) para os arquivos internos e as regras de compatibilidade.

## Instalação

1. Compile `SaveNX.nro` ou baixe o artefato `SaveNX-switch` gerado pelo GitHub Actions.
2. Crie `sdmc:/switch/SaveNX/` no cartão SD.
3. Copie o arquivo para `sdmc:/switch/SaveNX/SaveNX.nro`.
4. Em um build privado oficial, nenhuma configuração OAuth precisa ser copiada para o SD.
5. Inicie o Homebrew Menu por *title takeover* (segure `R` ao abrir um jogo), não pelo modo applet do Álbum. Saves grandes precisam da memória completa.

Ao iniciar a `v0.1.1` ou posterior pela primeira vez, arquivos da `v0.1.0` são migrados sem sobrescrever arquivos existentes. A `v0.1.2` faz essa migração sem recursão, inclusive quando uma tentativa anterior deixou pastas parcialmente migradas:

- `sdmc:/SaveNX/` → `sdmc:/switch/SaveNX/backups/`;
- `sdmc:/config/SaveNX/` → as subpastas `config/`, `cache/` e `logs/`;
- ZIPs temporários da raiz → `sdmc:/switch/SaveNX/temp/`.

A `v0.1.4` usa a montagem `sdmc` padrão do libnx e inicializa o log antes dos demais serviços. Se o aplicativo não conseguir abrir, o motivo da inicialização fica registrado em `sdmc:/switch/SaveNX/logs/SaveNX.log`.

## Uso rápido

1. Abra o SaveNX. No primeiro acesso, abra no celular o endereço oficial exibido, digite o código temporário e escolha uma conta cadastrada como testadora.
2. Selecione o perfil do Switch.
3. Selecione o jogo.
4. Escolha `Novo Backup`.
5. Para enviar automaticamente, habilite `Upload automático para remoto`; ou selecione um ZIP local e pressione `ZR`.
6. Backups no Drive aparecem com o prefixo `[GD]` dentro do jogo e usuário corretos.
7. Para restaurar, selecione o backup, pressione `Y`, confirme e mantenha `A` pressionado quando solicitado.

O restore somente começa após o arquivo passar pela validação. Se o UID, Title ID, tipo, rank ou índice do slot não corresponder ao save selecionado, o conteúdo atual não é apagado.

## Google Drive e privacidade

No build privado, o cliente OAuth é incorporado no `.nro` durante o GitHub Actions e não aparece no repositório. Após o primeiro login, apenas o estado necessário para renovar a sessão é salvo em:

```text
sdmc:/switch/SaveNX/config/google-drive.json
```

Esse arquivo contém um `refresh_token`. Não publique, envie ou inclua esse arquivo em commits. Builds sem cliente incorporado ainda aceitam um JSON manual nesse mesmo caminho como modo de compatibilidade.

O manifesto de cada backup guarda o UID do perfil e identificadores do save para impedir restore cruzado. Esses dados ficam no ZIP sob controle da conta Google autorizada.

## Compilação

Pré-requisitos:

- devkitPro com `switch-dev`/devkitA64 e `libnx`;
- Python 3;
- `switch-bzip2`, `switch-curl`, `switch-freetype`, `switch-harfbuzz`, `switch-libjpeg-turbo`, `switch-libjson-c`, `switch-libpng`, `switch-libwebp`, `switch-sdl2`, `switch-sdl2_image`, `switch-tinyxml2` e `switch-zlib`.

Clone com submódulos e compile:

```bash
git clone --recurse-submodules https://github.com/rodrigosinistro/SaveNX.git
cd SaveNX
make -j
```

O resultado é `SaveNX.nro`. O workflow em [`.github/workflows/build.yml`](.github/workflows/build.yml) executa a mesma compilação em um container devkitPro e publica um artefato pronto para a árvore do SD.

Para gerar o build privado sem expor o cliente OAuth, o mantenedor configura estes dois *Actions secrets* no repositório:

- `SAVENX_GOOGLE_CLIENT_ID`;
- `SAVENX_GOOGLE_CLIENT_SECRET`.

Se os dois secrets estiverem ausentes, o projeto continua compilando, mas exige o modo de compatibilidade por JSON descrito em [Configuração do Google Drive](docs/GOOGLE_DRIVE_SETUP_PT-BR.md). O pacote inclui `config/google-drive.example.json` como referência.

O teste host da proteção contra path traversal não exige o toolchain do Switch:

```bash
g++ -std=c++23 -Iinclude tests/path_safety_test.cpp -o /tmp/savenx-path-test
/tmp/savenx-path-test
g++ -std=c++23 -Iinclude tests/app_layout_test.cpp -o /tmp/savenx-layout-test
/tmp/savenx-layout-test
```

## Base técnica e inspiração

O repositório público do [NPShopHomebrew](https://github.com/arthur-moebios/NPShopHomebrew) oferece documentação e binários, mas não expõe o código-fonte do aplicativo. Por isso ele foi usado apenas como referência conceitual para a experiência de nuvem.

SaveNX é uma obra derivada da branch `rewrite` do [JKSV](https://github.com/J-D-K/JKSV), commit `d39fd80a208b7c802d12edb12f362506a0addb47`. Essa base fornece a integração madura com os serviços de save do Switch, a interface e o suporte aos diferentes tipos de container. As mudanças específicas do SaveNX estão descritas em [Arquitetura](docs/ARCHITECTURE.md).

## Licença

O código é distribuído sob a GNU GPL v3, conforme a base JKSV. Consulte [LICENSE](LICENSE) e [NOTICE.md](NOTICE.md). FsLib e SDLLib conservam suas licenças e históricos nos respectivos subdiretórios.
