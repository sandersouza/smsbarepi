---
description: "Use para diretrizes de plataforma e arquitetura baremetal no fork Raspberry Pi 3B."
name: "Platform and Architecture"
applyTo:
  - "src/**"
  - "include/**"
  - "linker/**"
  - "Makefile"
  - "docs/**"
---
# Platform and Architecture

## Base tecnica

- Alvo: Raspberry Pi 3B, AArch64, freestanding/no-libc (`-ffreestanding`, `-nostdlib`).
- Preferir C para runtime/kernel; limitar Assembly a bootstrap e baixo nivel estrito.
- Priorizar codigo simples, deterministico e legivel.
- Evitar alocacao dinamica salvo necessidade tecnica clara.
- Encapsular MMIO em helpers dedicados e documentar registradores relevantes.

## Boot oficial

- `BT0`: UART/logger minimo em polling para rastrear falhas antes de `screen` e `audio`
- `BT1`: `Interrupt + Timer`
- `BT2`: `screen`
- `BT3`: `audio`
- `BT4`: `SD/eMMC driver`
- `BT5`: `FatFs mount` antes do backend quando SD/eMMC inicializar
- `BT6`: leitura e aplicacao de `SD:/msx.cfg` via FatFs
- `BT7`: `NormalizeSettingsForBuild()`
- `BT8`: `initramfs` `boot-minimal`
- `BT9`: aplicacao da config no runtime
- `BT10`: `emulator backend`
- `BT11`: anexar UART ao runtime de debug quando `DEBUG=1`
- `BT12`: debug overlay pronto somente em `DEBUG=1`
- Primeiro blit visivel do backend antes do `USB Host`
- `BT13`: `USB Host`
- `BT14`: entrada no loop principal

## Fastboot

- Priorizar `screen` e `audio` antes do backend.
- Montar FatFs antes do backend e carregar configuracao diretamente do SD.
- Inicializar UART/logger minimo antes de `screen` e `audio`; em `DEBUG=1`, anexar runtime UART detalhado e overlay apos o backend.
- Fazer o primeiro blit visivel antes de inicializar `USB Host`.
- Adiar initramfs restante, mount FatFs de rotina e tarefas nao criticas para o loop principal.

## Configuracao runtime

- Arquivo canonico atual: `SD:/msx.cfg`.
- Leitura legado permitida: `SD:/msxbarepi.cfg` e `SD:/MSXBARE.CFG`.
- Gravacao: `SD:/msx.cfg` e BootConfig raw quando habilitado.
- Validar o arquivo inteiro antes de aplicar; em erro, manter fallback e registrar warning.
- Apos gravar, reabrir e validar o conteudo salvo antes de confirmar sucesso.

## Referencias

- Consultar `third_party/circle`, backend local e referencias especializadas antes de implementar drivers/perifericos sensiveis.
- Registrar fonte conceitual usada em audio, DMA, clock, video e boot quando a decisao nao for obvia.
