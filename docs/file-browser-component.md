# File Browser

O navegador de arquivos do SMSBarePI usa apenas o modo de ROM nesta fase.

## Root

- ROMs: `SD:/sms/roms`

## Extensoes

- `.sms`

Diretorios continuam listados antes de arquivos, entradas iniciadas por `.` sao ignoradas e `..` permite retornar ao diretorio anterior sem sair do root.

O initramfs de boot usa `roms/initramfs/bios.sms` como ROM default; ROMs de usuario continuam em `SD:/sms/roms`.
