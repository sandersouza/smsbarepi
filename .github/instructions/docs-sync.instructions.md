---
description: "Use quando atualizar contexto de handoff, milestones/issues locais, ou documentacao tecnica em docs/. Cobre sincronizacao entre codigo, docs/milestones, docs/issues, AGENTS.md e CONTEXT.md."
name: "SMSBarePI Docs Sync"
applyTo:
  - "docs/**"
  - "AGENTS.md"
  - "CONTEXT.md"
---
# SMSBarePI Docs Sync

## Objetivo

- Manter rastreabilidade entre mudancas de codigo, estado operacional e espelho local de milestones/issues.

## Regras de sincronizacao

- Toda mudanca relevante de codigo deve incluir atualizacao minima em `docs/` quando aplicavel.
- Ao fim de cada ciclo relevante (build, bug, decisao tecnica, milestone), atualizar `docs/handoff/<nome-da-branch>.md`.
- Manter `CONTEXT.md` apontando corretamente para o arquivo de handoff ativo da branch.
- Alteracoes de fluxo/guardrails do projeto devem refletir em `AGENTS.md` quando necessario.

## Milestones e issues locais

- Seguir `docs/github-project-management.md` como fonte de verdade para workflow.
- Manter `docs/milestones/` e `docs/issues/` sincronizados com remoto no mesmo ciclo de criacao/atualizacao.
- Em criacao de milestone/issue, preparar primeiro o espelho local (`README.md`, `CHALLENGES.md`, arquivo da issue) e usar esse material como fonte para publicacao remota.
- Cada milestone remota deve ter pasta local com `README.md` contendo `Milestone remota: #<numero>`.
- Cada pasta de milestone deve ter `CHALLENGES.md` com desafios por issue, solucao adotada e encaminhamentos.
- Cada issue vinculada a milestone deve ter arquivo local `docs/milestones/m<numero>/<numero>-<slug>.md`.
- Cada issue avulsa/standalone sem milestone deve ter arquivo local `docs/issues/<numero>-<slug>.md`.

## Qualidade da documentacao

- Registrar decisoes tecnicas com contexto, trade-off e impacto.
- Referenciar fontes consultadas quando influenciarem implementacao/correcao (ex.: arquivos/classes em `third_party/smsplus` e `third_party/circle`).
- Evitar texto redundante; priorizar estado atual, riscos em aberto e proximos passos verificaveis.
