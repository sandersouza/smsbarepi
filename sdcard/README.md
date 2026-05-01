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
A proporcao de video do runtime SMS e fixa em 4:3 (`640x480`) via framebuffer Circle,
sem depender de `sms.cfg` ou `config.txt`.
