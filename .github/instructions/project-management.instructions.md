---
description: "Use para gestao de issues, milestones, branches, PRs, board e espelho local em docs/milestones e docs/issues."
name: "Project Management"
applyTo:
  - "docs/github-project-management.md"
  - "docs/milestones/**"
  - "docs/issues/**"
  - "AGENTS.md"
  - "CONTEXT.md"
  - "docs/handoff/**"
---
# Project Management

## Fonte e ferramentas

- Seguir `docs/github-project-management.md` como fonte de verdade do fluxo.
- Para tarefas GitHub, tentar primeiro o GitHub MCP.
- Usar `gh` CLI somente se o MCP nao tiver funcao aplicavel ou falhar.
- Ao usar `gh`, evitar textos grandes inline; usar o `.md` do espelho local como fonte.

## Issues, milestones e board

- Toda issue deve ter milestone.
- Se nao houver milestone adequada, usar ou criar `FIX: Minor Issues / Other Issues`; ela e permanente e nao deve ser apagada.
- Issue nova entra em `Backlog`; mover no board conforme o fluxo: `Ready`, `In progress`, `In review`, `Done`.
- Ao ativar uma milestone, mover issues abertas da milestone para `Ready` e manter apenas a ativa em `In progress`.
- Toda issue deve registrar DoR e DoD; checklist operacional nao substitui criterio de aceite.

## Branches e PRs

- Branch de issue deve estar vinculada a issue de origem.
- Sub-issue de umbrella/epic deve derivar da branch umbrella e abrir PR contra ela; a umbrella abre PR para `main` ao consolidar.
- Branch de release usa `release/<versao>`.
- Nao reescrever historico da `main`.
- Nao remover branch local sem confirmar com o usuario.
- Garantir labels coerentes entre issue e PR.

## Estabilidade, commit e sincronizacao

- `git commit` e `git push` so podem ocorrer quando o usuario declarar a issue estavel.
- Se um commit local for feito por engano antes da estabilidade, desfazer o commit de volta ao worktree.
- Quando o usuario disser apenas `estavel`, abrir o PR da branch correta e aguardar review.
- Quando o usuario disser `mergeado` apos o PR, sincronizar remotos/locais e voltar para a branch correta: umbrella ativa ou `main`.

## Espelho local

- Manter `docs/milestones/` e `docs/issues/` sincronizados com o remoto.
- Criar o espelho local antes da milestone/issue remota.
- Milestone local: `docs/milestones/m<numero>/README.md` com `Milestone remota: #<numero>` e `CHALLENGES.md`.
- Issue de milestone: `docs/milestones/m<numero>/<numero>-<slug>.md`.
- Issue standalone: `docs/issues/<numero>-<slug>.md`.
