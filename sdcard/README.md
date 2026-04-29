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
A proporcao de video e configurada em `sms.cfg` por `proportion=16:9` ou `proportion=4:3`;
o runtime aplica essa escolha via framebuffer Circle, sem depender de `config.txt`.
