# Security

## Credenciais

Nunca abra uma issue com `google-drive.json`, refresh tokens, códigos OAuth ainda válidos, dumps do console ou saves pessoais. Remova também UIDs e Title IDs do log quando quiser mantê-los privados.

Os valores do cliente privado são injetados por GitHub Actions secrets e não devem ser commitados. O token da conta fica apenas em `sdmc:/switch/SaveNX/config/google-drive.json`.

## Restore

Teste novas versões com um save descartável. O SaveNX valida identidade e integridade antes do restore e cria um backup automático por padrão, mas falhas de energia, cartão SD ou filesystem ainda podem causar perda de dados. Mantenha cópias independentes.

Não habilite escrita em saves de sistema/NAND para restaurar saves comuns de jogos.

## Relato de vulnerabilidade

Ao publicar este projeto em um repositório próprio, configure um canal privado de security advisories e substitua esta seção pelo contato do mantenedor. Não inclua segredos ou saves reais em relatos públicos.
