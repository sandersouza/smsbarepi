---
description: "Use para o componente de navegador de arquivos e sua integracao com menu/pause."
name: "File Browser"
applyTo:
  - "src/tools/file_browser/**"
  - "src/tools/menu/**"
  - "docs/file-browser-component.md"
---
# File Browser

## Contrato

- Manter API publica estavel (`Open`, `ProcessInput`, getters de listagem/scroll/contexto), salvo mudanca planejada.
- Navegacao deve permanecer confinada ao root do modo ativo.
- Manter suporte a `..` para voltar ao diretorio anterior.
- Ordenar diretorios antes de arquivos.
- Ignorar entradas iniciadas por `.`.

## Integracao

- `combo_menu` orquestra estado visual e acoes.
- Catalogo, navegacao e confinamento permanecem no componente de file browser.
- Mudancas de auto-repeat/scroll devem preservar previsibilidade em diretorios grandes.

## Docs e locale

- Alterou comportamento ou API: atualizar `docs/file-browser-component.md`.
- Nova label visivel deve seguir `localization.instructions.md`.
