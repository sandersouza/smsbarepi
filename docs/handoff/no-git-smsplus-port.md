# Handoff: SMS Plus GX Port

Data: 2026-04-26

## Estado

- Workspace com repositorio Git local na branch `main`.
- Porta inicial do SMS Plus GX criada em `src/backend/sms/`.
- Backend integrado ao registry BarePI como `sms.smsplus`.
- Build local passa com `DEBUG=1` no corte SMS-only.

## Decisoes

- O primeiro corte usa framebuffer interno indexado 8bpp do SMS Plus GX e converte para RGB565 antes do blit BarePI.
- ROM loader implementado por FatFs em `SD:/sms/roms`, aceitando apenas `.sms`; no menu SMS o item `Load CART` abre direto o navegador e carrega sempre no slot unico do SMS.
- `roms/initramfs/bios.sms` e a ROM default de boot via `sms.cpio`; quando nenhum cartucho foi carregado, o backend roda a BIOS.
- Save/Load State usa `SD:/sms/slot-last.sav`.
- Sem cartucho carregado, o backend carrega `bios.sms` do initramfs como BIOS default.
- Viewport SMS usa base 256x192 para escala nativa e base fullscreen 256x243 para preencher a area visivel sem cortar. Presets de `Scale`: `1x` nativo, `2x` ponto medio entre nativo e fullscreen, `3x` fullscreen. O fullscreen e calculado como o maior multiplo de 1/16x que cabe em largura e altura: `960x540` => ~`560x531`; `640x480` => ~`496x470`.
- `Scale` so fica ativo/visivel em `DEBUG=1`; em `DEBUG=0`, o runtime ignora `scale=` persistido, normaliza para fullscreen maximo e salva `scale=3`. O Settings exibe `Scale` logo abaixo de `Proportion` apenas em `DEBUG=1`.
- Audio HDMI usa o pipeline upstream de geracao do SMS Plus GX para `sound.c`, `fmintf.c`, `emu2413.c` e `ym2413.c`. O SN76489 foi substituido no link por `src/backend/sms/modules/smsplus/smsplus_bare_psg.c`, preservando a API/contexto do SMS Plus GX e removendo `third_party/smsplus/sound/sn76489.o` dos objetos SMS sem alterar `third_party`.
- A ponte BarePI permanece no baseline de mix que estava mais proximo do correto: soma PSG+FM com ganho fixo 16x, clipping 16-bit, `BoostNoise` preservado, ganho final SMS em 100%, auto-gain do kernel desativado para SMS e ring buffer mono antes do HDMI.
- FM/YM2413 fica sempre ativo no backend SMS, configurando o core como SMS japones/domestic. O Settings SMS nao exibe mais `FM Audio`, e o build SMS nao salva nem aplica `fm_audio`/`fm_music` de configuracoes antigas.
- Logs periodicos UART de audio SMS com prefixo `SMSAUD` foram removidos. O audio permanece observavel pelo comportamento em hardware e pela telemetria agregada ja existente do backend; reinstrumentar UART apenas em nova investigacao pontual.
- Buffer de ROM limitado a 1 MB para manter o footprint dentro da janela atual do kernel.
- `sms.cfg` agora e lido/gravado somente via FatFs em `SD:/sms.cfg`; o espelho raw foi removido. `Proportion` persiste em `sms.cfg` como `proportion=16:9` ou `proportion=4:3`, com migracao de leitura para `resolution=720` => `16:9` e `resolution=768` => `4:3`. O kernel aplica essa escolha por `CComboScreenDevice::Resize()` (`960x540` ou `640x480`) no boot e em runtime, sem ler nem gravar `SD:/config.txt`.
- O fluxo de `CORE` persiste somente o bloco `#---KERNEL START---`/`#---KERNEL END---` do `config.txt`; sem esses marcadores, a troca de core falha sem regravar o arquivo.
- O menu `Settings -> CORE` enumera somente `*.img` presentes em `SD:/`; o fallback interno de kernel e `sms`, nao o id de backend `smsplus`.
- Ao sair de Settings, o kernel executa `HandleSettingsMenuClosed()` e agenda `SaveSettingsToStorage()`. Se `CORE` mudou, o mesmo fechamento tambem persiste `SD:/config.txt` e agenda reboot. Sair direto do pause menu com Settings ativo continua usando o mesmo fechamento.
- A mecanica `make menuconfig`/Kconfig foi removida do build SMS-only. O `Makefile` nao le `.config`, nao gera `build/config_build.h`, e o kernel nao usa flags de boot mode/feature enable vindas de Kconfig; o boot mode SMS permanece fixo em `NORM`. As opcoes de build restantes sao flags diretas como `DEBUG=0|1` e `OWNER=<nome>`.

## Validacao

```sh
scripts/check-third-party-pristine.sh
bash -n scripts/install-toolchains.sh scripts/make-initramfs.sh scripts/build-config.sh scripts/prepare-sd.sh scripts/update-sd-incremental.sh scripts/make-sdcard-package.sh scripts/check-third-party-pristine.sh scripts/check-toolchain.sh scripts/logserial.sh scripts/perf-regression.sh
make -j4 DEBUG=1
make -j4 DEBUG=0
make -n sdcard
```

Resultado atual:

- `artifacts/kernel8-default.img`
- `build/src/sms.img`
- `artifacts/kernel8-sms-cleanup.img`
- Ultima build local: `BUILD_BID=202604291752-090c2b1`, `TXTDBG BUILD:6824`, SHA1 `cb95ec86d3d30d9af6a8965dfad362a64655ff58` (`DEBUG=0`).

## Pendencias

- Validar em hardware Raspberry Pi 3B.
- Testar `Load CART` com ROM real em `SD:/sms/roms`.
- Validar em hardware Raspberry Pi 3B o audio da BIOS EU REV1/SEGA logo e `SHINOBI.SMS` com FM sempre ativo.
