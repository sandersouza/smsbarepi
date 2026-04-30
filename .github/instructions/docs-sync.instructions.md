---
description: "Use para atualizar docs, handoff, CONTEXT.md e espelho local de milestones/issues."
name: "Docs Sync"
applyTo:
  - "docs/**"
  - "AGENTS.md"
  - "CONTEXT.md"
---
# Docs Sync

## Regras

- Mudanca relevante de codigo deve atualizar a documentacao minima aplicavel.
- Quando reduzir ruido, acumular docs para o checkpoint estavel em vez de atualizar a cada microajuste.
- Manter `docs/handoff/<nome-da-branch>.md` com estado atual, riscos, validacao e proximos passos.
- Manter `CONTEXT.md` apontando para o handoff ativo da branch.
- Alteracao de fluxo/guardrail deve atualizar `AGENTS.md` somente quando necessario.

## Milestones e issues locais

- Seguir `project-management.instructions.md` para regras de GitHub e espelho local.
- Manter `docs/milestones/` e `docs/issues/` sincronizados com o remoto no ciclo de criacao/atualizacao.
- Usar o arquivo local da issue como fonte para publicacao remota quando possivel.

## Qualidade

- Registrar decisoes tecnicas com contexto, trade-off e impacto.
- Referenciar fontes consultadas quando influenciarem a implementacao.
- Evitar texto historico redundante; priorizar estado atual e proximos passos verificaveis.
