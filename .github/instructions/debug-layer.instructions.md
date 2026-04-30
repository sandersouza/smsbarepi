---
description: "Use para overlay/debug runtime HDMI, UART, hotkeys, labels e menu de debug."
name: "Debug Layer"
applyTo:
  - "src/tools/debug/**"
  - "src/tools/menu/**"
  - "src/kernel.cpp"
  - "docs/debug-layer.md"
---
# Debug Layer

## Regras

- Em desenvolvimento, preferir `DEBUG=1`; usar `DEBUG=0` somente quando solicitado ou em release.
- Hotkeys de debug devem atuar somente em build debug.
- Nao misturar `console_puts()` com painel de debug em runtime.
- Desenhar overlay apos o blit do emulador.
- Usar caixa opaca curta; evitar faixa opaca global sem necessidade.
- Fazer flush de cache apenas na area do painel, com throttle.
- Evitar getters que copiam structs grandes; preferir getters escalares.

## Overlay e UART

- `TXTDBG BUILD:XXXX` e o identificador visual canonico da build em teste.
- Limitar linhas do overlay HDMI a 64 caracteres visiveis.
- Ao adicionar/remover linha, atualizar limpeza visual no mesmo ciclo.
- Alternar `MAX/MIN/OFF` deve limpar framebuffer visivel e superficies auxiliares para nao deixar vestigios.
- Usar UART como canal oficial para telemetria detalhada quando overlay nao bastar, especialmente memoria/estado: endereco, esperado, lido e ponto temporal.

## Docs

- Preservar baseline de `docs/debug-layer.md` salvo motivo tecnico concreto.
- Mudou hotkey, label, item de menu, semantica de campo ou fluxo de debug: atualizar `docs/debug-layer.md`.
