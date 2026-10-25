SaveNX 0.4.2 — instalação

1. Feche o jogo e desligue o Switch antes de remover o cartão SD.
2. Extraia este ZIP. Copie a pasta switch para a raiz do SD, aceitando substituir somente o SaveNX.nro.
3. Mantenha suas pastas de backups e configurações anteriores. Não é necessário apagá-las.
4. Inicie o Homebrew Menu com memória completa (title override, mantendo R ao abrir um jogo) e abra SaveNX.

A versão 0.4.2 corrige os caminhos usados para abrir arquivos e pastas dos saves. Mantém os quadros grandes, a navegação com ambos os manches e a busca de saves da 0.4.1. Os backups desta versão ficam em /switch/SaveNX/backups-v2. Os backups antigos permanecem preservados, mas não são restaurados automaticamente pela nova versão.

Os números de saves por perfil são dados encontrados no console. Um backup é uma cópia ZIP concluída no cartão, que depois pode ser enviada ao Drive.

Primeiro backup:
- Meus jogos → escolha seu perfil → escolha o jogo.
- Pressione X para criar uma cópia local verificada.
- Google Drive → Conectar Google Drive → abra no celular o endereço mostrado, digite o código e autorize a conta.
- Enviar pendentes envia as cópias que aguardam conexão. A cópia local permanece no SD.

Não é necessário informar ID OAuth nem senha Google no Switch. O cadastro técnico do aplicativo deve vir preparado no pacote. Se o Google recusar a autorização, mantenha o backup local e informe a mensagem exibida.

Controles:
Direcional ou qualquer manche: navegar. A: escolher. B: voltar ou cancelar uma operação que permita cancelamento. +: sair.
X nos jogos ou backups: criar backup. X na seleção de perfis ou em uma lista de jogos vazia: atualizar a busca.
Para restaurar, escolha um backup, selecione Restaurar e segure A por 1,5 segundo. O app verifica o arquivo e salva o estado atual antes de gravar. Durante a gravação, aguarde e mantenha o console ligado.

O app trabalha com saves já existentes de jogos/perfis do mesmo console. Não cria perfis ou saves de jogos ainda não iniciados. Os backups na nuvem ficam no seu Google Drive pessoal em SaveNX/Backups-v2.

A conexão Google validada na versão 0.4.0 foi mantida. Não apague a pasta config: ela guarda sua autorização. Os backups da versão 0.4.0 continuam compatíveis.

Se uma operação falhar, a tela informa um código FS e o registro técnico inclui o caminho e a operação. Preserve /switch/SaveNX/logs/savenx.log para diagnóstico.

Se ainda não aparecerem saves, selecione Todos os saves e atualize a lista com X na tela de perfis. O app registra a consulta em /switch/SaveNX/logs/save-discovery.json e /switch/SaveNX/logs/savenx.log. Esses arquivos não contêm o token Google.

Esta versão deve ser validada no seu Switch. O pacote foi compilado para libnx e inclui o certificado necessário à conexão HTTPS; não depende de uma instalação anterior do JKSV. A autorização Google e o comportamento no hardware exigem teste no console real.
