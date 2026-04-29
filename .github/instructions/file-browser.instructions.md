---
description: "Use quando alterar o componente de navegador de arquivos (combo_file_browser) e sua integração com menu/pause."
name: "SMSBarePI File Browser Guardrails"
applyTo:
  - "src/combo_file_browser*.h"
  - "src/combo_file_browser*.cpp"
  - "src/combo_menu*.h"
  - "src/combo_menu*.cpp"
  - "docs/file-browser-component.md"
---
# SMSBarePI File Browser Guardrails

## Objetivo

- Preservar comportamento previsivel do navegador de arquivos e evitar regressao de UX.

## Contrato funcional

- Manter API publica do componente estavel (`Open`, `ProcessInput`, getters de listagem/scroll/contexto), salvo mudanca planejada.
- Navegacao deve permanecer confinada ao root do modo ativo (`SD:/sms/roms` ou `SD:/sms/roms`).
- Manter suporte a `..` para retorno ao diretorio anterior.
- Continuar ordenacao: diretorios antes de arquivos.
- Ignorar entradas iniciadas por `.`.

## Integracao com menu

- `combo_menu` deve orquestrar estado visual/acoes; logica de catalogo/navegacao permanece no componente.
- Alteracoes de auto-repeat/scroll precisam preservar previsibilidade em diretorios grandes.

## Sincronizacao documental

- Alterou comportamento do browser ou da API: atualizar `docs/file-browser-component.md` na mesma mudanca.
- Se houver novas labels visiveis no fluxo, aplicar regras de locale PT/EN/ES.
