# Milestones locais

Espelho local de milestones/issues do GitHub para planejamento técnico e handoff.

## Convenção
- Pasta: `m<numero-da-milestone>`
- Arquivo de issue: `<numero>-<slug>.md`
- Cada pasta de milestone deve conter `README.md` com metadados/descrição da milestone remota.
- O `README.md` da pasta deve conter obrigatoriamente `Milestone remota: #<numero>` para permitir auditoria/sync automática.
- Cada pasta de milestone deve conter `CHALLENGES.md` com histórico por issue: problema enfrentado, solução adotada, status (`Resolvido`/`Parcial`/`Futuro`) e referência de continuidade quando aplicável.

## Pastas atuais

O workspace local ainda pode conter historico de milestones antigas, mas a trilha
ativa deste port e SMSBarePI:

- `m0` (Hello World / baseline baremetal)
- `m1` (Debug Overlay HDMI)
- `m2` (Core BarePI)
- `m3` (Bring-up do Core)
- `m5` (Estabilizacao e Performance)
- `m6` (Review da documentacao)
- `m7` (Beta publica v0.1 - Performance e Boot)
- `m13` (Release: Dev Debug Toolkit)
- `m14` (Framework de Migracao de Emuladores Bare Metal)
- `m21` (Release: Build Menuconfig e Feature Gates)
- `m22` (Release: Suporte SMS)
