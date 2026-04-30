---
description: "Use para strings visiveis em locale, menu, debug e file browser."
name: "Localization"
applyTo:
  - "src/tools/locale/**"
  - "src/tools/menu/**"
  - "src/tools/debug/**"
  - "src/tools/file_browser/**"
  - "docs/debug-layer.md"
  - "docs/file-browser-component.md"
---
# Localization

## Regras

- Toda nova string visivel deve entrar no sistema de locale.
- Atualizar PT, EN e ES na mesma mudanca.
- Evitar hardcode fora das tabelas de traducao.
- Manter chaves estaveis e sem duplicacao semantica.
- Usar texto curto e claro em todos os idiomas.

## Validacao

- Confirmar que PT/EN/ES carregam sem fallback inesperado.
- Verificar no fluxo afetado se nenhuma string ficou faltando, truncada ou desalinhada.
