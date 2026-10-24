# Segurança

O SaveNX usa o OAuth Device Flow do Google com escopo `drive.file`. A senha Google é informada apenas em uma página oficial do Google, fora do aplicativo.

A sessão fica em `sdmc:/switch/SaveNX/config/google-drive.json` e pode conter refresh token e cliente OAuth manual. Não publique esse arquivo, suas cópias `.tmp`/`.previous`, backups reais ou logs que contenham dados pessoais.

Clientes configurados pelo mantenedor são incorporados apenas durante o build; o cabeçalho gerado é ignorado pelo Git. Um aplicativo instalado não consegue esconder definitivamente o cliente que usa. Refresh tokens de contas nunca devem ser incorporados ao NRO.

As conexões verificam certificados TLS. O endereço de sessão de upload é limitado à origem Google esperada. Arquivos remotos passam pela validação de identidade, caminhos e CRC antes da restauração. A cópia de segurança anterior à restauração permanece habilitada por padrão.

Relatos de problemas devem omitir arquivos de sessão e dados reais de saves. Consulte `docs/VALIDACAO.md` para as verificações feitas e para os testes necessários no console.
