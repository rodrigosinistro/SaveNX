# Formato de backup SaveNX

## Layout no Drive

```text
SaveNX/USER_<UID32HEX>/TITLE_<TITLEID16HEX>/<timestamp>.zip
```

O apelido do perfil e o nome do jogo não participam das chaves remotas. Isso mantém o caminho estável depois de renomear um usuário, mudar o idioma do console ou atualizar os metadados do título.

## Conteúdo do ZIP

Cada backup em nuvem contém:

- todos os arquivos e diretórios visíveis na raiz montada do save;
- `.nx_save_meta.bin`, estrutura binária necessária para validar e, quando preciso, ampliar o container antes do restore;
- `savenx_manifest.json`, descrição legível e versionada do backup.

Exemplo resumido do manifesto:

```json
{
  "schema": "savenx.backup.v1",
  "createdBy": "SaveNX",
  "appVersion": "0.1.0",
  "createdAtUnix": 1786492800,
  "user": {
    "nickname": "Jogador",
    "storageKey": "USER_00112233445566778899AABBCCDDEEFF",
    "accountUid": "00112233445566778899AABBCCDDEEFF"
  },
  "title": {
    "applicationId": "0100AABBCCDDEEFF",
    "name": "Nome do jogo"
  },
  "save": {
    "saveDataId": "0000000000001234",
    "type": 1,
    "rank": 0,
    "index": 0,
    "dataSize": 33554432,
    "journalSize": 16777216
  }
}
```

O arquivo binário também registra application ID, AccountUid, system save ID, tipo, rank, índice, owner ID, tamanhos de dados/journal, flags e commit ID fornecidos pelo Horizon OS.

## Pré-condições do restore

Antes de apagar qualquer arquivo do save atual, o SaveNX:

1. abre o ZIP;
2. rejeita nomes absolutos, componentes `.`/`..`, barras invertidas e caminhos com `:`;
3. lê integralmente todas as entradas e exige que o CRC de cada uma seja válido;
4. exige `.nx_save_meta.bin` com magic/revisão reconhecidos;
5. compara application ID, system save ID, tipo, rank e índice com o slot selecionado;
6. para saves de conta, compara os 128 bits do AccountUid;
7. aumenta o container apenas se o backup exigir mais espaço;
8. cria um backup automático do conteúdo atual, quando a opção padrão está ativa;
9. somente então limpa, grava e faz commit no filesystem do save.

`savenx_manifest.json` e `.nx_save_meta.bin` são arquivos de controle e não são copiados para dentro do save restaurado.

## Compatibilidade

- `savenx.backup.v1` é o primeiro schema público do manifesto.
- A restauração é comandada pelo metadata binário, não por nomes exibidos na interface.
- ZIPs antigos sem `.nx_save_meta.bin` não são restaurados automaticamente porque não permitem validar com segurança usuário e slot.
- Um save ainda precisa ter um container no destino. A interface herdada permite criar o container para títulos instalados quando ele não existe.
