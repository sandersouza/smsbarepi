---
description: "Use para regras de gestao de issues/milestones/branches/PRs e sincronizacao do espelho local em docs/milestones e docs/issues."
name: "SMSBarePI Project Management"
applyTo:
  - "docs/github-project-management.md"
  - "docs/milestones/**"
  - "docs/issues/**"
  - "AGENTS.md"
  - "CONTEXT.md"
  - "docs/handoff/**"
---
# SMSBarePI Project Management

## Objetivo

- Manter fluxo previsivel de trabalho entre issues, branches, PRs e milestones.

## Fonte de verdade

- Seguir `docs/github-project-management.md` como referencia principal de processo.
- Este arquivo define guardrails compactos; comandos detalhados/checklists operacionais ficam no documento de gestao.

## Ferramentas GitHub

- Preferir **GitHub MCP** para operacoes de gestao (issues, PRs, projeto, milestones) sempre que estiver disponivel/funcional.
- Usar **`gh` CLI apenas como fallback** quando MCP estiver indisponivel, com erro de autenticacao/permissao, ou sem suporte para a operacao desejada.
- Evitar comandos longos inline no terminal para criar issues; preparar um `.md` local e usá-lo como fonte da issue remota.

## Branches

- Branch de issue deve ser vinculada a issue de origem.
- Quando uma issue atuar como umbrella/epic com branch propria, cada sub-issue operacional deve nascer em branch derivada da branch umbrella, e nao diretamente de `main`.
- Nessa topologia, a branch da sub-issue deve abrir PR para a branch umbrella; a branch umbrella so abre PR para `main` ao consolidar a trilha.
- Branch de release deve seguir padrao `release/<versao>`.
- Nunca reescrever historico da `main`.
- Nem `git commit` local nem `git push` devem acontecer durante iteracoes experimentais da issue; ambos so sao permitidos quando o usuario declarar explicitamente que o estado atual da issue esta estavel.
- Nao publicar nem consolidar cortes intermediarios apenas para backup/sincronizacao; manter iteracoes experimentais somente no worktree ate confirmacao do usuario.
- Se um commit local for feito por engano antes da estabilidade da issue, desfazer imediatamente esse commit de volta ao worktree antes de continuar.
- Antes de remover branch local, confirmar com o usuario.

## Ciclo de issue/board

- Issue recem-criada deve entrar em `Backlog`, nao em `Ready`.
- Atualizar status de item no projeto: `In progress` -> `In review` -> `Done`.
- Se testes falharem, voltar item para `In progress`.
- Ao concluir issue, mover item para `Done` no mesmo ciclo.
- Ao ativar milestone, preparar board movendo todas as issues abertas da milestone para `Ready` e manter apenas a ativa em `In progress`.

## Definicao de issue

- Toda issue deve nascer com `DoR` (Definition of Ready) e `DoD` (Definition of Done) bem definidos; quando ainda houver descoberta pendente, registrar `DoR/DoD` como sugestao inicial.
- Checklist operacional nao substitui `DoR/DoD`; ambos devem deixar claro prerequisitos, escopo de entrega e criterio de aceite.

## PRs

- Ao concluir issue estavel sem umbrella ativa, abrir PR da branch da issue para `main`.
- Ao concluir sub-issue que esteja trabalhando sob uma umbrella branch ativa, abrir PR da branch da sub-issue para a branch umbrella correspondente.
- Enviar link do PR para aprovacao/review antes de seguir para proxima issue.
- Garantir labels coerentes entre issue e PR (prioridade/tipo/area/beta).

## Espelho local de milestones/issues

- Manter `docs/milestones/` e `docs/issues/` sincronizados com remoto.
- Criar primeiro o espelho local da milestone/issue e so depois subir a milestone/issue remota ao GitHub.
- Cada milestone remota deve ter pasta local com `README.md` e campo `Milestone remota: #<numero>`.
- Cada pasta de milestone deve conter `CHALLENGES.md` com desafios/solucao/encaminhamento por issue.
- Cada issue vinculada a milestone deve ficar em `docs/milestones/m<numero>/<numero>-<slug>.md`, inclusive fechadas para historico.
- Cada issue avulsa/standalone sem milestone deve ficar em `docs/issues/<numero>-<slug>.md`, inclusive fechadas para historico.
