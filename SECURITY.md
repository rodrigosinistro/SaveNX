# Security

## Credenciais

Nunca abra uma issue com `client_secret.json`, refresh tokens, códigos OAuth ainda válidos, dumps do console ou saves pessoais. Remova também UIDs e Title IDs do log quando quiser mantê-los privados.

O arquivo real de credenciais fica apenas em `sdmc:/config/SaveNX/client_secret.json` e é ignorado pelo Git.

## Restore

Teste novas versões com um save descartável. O SaveNX valida identidade e integridade antes do restore e cria um backup automático por padrão, mas falhas de energia, cartão SD ou filesystem ainda podem causar perda de dados. Mantenha cópias independentes.

Não habilite escrita em saves de sistema/NAND para restaurar saves comuns de jogos.

## Relato de vulnerabilidade

Ao publicar este projeto em um repositório próprio, configure um canal privado de security advisories e substitua esta seção pelo contato do mantenedor. Não inclua segredos ou saves reais em relatos públicos.
