# Configuração do Google Drive

O SaveNX usa o fluxo OAuth para TVs e dispositivos com entrada limitada. A senha da conta Google é digitada somente em uma página oficial do Google no celular ou computador.

## 1. Criar o projeto

1. Acesse o [Google Cloud Console](https://console.cloud.google.com/).
2. Crie um projeto dedicado, por exemplo `SaveNX`.
3. Em **APIs e serviços**, habilite a **Google Drive API**.
4. Configure a tela de consentimento do Google Auth Platform.
5. Se o projeto estiver em modo de teste, adicione a sua conta Google à lista de usuários de teste.

## 2. Criar o cliente OAuth

1. Abra **APIs e serviços > Credenciais**.
2. Escolha **Criar credenciais > ID do cliente OAuth**.
3. Selecione o tipo **TVs and Limited Input devices** (TVs e dispositivos com entrada limitada).
4. Dê um nome como `SaveNX Switch`.
5. Baixe o JSON das credenciais.

O SaveNX espera o formato `installed`, com pelo menos `client_id` e `client_secret`. Compare com [`config/client_secret.example.json`](../config/client_secret.example.json).

## 3. Copiar para o cartão SD

Renomeie o arquivo baixado para `client_secret.json` e coloque exatamente em:

```text
sdmc:/config/SaveNX/client_secret.json
```

Não coloque o exemplo fictício nessa pasta e não publique o arquivo real no GitHub.

## 4. Autorizar a conta

1. Conecte o Switch à internet e abra o SaveNX.
2. O aplicativo mostrará uma URL oficial do Google e um código temporário.
3. Abra a URL no celular ou computador.
4. Entre na conta Google desejada, digite o código e aceite o acesso solicitado.
5. Volte ao Switch. O SaveNX concluirá o login, criará `SaveNX` na raiz do Meu Drive se necessário e salvará o `refresh_token` no mesmo JSON do cartão SD.

O escopo solicitado é:

```text
https://www.googleapis.com/auth/drive.file
```

Esse escopo permite que o aplicativo trabalhe com os arquivos que ele próprio cria ou que foram explicitamente abertos com ele; não é uma autorização genérica para ler todo o Drive.

## Solução de problemas

- **O código não aparece:** confirme o caminho e o nome do JSON, a conexão de rede e se `client_id`/`client_secret` estão dentro de `installed`.
- **`access_denied` ou app bloqueado:** verifique a tela de consentimento e a lista de usuários de teste.
- **Pede login novamente depois de alguns dias:** projetos externos em modo de teste podem ter refresh tokens de curta duração. Reautorize ou ajuste o status de publicação conforme as regras do Google.
- **Pasta duplicada:** mantenha apenas uma pasta `SaveNX` na raiz do Meu Drive para evitar ambiguidade.
- **Falha HTTPS:** confira a data e hora do console. O SaveNX valida o certificado do servidor usando `romfs:/certs/cacert.pem`.

Referências oficiais:

- [OAuth 2.0 for TV and Limited-Input Device Applications](https://developers.google.com/identity/protocols/oauth2/limited-input-device)
- [Google Drive API scopes](https://developers.google.com/workspace/drive/api/guides/api-specific-auth)
- [Create and populate folders](https://developers.google.com/workspace/drive/api/guides/folder)
