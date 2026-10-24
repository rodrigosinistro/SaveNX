# SaveNX 0.4.0

Aplicativo homebrew original para backup e restauração de saves do Nintendo Switch, com cópias no cartão SD e no Google Drive pessoal.

Esta versão foi escrita novamente: interface, navegação, acesso aos saves, arquivos de backup e integração Google são implementações próprias. Não usa o código, as telas, as bibliotecas FsLib/SDLLib ou os assets do JKSV/NPShop. As dependências são bibliotecas públicas de uso geral e o SDK homebrew libnx.

## O que faz

- Lista os perfis do console e seus saves existentes, incluindo saves compartilhados de jogos.
- Cria cópias ZIP verificadas por CRC, separadas por perfil, jogo e espaço de save.
- Cria backups de todos os saves disponíveis a partir da tela inicial.
- Autoriza o Google pelo celular ou computador; o Switch mostra somente link e código.
- Envia cópias com uma fila persistente e confirmação do arquivo completo no Drive.
- Lista backups locais e da nuvem e permite restaurá-los no mesmo perfil e jogo.
- Valida integralmente o ZIP antes de gravar e cria uma cópia de segurança obrigatória do save atual.

Uma única cena controla toda a tela. O renderizador limpa o quadro completo e o recorte antes de desenhar. As tarefas de disco e rede não desenham nem alteram o estado visual.

## Instalação e controles

Veja [INSTALL.md](INSTALL.md). O pacote de instalação inclui `switch/SaveNX/SaveNX.nro`; copie a pasta `switch` para a raiz do SD. Abra pelo Homebrew Menu com memória completa (title override). Feche o jogo antes de usar seus saves.

- Direcional: navegar; A: escolher; B: voltar/cancelar; +: sair.
- X na lista de jogos ou backups: novo backup.
- Para restaurar, selecione a segunda opção e segure A por 1,5 segundo.

## Google Drive

O usuário escolhe **Google Drive → Conectar Google Drive**, abre o endereço oficial exibido e autoriza o código. O app nunca pede a senha Google nem um ID OAuth na interface.

O cadastro OAuth pertence ao aplicativo e precisa estar incluído no pacote pelo mantenedor. Para compilação própria, use um cliente Google do tipo **TVs and Limited Input devices**, com Drive API e escopo `drive.file`. O workflow pode recebê-lo pelos secrets `SAVENX_GOOGLE_CLIENT_ID` e `SAVENX_GOOGLE_CLIENT_SECRET`. Sem essa configuração, as operações locais funcionam e a conexão informa que o pacote ainda não está preparado. Nenhum cadastro pessoal é publicado neste repositório.

Os backups usam `SaveNX/Backups-v2/PERFIL/TITLE_ID/ESPAÇO` na conta Drive autorizada. A fila no SD só é removida depois da confirmação de ID e tamanho do arquivo remoto. A cópia local permanece.

## Dados e compatibilidade

- Backups novos: `/switch/SaveNX/backups-v2/`.
- Fila de envio: `/switch/SaveNX/queue-v2/`.
- Sessão Google: `/switch/SaveNX/config/cloud-session.json`.
- Ajustes: `/switch/SaveNX/config/settings-v2.json`.
- Registro técnico sem tokens: `/switch/SaveNX/logs/savenx.log`.

As pastas das versões anteriores permanecem no SD. Esta versão usa um formato próprio e restaura somente seus novos backups; não converte backups de outros aplicativos. Pode reaproveitar uma sessão anterior do SaveNX se o cadastro Google coincidir. Proteja o SD, que contém seus backups e o token da conta autorizada.

A proteção automática ocorre quando você abre o SaveNX e escolhe fazer backup, e o envio automático ocorre após o backup quando ativado. Não existe um módulo residente fazendo cópias durante o jogo. Nenhum save de sistema/NAND é incluído.

## Compilar e validar

Com devkitA64/libnx e portlibs Switch instaladas:

```sh
python3 tools/configure_google.py
mkdir -p romfs/certs
cp /etc/ssl/certs/ca-certificates.crt romfs/certs/cacert.pem
make -j2
python3 tools/package_switch.py
```

A configuração padrão não contém credenciais. O arquivo JSON da aplicação reserva um slot RomFS de 4096 bytes, também configurável pelo mantenedor durante o empacotamento. Esse slot não contém tokens de usuário. `tools/configure_google.py` valida a marca e preserva o tamanho de um NRO ao preparar uma distribuição privada.

Testes de host:

```sh
# Para os testes completos de ZIP no Linux: libminizip-dev e libjson-c-dev
python3 tools/run_host_tests.py
```

Os testes exercitam caminhos de arquivos, isolamento de perfil/jogo/espaço, CRC e preflight dos ZIPs, confirmação de upload, navegação, limites de todas as telas e configuração do pacote. GitHub Actions compila o NRO e gera um ZIP instalável. Compilação e testes de host não substituem o teste no console real ou a autorização da conta Google.

## Licença

Código original sob MIT. Veja [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
