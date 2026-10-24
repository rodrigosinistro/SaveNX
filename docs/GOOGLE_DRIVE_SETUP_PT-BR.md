# Google Drive — configuração do SaveNX 0.3.0

## Preparação pelo mantenedor

1. Crie ou selecione um projeto dedicado no [Google Cloud Console](https://console.cloud.google.com/).
2. Habilite a **Google Drive API**.
3. Configure a tela de consentimento do Google Auth Platform. Se o app estiver em teste, cadastre as contas testadoras.
4. Crie um cliente OAuth do tipo **TVs and Limited Input devices**. Um cliente Web ou Desktop não substitui esse tipo no Device Flow.
5. No GitHub, em **Settings → Secrets and variables → Actions**, configure `SAVENX_GOOGLE_CLIENT_ID` e `SAVENX_GOOGLE_CLIENT_SECRET`.
6. Execute a compilação. O build incorpora o cliente no NRO sem gravá-lo nos fontes ou no log.

O NRO não esconde definitivamente um cliente que precisa usar localmente. Nunca incorpore refresh tokens de usuários no build e nunca publique o arquivo de sessão.

## Sem cliente incorporado

O aplicativo ainda abre e permite backup/restauração locais. Escolha **Configurar cliente Google** e informe o ID e o client secret OAuth; eles não são o e-mail ou a senha da conta Google. Uma alternativa é copiar o JSON do cliente para `sdmc:/switch/SaveNX/config/google-drive.json`, seguindo `config/google-drive.example.json`.

## Autorizar uma conta

1. No SaveNX, escolha **Conectar Google Drive**.
2. Abra a URL oficial exibida no celular ou computador.
3. Informe o código temporário e aceite o acesso solicitado.
4. Volte ao Switch e aguarde a confirmação. **B** cancela o fluxo.

A sessão fica em `sdmc:/switch/SaveNX/config/google-drive.json`. A substituição usa arquivo temporário e uma cópia anterior recuperável para sistemas em que rename não sobrescreve o destino. A instalação limpa inicia desconectada; não depende de um token prévio.

O escopo é `https://www.googleapis.com/auth/drive.file`: arquivos criados pelo app ou compartilhados explicitamente com ele. Uma nova aplicação OAuth não recebe automaticamente acesso aos backups criados pelo NPShop. Esta versão não implementa importação direta do servidor ou das pastas antigas do NPShop.

## Falhas comuns

- `invalid_client`: confira o tipo do cliente, o ID e o client secret.
- `access_denied`: confirme a conta na lista de testadores e a autorização na página Google.
- Código expirado: inicie novamente a conexão.
- Falha HTTPS: confirme data/hora e rede do console. A verificação dos certificados permanece ativa.
- Projetos externos em modo de teste podem exigir nova autorização após expiração de refresh tokens; a duração depende da configuração Google.
- Um upload interrompido permanece na fila. Escolha **Reenviar backups pendentes** com o Drive conectado.

Referências: [Device Flow](https://developers.google.com/identity/protocols/oauth2/limited-input-device), [escopos](https://developers.google.com/workspace/drive/api/guides/api-specific-auth), [uploads](https://developers.google.com/workspace/drive/api/guides/manage-uploads).
