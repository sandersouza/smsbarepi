---
description: "Use para referencias tecnicas e alinhamento de implementacao."
name: "Code Reference"
applyTo:
  - "src/**"
  - "include/**"
  - "docs/**"
  - "third_party/**"
---
# Code Reference

## Regra principal

- Antes de implementar comportamento complexo, consultar referencias existentes em `third_party/`, `src/`, `include/` e `docs/`.
- Se houver solucao aplicavel, adaptar ao ambiente baremetal em vez de reinventar.
- Registrar no codigo, docs ou handoff a referencia usada quando ela influenciar comportamento relevante.

## Prioridade de referencias

- `blueMSX` e referencia semantica primaria para MSX/MSX2+/V9958 quando disponivel.
- Consultar `third_party/fmsx`, `third_party/circle` e referencias internas antes de implementar comportamento complexo.
- Em divergencias entre backend local e referencia, auditar a diferenca antes de criar heuristica local.

## Areas sensiveis

- CPU, VDP/V9958, PSG/audio, memoria, mappers/cartuchos, I/O, video, DMA, clocks e boot.
- Para drivers/perifericos, documentar restricoes baremetal que impedirem espelhamento literal da referencia.
