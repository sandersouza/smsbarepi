# Scripts do Projeto

Este diretorio concentra utilitarios operacionais de build, validacao de
ambiente, preparo de SD e empacotamento do SMSBarePI.

## Resumo rapido

- `build-config.sh`: build assistido do artefato default.
- `install-toolchains.sh`: instala/valida dependencias de toolchain por SO.
- `check-toolchain.sh`: checa toolchain AArch64.
- `check-third-party-pristine.sh`: garante que `third_party/circle` e `third_party/smsplus` estejam limpos.
- `make-initramfs.sh`: gera `sms.cpio` a partir de `roms/initramfs`.
- `make-sdcard-package.sh`: exporta um pacote ZIP de SD.
- `prepare-sd.sh`: fluxo oficial de preparo de SD com formatacao.
- `update-sd-incremental.sh`: atualizacao incremental de SD sem formatar.
- `perf-regression.sh`: suite local de benchmark/regressao.
- `logserial.sh`: apoio para leitura serial/UART.

## Pre-requisitos gerais

- Shell Bash.
- Toolchain AArch64 (`aarch64-none-elf-*`, `aarch64-elf-*` ou `aarch64-linux-gnu-*`).
- Para preparo de SD: macOS com `diskutil`/`plutil`, ou Linux com `lsblk`; para formatacao no Linux, `parted`, `mkfs.vfat` e `udisksctl`.
- `third_party/circle` e `third_party/smsplus` presentes e sem alteracoes locais.

## Build

Build default:

```sh
make
```

Fluxo assistido:

```sh
./scripts/build-config.sh
```

Modo nao interativo:

```sh
NON_INTERACTIVE=1 ./scripts/build-config.sh
```

Antes de targets que dependem de Circle/SMS Plus GX, rode:

```sh
make guard-third-party
```

O backend SMS e fixo em `sms.smsplus`. Opcoes de build suportadas ficam na
linha de comando, por exemplo `make DEBUG=0` ou `make OWNER=<nome>`.

## Initramfs

Gerar `sdcard/sms.cpio`:

```sh
make initramfs
```

Ou diretamente:

```sh
./scripts/make-initramfs.sh roms/initramfs sdcard/sms.cpio
```

O diretorio de entrada deve conter `bios.sms`; ROMs extras `.sms` tambem sao
incluidas quando presentes.

## SD e pacote

Preparar SD pelo fluxo oficial:

```sh
make sdcard
```

Esse target formata/grava midia e deve ser executado apenas com confirmacao
explicita.

Gerar pacote ZIP:

```sh
make package
```

Arquivos canonicos no SD:

- kernel: `sms.img`
- initramfs: `sms.cpio`
- configuracao: `sms.cfg`
- ROMs de usuario: `SD:/sms/roms`

O `sms.cfg` default gerado pelos scripts usa `cartridge_path=` para o cartucho
selecionado. Quando vazio, o backend sobe com `bios.sms` do initramfs.
