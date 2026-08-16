# Arquitetura

## Componentes

| Área | Responsabilidade |
|---|---|
| `data::User` | Lê os perfis e deriva uma chave estável a partir do AccountUid completo. |
| `data::TitleInfo` | Resolve Title ID, nome, ícone e tamanhos esperados do save. |
| `fs::ScopedSaveMount` | Monta e desmonta o container pelo `libnx`. |
| `fs::MiniZip` / `MiniUnzip` | Empacota todos os arquivos, verifica entradas e restaura o ZIP. |
| `fs::SaveMetaData` | Captura e valida identidade, slot e dimensionamento do container. |
| `fs::BackupManifest` | Grava `savenx_manifest.json` para auditoria e compatibilidade futura. |
| `remote::GoogleDrive` | Executa OAuth Device Flow e operações da Drive API via HTTPS. |
| `remote::enter_backup_directory` | Garante o escopo `SaveNX/<usuário>/<título>` antes de listar ou enviar. |
| `migration::migrate_v0_1_0_layout` | Move dados antigos para a pasta unificada sem sobrescrever destinos. |

## Fluxo de backup em nuvem

1. O usuário e o título selecionados fornecem AccountUid e Title ID.
2. O storage retorna à raiz lógica `SaveNX` e entra/cria as pastas estáveis.
3. O save é montado apenas durante a leitura.
4. Um ZIP temporário em `sdmc:/switch/SaveNX/temp/` recebe manifesto, metadata binário e todos os arquivos do save.
5. Qualquer falha de mount, leitura, escrita ou fechamento invalida e remove o ZIP parcial.
6. O arquivo completo é enviado por uma sessão resumable da Drive API.
7. O temporário é apagado; se `Manter backups locais` estiver ativo, ele permanece na árvore local do mesmo usuário.

## Fluxo de restore

1. O arquivo remoto é baixado por ID para o cartão SD, com verificação HTTPS e tamanho final.
2. O ZIP inteiro passa pelo preflight de path e CRC.
3. O metadata é comparado ao `FsSaveDataInfo` atualmente selecionado.
4. Quando necessário, o container é estendido para os tamanhos do backup.
5. O save atual é copiado para um backup de segurança por padrão.
6. O conteúdo atual é apagado e o filesystem recebe commit.
7. Os arquivos do ZIP são gravados com commits intermediários conforme o tamanho do journal.

## Decisões de segurança

- OAuth no dispositivo: nenhuma senha é coletada pelo NRO.
- Cliente OAuth privado injetado no build; o repositório não contém credenciais reais.
- Configuração, cache, logs, temporários e backups locais ficam abaixo de `sdmc:/switch/SaveNX/`.
- Escopo `drive.file`: privilégio menor que acesso total ao Drive.
- Certificados validados e protocolo restrito a HTTPS, inclusive em redirecionamentos.
- Chaves remotas baseadas em IDs, evitando mistura por apelidos iguais ou renomeados.
- Restore fail-closed: backup sem metadata, corrompido ou de outro slot é recusado.
- Escrita em saves de sistema permanece opt-in.
- Atualização automática está desabilitada até existir um endpoint oficial do SaveNX.

## Origem do código

A base é a branch `rewrite` do JKSV no commit `d39fd80a208b7c802d12edb12f362506a0addb47`, mais FsLib `382c7d490a68e4897fdc7d1ba37ac6e49cd6ac0e` e SDLLib `9fa1be94b7c0393f479f47dc9a74908a38112fe8`. A licença GPLv3 e os avisos dos componentes são preservados.
