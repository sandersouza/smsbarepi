# Contexto Atual

Handoff ativo: `docs/handoff/no-git-smsplus-port.md`

Estado:

- Projeto sem repositorio Git local neste workspace.
- Backend default fixo: `sms.smsplus`.
- Kernel de boot no SD: `sms.img`.
- Initramfs externo: `sms.cpio`.
- Diretorio de ROMs: `SD:/sms/roms`.
- Loader visivel esperado: `Load CART`, abrindo direto o navegador de ROMs SMS.

Ultima validacao local:

```sh
make guard-third-party
NON_INTERACTIVE=1 make syncconfig
make
make backend-module
make initramfs
```

Resultado atual: build concluido, gerando `artifacts/kernel8-default.img`, `build/src/sms.img`, `artifacts/backend/sms/sms.smsplus.mod.elf` e `sdcard/sms.cpio`.
Ultima build: `BUILD_BID=202604282046-nogit`, `TXTDBG BUILD:8F47`, SHA1 `9527c5249ceb1ce6899476474b2c685cc86b9b10` (`DEBUG=1`).
Audio SMS validado em link local com `third_party/smsplus/sound/sound.o` fornecendo `sound_init`, `sound_update`, `psg_write`, `fmunit_write`, `stream_update` e `snd`. O renderer SN76489 e local em `src/backend/sms/modules/smsplus/smsplus_bare_psg.c`, substituindo o objeto `third_party/smsplus/sound/sn76489.o` sem modificar `third_party`; YM2413/FM continua upstream. A ponte SMS usa mixer simples com ganho fixo 16x, `BoostNoise` preservado, clipping 16-bit, ring buffer mono, ganho final SMS em 100% e auto-gain SMS desativado. FM fica sempre ativo no backend SMS; o Settings SMS nao tem item `FM Audio`, o build SMS nao salva nem aplica `fm_audio`/`fm_music`, e os logs periodicos UART `SMSAUD` foram removidos.
Viewport SMS: presets de `Scale` sao `1x` nativo 256x192, `2x` ponto medio entre nativo e fullscreen, e `3x` fullscreen. `Scale` so fica ativo/visivel em `DEBUG=1`; em `DEBUG=0`, o runtime ignora `scale=` persistido e usa sempre o fullscreen maximo permitido pela proporcao. O fullscreen calcula o maior multiplo barato de 1/16x que cabe em largura e altura. Com `proportion=16:9`, o framebuffer Circle e `960x540` e o viewport esperado e ~`560x531`; com `proportion=4:3`, o framebuffer e `640x480` e o viewport esperado e ~`496x470`.
Configuracao SMS-only: scripts e `sms.cfg` default usam `cartridge_path=`; quando vazio, o backend usa `roms/initramfs/bios.sms`. O kernel nao le nem grava espelho raw de `sms.cfg`: settings persistem somente em `SD:/sms.cfg`. `Proportion` persiste como `proportion=16:9` ou `proportion=4:3`, aceita migracao de `resolution=720/768`, e aplica `CComboScreenDevice::Resize()` sem ler/gravar `config.txt`. `SD:/config.txt` permanece separado para `CORE`, que grava apenas o bloco entre `#---KERNEL START---` e `#---KERNEL END---`.
Ao sair de Settings com `ESC`, ou fechar com `F12` enquanto Settings estiver ativo, o kernel agenda `SaveSettingsToStorage()`. Se `CORE` mudou, o fechamento tambem persiste o bloco `KERNEL` e agenda reboot.
Menu `Settings -> CORE`: lista apenas arquivos `*.img` encontrados em `SD:/` e usa `sms` como fallback de kernel, separando nome de kernel (`sms.img`) do backend (`sms.smsplus`).
