# Contexto Atual

Handoff ativo: `docs/handoff/no-git-smsplus-port.md`

Estado:

- Workspace com repositorio Git local na branch `main`.
- Backend default fixo: `sms.smsplus`.
- Kernel de boot no SD: `sms.img`.
- Initramfs externo: `sms.cpio`.
- Diretorio de ROMs: `SD:/sms/roms`.
- Loader visivel esperado: `Load CART`, abrindo direto o navegador de ROMs SMS.

Ultima validacao local:

```sh
scripts/check-third-party-pristine.sh
bash -n scripts/install-toolchains.sh scripts/make-initramfs.sh scripts/build-config.sh scripts/prepare-sd.sh scripts/update-sd-incremental.sh scripts/make-sdcard-package.sh scripts/check-third-party-pristine.sh scripts/check-toolchain.sh scripts/logserial.sh scripts/perf-regression.sh
make -j4 DEBUG=1
make -j4 DEBUG=0
make -n sdcard
```

Resultado atual: build concluido, gerando `artifacts/kernel8-default.img`, `build/src/sms.img` e `artifacts/kernel8-sms-cleanup.img`.
Ultima build: `BUILD_BID=202604291752-090c2b1`, `TXTDBG BUILD:6824`, SHA1 `cb95ec86d3d30d9af6a8965dfad362a64655ff58` (`DEBUG=0`).
Audio SMS validado em link local com `third_party/smsplus/sound/sound.o` fornecendo `sound_init`, `sound_update`, `psg_write`, `fmunit_write`, `stream_update` e `snd`. O renderer SN76489 e local em `src/backend/sms/modules/smsplus/smsplus_bare_psg.c`, substituindo o objeto `third_party/smsplus/sound/sn76489.o` sem modificar `third_party`; YM2413/FM continua upstream. A ponte SMS usa mixer simples com ganho fixo 16x, `BoostNoise` preservado, clipping 16-bit, ring buffer mono, ganho final SMS em 100% e auto-gain SMS desativado. FM fica sempre ativo no backend SMS; o Settings SMS nao tem item `FM Audio`, o build SMS nao salva nem aplica `fm_audio`/`fm_music`, e os logs periodicos UART `SMSAUD` foram removidos.
Viewport SMS: presets de `Scale` sao `1x` nativo 256x192, `2x` ponto medio entre nativo e fullscreen, e `3x` fullscreen. `Scale` so fica ativo/visivel em `DEBUG=1`; em `DEBUG=0`, o runtime ignora `scale=` persistido e usa sempre o fullscreen maximo permitido pela proporcao. O fullscreen calcula o maior multiplo barato de 1/16x que cabe em largura e altura. Com `proportion=16:9`, o framebuffer Circle e `960x540` e o viewport esperado e ~`560x531`; com `proportion=4:3`, o framebuffer e `640x480` e o viewport esperado e ~`496x470`.
Configuracao SMS-only: scripts e `sms.cfg` default usam `cartridge_path=`; quando vazio, o backend usa `roms/initramfs/bios.sms`. O kernel nao le nem grava espelho raw de `sms.cfg`: settings persistem somente em `SD:/sms.cfg`. `Proportion` persiste como `proportion=16:9` ou `proportion=4:3`, aceita migracao de `resolution=720/768`, e aplica `CComboScreenDevice::Resize()` sem ler/gravar `config.txt`. `SD:/config.txt` permanece separado para `CORE`, que grava apenas o bloco entre `#---KERNEL START---` e `#---KERNEL END---`.
Ao sair de Settings com `ESC`, ou fechar com `F12` enquanto Settings estiver ativo, o kernel agenda `SaveSettingsToStorage()`. Se `CORE` mudou, o fechamento tambem persiste o bloco `KERNEL` e agenda reboot.
Menu `Settings -> CORE`: lista apenas arquivos `*.img` encontrados em `SD:/` e usa `sms` como fallback de kernel, separando nome de kernel (`sms.img`) do backend (`sms.smsplus`).
Build SMS-only: a mecanica `make menuconfig`/Kconfig foi removida. O build nao le `.config` nem gera `build/config_build.h`; opcoes restantes ficam em flags diretas como `DEBUG=0|1` e `OWNER=<nome>`. O boot mode e fixo em `NORM` no kernel SMS.
