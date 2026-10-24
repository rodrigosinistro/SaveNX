# Configurações — SaveNX 0.3.0

| Opção | Padrão | Comportamento |
|---|---:|---|
| Backup automático ao restaurar | Ligado | Cria uma cópia de segurança antes da restauração. |
| Upload automático para remoto | Ligado | Registra o ZIP na fila e tenta enviar quando o Drive está conectado. Desligar impede novos envios automáticos. |
| Manter backups locais | Ligado | Esta versão mantém uma cópia local independente, inclusive após enviar ao Drive. |
| Exportar saves para ZIP | Ligado | Necessário para a fila de upload e recomendado para portabilidade. |
| Segurar para restaurar | Ligado | Exige confirmação prolongada para restaurar. |
| Mostrar apenas títulos montáveis | Ligado | Esconde entradas que não podem ser abertas. |
| Escrita em saves de sistema/NAND | Desligado | Saves comuns de jogos não precisam desta opção. |

A tela inicial separa o carregamento de jogos da conexão Google. Escolha **Conectar Google Drive** para autorizar ou recuperar uma sessão existente. **Reenviar backups pendentes** processa a fila com o aplicativo aberto. O perfil e o título continuam isolados por identificadores estáveis.

Configuração, sessão, fila e logs ficam sob `sdmc:/switch/SaveNX`. A pasta local de backups padrão é `sdmc:/switch/SaveNX/backups`.
