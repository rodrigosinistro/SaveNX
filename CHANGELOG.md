# Changelog

## 0.3.0 — retomada

- Recupera a base-fonte SaveNX v0.1.0 com proveniência explícita e compila um novo NRO.
- Tela inicial sem enumeração de saves ou OAuth; ações explícitas para carregar jogos e conectar Google.
- Dados concentrados em `/switch/SaveNX`, sem substituir o devoptab SD do libnx.
- Cliente OAuth configurável no build ou no aplicativo; persistência recuperável da sessão e tratamento de pending/slow_down/denied/expired.
- Fila de uploads persistente com snapshots independentes, reenvio e confirmação por ID/tamanho.
- Recupera uploads com resposta perdida por propriedade privada do Drive e mantém backups locais.
- Corrige fallback de dados de título e caracteres de controle/marcadores de cor em nomes.
- Adiciona testes host e workflow de compilação e empacotamento Switch.

Pacote v0.2.17 preservado como referência de comportamento; seus fontes não estavam disponíveis no pacote instalável.
