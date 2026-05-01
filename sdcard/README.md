# SDCard SMSBarePI

Payload base para boot no Raspberry Pi 3B.

Arquivos esperados na particao FAT:

- `config.txt`
- `cmdline.txt`
- `sms.img`
- `sms.cpio` quando o initramfs externo estiver ativo
- `sms.cfg`
- `sms/roms/`
- `sms/slot-last.sav` quando Save State for usado

O `config.txt` deve apontar `kernel=sms.img` e `initramfs sms.cpio followkernel`.
O framebuffer Circle do runtime SMS acompanha a area ativa do frame. `Overscan OFF`,
padrao, usa framebuffer `2.25x` (`256x192` -> `576x432`) com o viewport em `2x`
centralizado. `Overscan ON` usa escala `2x` exata (`256x192` -> `512x384`).
`Scale` e `Proportion` nao sao opcoes de Settings.
