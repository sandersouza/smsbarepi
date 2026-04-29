# GitHub Project Management

Fonte de verdade local para o fluxo de gestao do SMSBarePI.

## Estado atual

- Repositorio remoto: `sandersouza/smsbarepi`
- Projeto GitHub: a definir.
- Branch de trabalho: `main` como base; novas trilhas devem nascer em branch de issue no padrao `<numero>-<slug>`.

## Regras

- Criar primeiro o espelho local de milestones/issues em `docs/`.
- So criar issue/PR remoto quando o repositorio GitHub do SMSBarePI existir.
- Nao reutilizar issues, PRs ou milestones de projetos anteriores para o SMSBarePI.
- Quando o remoto existir, atualizar este documento com:
  - nome do repositorio
  - nome do GitHub Project
  - comandos oficiais de issue/PR
  - padrao de branch

## Comandos oficiais

- Criacao de issue remota:
  - Preferir GitHub MCP quando o repo estiver instalado/disponivel.
  - Fallback oficial: `gh issue create -R sandersouza/smsbarepi --title "<titulo>" --body-file docs/issues/<numero>-<slug>.md`
- Consulta de issues:
  - `gh issue list -R sandersouza/smsbarepi --limit 50`
- Branch local vinculada a issue:
  - `git checkout -b <numero>-<slug>`

## Padrao de branch

- Branch de issue: `<numero>-<slug>`
- Branch standalone excepcional: `<slug>` apenas com justificativa tecnica registrada no handoff.

## Fluxo temporario

Enquanto nao houver remoto:

1. Registrar decisoes em `CONTEXT.md` e `docs/handoff/main.md`.
2. Manter `README.md` e `.github/instructions/` alinhados com o estado real.
3. Validar localmente com `make guard-third-party` e `make`.
