# Validação — SaveNX 0.3.0

## Verificação automatizada

`python3 tools/run_host_tests.py` compila e executa três testes C++ sem SDK do Switch:

- caminhos de ZIP, incluindo rejeição de traversal;
- fila persistente, snapshot independente, recuperação após reinício, publicação interrompida, confirmação e preservação do original;
- limites de resposta do upload (200/201, ID e tamanho), códigos transitórios e origem HTTPS da sessão;
- texto UTF-8 e neutralização dos marcadores de cor da interface;
- escrita e recuperação do arquivo de sessão.

O workflow compila o NRO com as bibliotecas Switch e verifica a assinatura do container ao empacotar. Compilação e testes host não substituem testes no console.

## Roteiro no console

1. Instalação limpa: renomeie a pasta atual para preservar os backups, copie o novo pacote e abra sem arquivos de configuração anteriores.
2. Confirme que a tela inicial aparece sem consultar jogos ou iniciar OAuth. Abra Jogos e perfis e valide a lista.
3. Faça backup de um save descartável, sem conexão. Confirme o ZIP local e a pendência, feche e reabra o aplicativo.
4. Configure e autorize o cliente Google. Verifique o envio da pendência e o ZIP no Drive, sob os IDs corretos.
5. Interrompa a rede durante um upload; confirme a preservação do original e da fila, reconecte e reenvie.
6. Restaure o backup de teste; confira o backup de segurança e abra o jogo para verificar o conteúdo.
7. Repita em outro perfil/console para testar isolamento. Esta versão rejeita restauração para uma identidade de container diferente; migração entre perfis não é um requisito já validado.

Nenhum teste real no Nintendo Switch ou na conta Google é marcado como concluído apenas por passar na compilação.
