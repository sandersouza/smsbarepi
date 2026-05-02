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
git diff --check
bash -n scripts/prepare-sd.sh scripts/make-sdcard-package.sh scripts/update-sd-incremental.sh
make -j4 DEBUG=1
make -j4 DEBUG=0
make -n sdcard
```

Resultado atual: build concluido, gerando `artifacts/kernel8-default.img` e `build/src/sms.img`.
Ultima build: `BUILD_BID=202605011231-d9fab11`, `TXTDBG BUILD:E2D2`, SHA1 `174290c81f7142e658a1546a658bfabc129d3b32` (`DEBUG=0`).
Audio SMS validado em link local com `third_party/smsplus/sound/sound.o` fornecendo `sound_init`, `sound_update`, `psg_write`, `fmunit_write`, `stream_update` e `snd`. O renderer SN76489 e local em `src/backend/sms/modules/smsplus/smsplus_bare_psg.c`, substituindo o objeto `third_party/smsplus/sound/sn76489.o` sem modificar `third_party`; YM2413/FM continua upstream. A ponte SMS usa mixer simples com ganho fixo 16x, `BoostNoise` preservado, clipping 16-bit, ring buffer mono, ganho final SMS em 100% e auto-gain SMS desativado. FM/YM2413 e controlado por `Settings -> FM Sound [ON/OFF]`, persistido como `fm_music=0|1`; `ON` ativa `sms.use_fm` e o mapa SMSJ antes de `system_init/system_reinit` para expor as portas Mark III/SMSJ `0xF0/0xF1/0xF2`; `OFF` volta ao mapa SMS sem FM. O latch `0xF2` e adaptado via wrapper local para retornar apenas bit 0, inclusive durante deteccao com I/O chip desabilitado.
Viewport SMS: framebuffer Circle dinamico. `Overscan OFF` e o default e usa framebuffer `2.25x` (`256x192` -> `576x432`) com o viewport do emulador em escala exata `2x` centralizada. `Overscan ON` usa framebuffer `2x` da area ativa (`256x192` -> `512x384`). O Settings nao exibe `Scale` nem `Proportion`.
Configuracao SMS-only: `sms.cfg` nao persiste midia, escala nem proporcao; persiste `overscan=0|1`. ROMs, discos e cassetes devem ser carregados manualmente pelo menu; no boot, o backend usa `roms/initramfs/bios.sms` quando nenhum cartucho foi carregado. Save/Load State usa `SD:/sms/slot-last.sav`, com serializacao em memoria pela ponte local SMS Plus. O kernel nao le nem grava espelho raw de `sms.cfg`: settings persistem somente em `SD:/sms.cfg`. `scale=`/`scale_max=`/`proportion=`/`resolution=` de configs antigas sao ignorados. `SD:/config.txt` permanece separado para `CORE`, que grava apenas o bloco entre `#---KERNEL START---` e `#---KERNEL END---`.
Ao sair de Settings com `ESC`, ou fechar com `F12` enquanto Settings estiver ativo, o kernel agenda `SaveSettingsToStorage()`. Se `CORE` mudou, o fechamento tambem persiste o bloco `KERNEL` e agenda reboot.
Menu `Settings -> CORE`: lista apenas arquivos `*.img` encontrados em `SD:/` e usa `sms` como fallback de kernel, separando nome de kernel (`sms.img`) do backend (`sms.smsplus`).
Build SMS-only: a mecanica `make menuconfig`/Kconfig foi removida. O build nao le `.config` nem gera `build/config_build.h`; opcoes restantes ficam em flags diretas como `DEBUG=0|1` e `OWNER=<nome>`. O boot mode e fixo em `NORM` no kernel SMS.
