# Arquitetura — retomada 0.3.0

A base-fonte disponível é SaveNX v0.1.0 (derivada de JKSV rewrite). O ZIP v0.2.17 contém apenas instalação e documentação e foi usado para recuperar requisitos de estabilidade, não como fonte compilável.

## Execução

`SaveNX` prepara as pastas com o devoptab padrão do libnx, abre o log e carrega a interface. `HomeState` permite escolher carregar perfis/jogos, conectar o Google, reenviar pendências ou abrir configurações. Dados e OAuth não iniciam no boot.

O acesso aos saves usa `data::User`, `data::TitleInfo`, `fs::ScopedSaveMount` e os motores ZIP e metadata existentes. As tarefas de transferência bloqueiam a navegação até concluírem. O worker não altera a pilha de estados da interface.

## Nuvem

`remote::GoogleDrive` usa OAuth Device Flow, `drive.file`, TLS validado e sessões resumable da Drive API. O cliente OAuth pode ser incorporado no build ou configurado no aplicativo. Refresh tokens pertencem à conta autorizada e ficam exclusivamente no SD.

Um backup completo é salvo localmente e copiado para `pending_uploads`. Cada trabalho usa um ZIP imutável e um marcador versionado `.job`. O marcador só é publicado depois de gravar o snapshot. A fila é independente da lista de jogos, portanto pode ser processada antes de enumerar saves.

Ao conectar ou reenviar, a fila entra nas pastas USER/TITLE e envia cada snapshot. Uma propriedade `appProperties.savenxJob` permite localizar um envio concluído se a resposta final se perder. Falhas de rede/408/429/5xx podem tentar novamente com uma sessão nova. A confirmação exige HTTP 200/201, ID e tamanho esperado. Só então a pendência é retirada; o backup local original permanece.

Envios desta versão acontecem com o aplicativo aberto. Nenhum sysmodule foi incorporado.

## Compatibilidade

O formato de backup é o da base SaveNX/JKSV descrito em BACKUP_FORMAT.md. O `.save_meta` do NPShop usa outro protocolo; importação desses arquivos exige um adaptador e não faz parte desta entrega. O código e as credenciais antigos do NPShop não são dependências de execução.

## Componentes incorporados

JKSV rewrite `d39fd80a208b7c802d12edb12f362506a0addb47`; FsLib `382c7d490a68e4897fdc7d1ba37ac6e49cd6ac0e`; SDLLib `9fa1be94b7c0393f479f47dc9a74908a38112fe8`. Os fontes dos dois últimos acompanham o repositório, sem submódulos necessários para compilar.
