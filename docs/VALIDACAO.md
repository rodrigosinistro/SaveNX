# Estado de validação da versão 0.1.3

Verificações executadas neste pacote:

- os 16 arquivos de tradução são JSON válido e foram comprimidos/descomprimidos sem diferença;
- o exemplo OAuth é JSON válido e não contém credenciais reais;
- o workflow de build é YAML válido;
- links relativos da documentação apontam para arquivos existentes;
- os testes host de caminhos ZIP e do layout unificado passam com `-Wall -Wextra -Werror`;
- o bundle de CA é parseável pelo OpenSSL;
- ícone NRO: JPEG 256 × 256; ícone de cabeçalho: PNG 48 × 50;
- `git diff --check` não encontrou erros de whitespace.
- o relatório de falha real da `v0.1.1` foi diagnosticado como estouro de pilha na migração recursiva; a `v0.1.2` substitui a recursão por uma fila alocada no heap e preserva a migração parcial.
- a `v0.1.3` monta o SD e inicializa o log antes dos demais serviços, permitindo diagnosticar encerramentos antecipados que não geram relatório do Atmosphère.

Limite deste ambiente: devkitPro/devkitA64 e `libnx` não estão instalados, portanto `SaveNX.nro` não foi produzido localmente. O workflow em `.github/workflows/build.yml` usa a imagem oficial `devkitpro/devkita64`, que já contém `switch-dev` e `switch-portlibs`, e é o caminho preparado para a compilação completa.

Antes de uma release estável ainda é necessário:

1. executar o workflow e corrigir qualquer incompatibilidade detectada pelo toolchain atual;
2. testar login, upload, download e refresh token em um Switch com Atmosphère;
3. testar backup/restore de um save descartável em dois perfis diferentes;
4. simular ZIP corrompido e confirmar que o save atual permanece intacto;
5. testar saves maiores que o journal e interrupções de rede.
