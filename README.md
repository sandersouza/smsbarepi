# SMSBarePI

Base baremetal para Raspberry Pi 3B focada em emulacao Sega SMS sem sistema operacional.

O backend default e `sms.smsplus`, uma porta inicial do SMS Plus GX em `third_party/smsplus` integrada ao framework BarePI. A primeira meta e bootar o core na viewport HDMI, com carregamento de ROMs via menu.

## Build

```sh
make
```

Artefatos principais:

- `artifacts/kernel8-default.img`: artefato estavel gerado pelo build.
- `build/src/sms.img`: imagem com o nome esperado no SD.
- `sdcard/sms.cpio`: initramfs externo quando habilitado.

## SD

O boot do Raspberry Pi deve usar:

- kernel: `sms.img`
- initramfs: `sms.cpio`
- ROMs: `SD:/sms/roms`
- config: `SD:/sms.cfg`
- save/load state: `SD:/sms/slot-last.sav`

Use o fluxo oficial:

```sh
make sdcard
```

## Backend

- Familia: `sms`
- Backend default: `sms.smsplus`
- Referencia upstream: `third_party/smsplus`
- Loader habilitado nesta fase: `Load CART`

Formato aceito pelo navegador de ROMs: `.sms`.

## Estado Atual

- O backend SMS Plus GX inicializa com framebuffer 8bpp interno e converte para RGB565 no contrato BarePI.
- `roms/initramfs/bios.sms` e empacotada em `sms.cpio` e carregada como ROM default de boot quando nenhum cartucho foi carregado.
- Sem cartucho carregado em `sms.cfg`, o core sobe usando a BIOS do initramfs.
- O audio HDMI usa uma implementacao baremetal do PSG SN76489 e habilita o FM/YM2413 do SMS Plus GX para o modo SMS japones.
# smsbarepi
