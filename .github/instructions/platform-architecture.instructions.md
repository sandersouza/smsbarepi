---
description: "Use para diretrizes base de plataforma/arquitetura baremetal no smsbarepi (Raspberry Pi 3B, C/ASM, freestanding, MMIO, debug HDMI)."
name: "SMSBarePI Platform and Architecture"
applyTo:
  - "src/**"
  - "include/**"
  - "linker/**"
  - "Makefile"
  - "docs/**"
---
# SMSBarePI Platform and Architecture

## Objetivo

- Garantir consistencia tecnica da base baremetal SMSBarePI para Raspberry Pi 3B.

## Plataforma alvo

- Arquitetura: AArch64.
- Imagem de boot no SD: `sms.img` (`config.txt` deve apontar `kernel=sms.img` para o core SMS).
- Artefatos estaveis de build permanecem em `artifacts/kernel8-*.img` ate rodada dedicada de renomeacao de entrega.
- Execucao freestanding/no-libc (`-ffreestanding`, `-nostdlib`).

## Linguagens e escopo

- Preferir C para runtime/kernel e logica de emulacao.
- Limitar Assembly a bootstrap e trechos estritamente de baixo nivel.

## Diretrizes de implementacao

- Priorizar codigo simples, deterministico e legivel.
- Evitar alocacao dinamica nesta fase, salvo necessidade tecnica clara.
- Encapsular MMIO em helpers dedicados e documentar objetivo/registradores.
- Evitar abstracoes prematuras em caminhos de hardware sensiveis.

## Video e debug

- HDMI e saida de video obrigatoria.
- UART e canal auxiliar de debug.
- Para overlay runtime HDMI, seguir baseline de `docs/debug-layer.md`.

## Ordem oficial de boot

- `BT1`: `Interrupt + Timer`
- `BT2`: `screen`
- `BT3`: `audio`
- `BT4`: `SD/eMMC driver`
- `BT5`: `FatFs mount` antes do backend quando o SD/eMMC inicializar
- `BT6`: leitura e aplicacao de `SD:/sms.cfg` somente via FatFs
- `BT7`: `NormalizeSettingsForBuild()`
- `BT8`: `initramfs` `boot-minimal`
- `BT9`: aplicacao da config no runtime
- `BT10`: `emulator backend`
- `BT11`: `UART + Logger` (somente `DEBUG=1`)
- `BT12`: debug overlay pronto (somente `DEBUG=1`)
- primeiro blit visivel do backend antes do `USB Host`
- `BT13`: `USB Host`
- `BT14`: entrada no loop principal

## Politica de fastboot

- Priorizar `screen` e `audio` antes do backend.
- Nao usar snapshot raw/espelho para `sms.cfg`.
- Montar FatFs antes do backend e carregar `SD:/sms.cfg` diretamente.
- `SD:/config.txt` continua separado: pode ser lido/gravado pelos fluxos de `Resolution` e `CORE`, mas nao substitui nem espelha `sms.cfg`.
- Em `DEBUG=1`, inicializar `UART + Logger` e o overlay somente apos o backend.
- Fazer o primeiro blit visivel antes de inicializar `USB Host`, para reduzir o tempo ate imagem util mesmo quando USB demorar.
- Adiar o restante do `initramfs`, o mount FatFs de rotina e outras tarefas nao criticas para o loop principal, sempre depois da primeira tentativa de inicializacao do `USB Host`.

## Fluxo oficial de configuracao

- Arquivo canonico: `SD:/sms.cfg`
- Compatibilidade de leitura: nao manter fallback para nomes de configuracao de outros projetos.
- Gravacao: sempre sobrescrever `SD:/sms.cfg`

### Carga

- montar `FatFs`
- tentar `sms.cfg`
- validar o arquivo inteiro antes de aplicar
- se houver erro, cair em fallback e registrar warning na UART/logger
- se estiver valido, aplicar as configuracoes antes do init do backend

### Gravacao

- sobrescrever o arquivo existente em `SD:/sms.cfg`
- reabrir e validar o conteudo salvo
- se estiver valido, concluir o save e voltar ao menu principal
- se estiver invalido, registrar erro e manter o estado anterior em memoria

## Referencias tecnicas

- Consultar `third_party/circle` e `third_party/smsplus` antes de implementar do zero.
- Em drivers/perifericos sensiveis (audio, DMA, clock, video), registrar fonte conceitual usada.

## Marcos tecnicos de alto nivel

- Boot minimo + UART.
- Timer/IRQ basicos.
- Framebuffer/VDP de debug.
- Loop principal de emulacao.
- Carregamento de ROM/cartuchos SMS e perifericos principais.

## Definition of Done (fase atual)

- Build gera imagem de kernel sem erros.
- Boot no Raspberry Pi 3B sem OS.
- Telemetria/saida minima de validacao visivel (HDMI e/ou UART conforme etapa).
