# AGENTS

## Objetivo do projeto

Construir uma base baremetal para Raspberry Pi 3B e evoluir ate um emulador Sega SMS executavel sem sistema operacional, usando SMS Plus GX como backend inicial.

## Fonte canonica de regras de fluxo

As regras operacionais estao centralizadas em `.github/instructions/`.

Regra unica: sempre carregar e aplicar **todos** os arquivos `*.instructions.md` dessa pasta.

## Referencias rapidas

- Contexto persistente da branch: `docs/handoff/<nome-da-branch>.md`
- Ponteiro de contexto atual: `CONTEXT.md`
- Fonte de verdade de gestao: `docs/github-project-management.md`
- Kernel no SD: `sms.img`
- Initramfs no SD: `sms.cpio`
- ROMs: `SD:/sms/roms`
