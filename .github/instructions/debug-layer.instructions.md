---
description: "Use quando alterar overlay/debug runtime HDMI, hotkeys, labels de telemetria e comportamento visual de painel/menu de pause."
name: "SMSBarePI Debug Layer Guardrails"
applyTo:
  - "src/combo_debug_overlay*.h"
  - "src/combo_debug_overlay*.cpp"
  - "src/kernel.cpp"
  - "docs/debug-layer.md"
---
# SMSBarePI Debug Layer Guardrails

## Objetivo

- Manter consistencia visual e operacional da debug layer HDMI.

## Build e ativacao

- Em desenvolvimento, preferir build com `DEBUG=1`.
- Usar `DEBUG=0` apenas quando solicitado explicitamente.
- Hotkeys de debug devem atuar somente quando compilado com debug.

## Regras de runtime/overlay

- Nao misturar `console_puts()` com painel de debug em runtime.
- Desenhar overlay apos blit do emulador.
- Usar caixa opaca curta para legibilidade; evitar faixa opaca global sem necessidade.
- Fazer flush de cache apenas na area do painel, com throttle.
- Evitar getters de debug que copiam structs grandes; preferir getters escalares.
- Tratar `TXTDBG BUILD:XXXX` (4 digitos hex) como identificador visual canonico da build em teste.
- Em casos de freeze com RPi ainda responsivo, complementar overlay HDMI com logs UART para diagnostico aprofundado.
- Quando houver investigacao de runtime em hardware (ex.: cheat nao aplicado, valor incorreto em memoria, divergencia entre UI e estado interno), priorizar telemetria adicional via UART com endereco, valor esperado, valor lido e ponto temporal da operacao.
- Tratar UART como canal oficial de telemetria detalhada para depuracao de memoria/estado quando o overlay HDMI nao for suficiente ou puder mascarar o problema.
- Limitar cada linha do overlay HDMI a no maximo 64 caracteres visiveis para nao sobrepor a area do mailbox do emulador.
- Ao adicionar nova linha no overlay, atualizar a rotina de limpeza (`ClearDebugOverlay`) no mesmo ciclo para limpar explicitamente a nova linha quando o debug for desativado.
- No preset `DEBUG [MIN]`, a linha compacta de `FPS` deve carregar tambem o contador de `frame` atual quando isso ajudar a correlacionar o estado exibido com logs UART.
- A limpeza/reescrita do overlay deve usar o comprimento real previamente renderizado em cada linha; nao limpar com padding fixo maior que o conteudo visivel.
- Toda alternancia entre modos do `DEBUG OVERLAY` (`MAX/MIN/OFF`) deve limpar a tela inteira antes da nova fase, incluindo framebuffer visivel e superfícies auxiliares, para nao deixar vestigios entre os modos.

## Baseline visual

- Reutilizar baseline atual documentado em `docs/debug-layer.md` salvo motivo tecnico concreto.
- Preservar layout quando legivel e alterar apenas campos/contadores necessarios no ciclo.

## Sincronizacao documental

- Mudou hotkey, label, item de menu, semantica de campo ou fluxo de debug: atualizar `docs/debug-layer.md` na mesma mudanca.
