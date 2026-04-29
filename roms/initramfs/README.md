## ROMs do initramfs SMSBarePI

O initramfs externo padrao e gerado a partir desta pasta e copiado para
`sdcard/sms.cpio` pelo fluxo oficial `make initramfs` ou `make sdcard`.

Arquivo obrigatorio nesta fase:

```text
bios.sms - BIOS/ROM default usada quando nenhum cartucho foi carregado.
```

ROMs extras com extensao `.sms` podem ser empacotadas no initramfs para testes,
mas ROMs de usuario devem ficar preferencialmente em `SD:/sms/roms`.

As ROMs devem ser obtidas de midias ou equipamentos que voce tenha direito de
usar. Depois de alterar arquivos nesta pasta, regenere o payload com:

```sh
make initramfs
```

Ou prepare o SD pelo fluxo completo:

```sh
make sdcard
```
