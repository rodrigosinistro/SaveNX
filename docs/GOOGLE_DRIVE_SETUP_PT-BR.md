# Configuração privada do Google Drive

O SaveNX usa o fluxo OAuth para TVs e dispositivos com entrada limitada. A senha da conta Google é digitada somente em uma página oficial do Google no celular ou computador. Na `v0.1.1`, os testadores não precisam criar projetos, baixar JSON ou digitar o Gmail no Switch.

Esta configuração é feita uma única vez pelo mantenedor do build privado.

## 1. Criar o projeto

1. Acesse o [Google Cloud Console](https://console.cloud.google.com/).
2. Crie um projeto dedicado, por exemplo `SaveNX`.
3. Em **APIs e serviços**, habilite a **Google Drive API**.
4. Configure a tela de consentimento do Google Auth Platform.
5. Mantenha o projeto em modo de teste.
6. Adicione sua conta Google e as contas autorizadas à lista de usuários de teste.

## 2. Criar o cliente OAuth

1. Abra **APIs e serviços > Credenciais**.
2. Escolha **Criar credenciais > ID do cliente OAuth**.
3. Selecione o tipo **TVs and Limited Input devices** (TVs e dispositivos com entrada limitada).
4. Dê um nome como `SaveNX Switch`.
5. Abra o cliente criado e copie `client_id` e `client_secret`.

## 3. Configurar o build privado

No repositório GitHub, abra **Settings > Secrets and variables > Actions** e crie:

- `SAVENX_GOOGLE_CLIENT_ID`, contendo o `client_id`;
- `SAVENX_GOOGLE_CLIENT_SECRET`, contendo o `client_secret`.

O workflow gera um cabeçalho temporário durante a compilação. Nenhum dos dois valores é gravado no código-fonte ou no artefato como arquivo separado. Como todo aplicativo instalado em um dispositivo, o `.nro` não deve ser considerado capaz de esconder definitivamente seu próprio cliente OAuth.

Para uma compilação local privada, copie [`config/oauth_client.generated.example.hpp`](../config/oauth_client.generated.example.hpp) para `include/oauth_client.generated.hpp` e substitua os valores fictícios. O destino é ignorado pelo Git e nunca deve ser commitado.

## 4. Autorizar uma conta testadora

1. Copie o pacote para o SD e abra o SaveNX conectado à internet.
2. O aplicativo mostrará uma URL oficial do Google e um código temporário.
3. Abra a URL no celular ou computador.
4. Escolha uma conta presente na lista de testadores, digite o código e aceite o acesso solicitado.
5. Volte ao Switch. O SaveNX concluirá o login, mostrará a conta conectada e criará `SaveNX` na raiz do Meu Drive.

O estado da sessão será salvo somente dentro da pasta do aplicativo:

```text
sdmc:/switch/SaveNX/config/google-drive.json
```

Não publique esse arquivo: ele contém o `refresh_token` da conta autorizada. Em builds com cliente incorporado, o `client_id` e o `client_secret` não são duplicados nesse arquivo.

## Compatibilidade sem cliente incorporado

Se o build foi gerado sem os dois Actions secrets, baixe o JSON do cliente do tipo **TVs and Limited Input devices**, renomeie para `google-drive.json` e copie para o caminho acima. O formato esperado contém o objeto `installed`, com `client_id` e `client_secret`; consulte [`config/google-drive.example.json`](../config/google-drive.example.json).

Ao atualizar da `v0.1.0`, o antigo `sdmc:/config/SaveNX/client_secret.json` é migrado automaticamente e a autorização existente é preservada.

O escopo solicitado é:

```text
https://www.googleapis.com/auth/drive.file
```

Esse escopo permite que o aplicativo trabalhe com os arquivos que ele próprio cria ou que foram explicitamente abertos com ele; não é uma autorização genérica para ler todo o Drive.

## Solução de problemas

- **O código não aparece:** confirme a conexão de rede e se os dois Actions secrets estavam disponíveis no build. Em modo de compatibilidade, confira `google-drive.json`.
- **`access_denied` ou app bloqueado:** verifique a tela de consentimento e a lista de usuários de teste.
- **Pede login novamente depois de alguns dias:** projetos externos em modo de teste normalmente emitem refresh tokens com validade de sete dias. Para este estágio privado, basta reautorizar quando solicitado.
- **Pasta duplicada:** mantenha apenas uma pasta `SaveNX` na raiz do Meu Drive para evitar ambiguidade.
- **Falha HTTPS:** confira a data e hora do console. O SaveNX valida o certificado do servidor usando `romfs:/certs/cacert.pem`.

Referências oficiais:

- [OAuth 2.0 for TV and Limited-Input Device Applications](https://developers.google.com/identity/protocols/oauth2/limited-input-device)
- [Google Drive API scopes](https://developers.google.com/workspace/drive/api/guides/api-specific-auth)
- [Create and populate folders](https://developers.google.com/workspace/drive/api/guides/folder)
