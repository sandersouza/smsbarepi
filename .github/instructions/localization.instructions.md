---
description: "Use quando criar ou alterar labels visiveis ao usuario em menus, overlays e file browser. Garante localizacao PT/EN/ES e evita strings hardcoded fora do sistema de locale."
name: "SMSBarePI Localization Guardrails"
applyTo:
  - "src/combo_locale*.cpp"
  - "src/combo_locale*.h"
  - "src/combo_menu*.cpp"
  - "src/combo_menu*.h"
  - "src/combo_debug_overlay*.cpp"
  - "src/combo_debug_overlay*.h"
  - "src/combo_file_browser*.cpp"
  - "src/combo_file_browser*.h"
  - "docs/debug-layer.md"
---
# SMSBarePI Localization Guardrails

## Objetivo

- Garantir consistencia de strings visiveis ao usuario entre idiomas suportados (`PT`, `EN`, `ES`).

## Regras

- Toda nova string visivel ao usuario deve entrar no sistema de locale; evitar hardcode fora das tabelas de traducao.
- Ao criar/alterar labels de menu, submenu, campos, slots e estados (ex.: `[ EMPTY ]`), atualizar `PT`, `EN` e `ES` na mesma mudanca.
- Manter nomes de chaves de locale estaveis e sem duplicacao semantica.
- Preservar tom curto e claro para texto de UI em todos os idiomas.

## Areas sensiveis

- Mudancas em menu/overlay/file browser devem revisar impacto em texto exibido.
- Se a alteracao mudar hotkeys, labels ou campos de debug, atualizar `docs/debug-layer.md` junto.

## Validacao minima

- Confirmar que os tres idiomas carregam sem fallback inesperado.
- Verificar rapidamente no fluxo de UI afetado se nenhuma string ficou faltando ou truncada.
