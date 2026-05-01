# Handoff: SMS Plus GX Port

Data: 2026-04-26

## Estado

- Workspace com repositorio Git local na branch `main`.
- Porta inicial do SMS Plus GX criada em `src/backend/sms/`.
- Backend integrado ao registry BarePI como `sms.smsplus`.
- Build local passa com `DEBUG=1` no corte SMS-only.

## Decisoes

- O primeiro corte usa framebuffer interno indexado 8bpp do SMS Plus GX e converte para RGB565 antes do blit BarePI.
- ROM loader implementado por FatFs em `SD:/sms/roms`, aceitando apenas `.sms`; no menu SMS o item `Load CART` abre direto o navegador e carrega sempre no slot unico do SMS.
- `roms/initramfs/bios.sms` e a ROM default de boot via `sms.cpio`; quando nenhum cartucho foi carregado, o backend roda a BIOS.
- Save/Load State usa `SD:/sms/slot-last.sav`.
- Sem cartucho carregado, o backend carrega `bios.sms` do initramfs como BIOS default.
- Viewport SMS usa framebuffer Circle dinamico. `Overscan OFF` e o default e usa framebuffer `2.25x` (`256x192` -> `576x432`) com o viewport do emulador em escala exata `2x` centralizada. `Overscan ON` usa framebuffer `2x` da area ativa (`256x192` -> `512x384`). O Settings nao exibe `Scale` nem `Proportion`.
- Audio HDMI usa o pipeline upstream de geracao do SMS Plus GX para `sound.c`, `fmintf.c`, `emu2413.c` e `ym2413.c`. O SN76489 foi substituido no link por `src/backend/sms/modules/smsplus/smsplus_bare_psg.c`, preservando a API/contexto do SMS Plus GX e removendo `third_party/smsplus/sound/sn76489.o` dos objetos SMS sem alterar `third_party`.
- A ponte BarePI permanece no baseline de mix que estava mais proximo do correto: soma PSG+FM com ganho fixo 16x, clipping 16-bit, `BoostNoise` preservado, ganho final SMS em 100%, auto-gain do kernel desativado para SMS e ring buffer mono antes do HDMI.
- FM/YM2413 fica sempre ativo no backend SMS, configurando o core como SMS japones/domestic. O Settings SMS nao exibe mais `FM Audio`, e o build SMS nao salva nem aplica `fm_audio`/`fm_music` de configuracoes antigas.
- Logs periodicos UART de audio SMS com prefixo `SMSAUD` foram removidos. O audio permanece observavel pelo comportamento em hardware e pela telemetria agregada ja existente do backend; reinstrumentar UART apenas em nova investigacao pontual.
- Buffer de ROM limitado a 1 MB para manter o footprint dentro da janela atual do kernel.
- `sms.cfg` agora e lido/gravado somente via FatFs em `SD:/sms.cfg`; o espelho raw foi removido. Escala e proporcao nao sao configuraveis: `scale=`, `scale_max=`, `proportion=` e `resolution=` de configs antigas sao ignorados. `overscan=0|1` controla apenas o padding do framebuffer, e o kernel aplica `CComboScreenDevice::Resize()` sem ler nem gravar `SD:/config.txt`.
- `sms.cfg` nao persiste midia: ROMs, discos e cassetes nao sao salvos nem restaurados automaticamente no boot. `Load CART` continua carregando ROM manualmente pelo menu; sem cartucho carregado, o backend usa `roms/initramfs/bios.sms`.
- O fluxo de `CORE` persiste somente o bloco `#---KERNEL START---`/`#---KERNEL END---` do `config.txt`; sem esses marcadores, a troca de core falha sem regravar o arquivo.
- O menu `Settings -> CORE` enumera somente `*.img` presentes em `SD:/`; o fallback interno de kernel e `sms`, nao o id de backend `smsplus`.
- Ao sair de Settings, o kernel executa `HandleSettingsMenuClosed()` e agenda `SaveSettingsToStorage()`. Se `CORE` mudou, o mesmo fechamento tambem persiste `SD:/config.txt` e agenda reboot. Sair direto do pause menu com Settings ativo continua usando o mesmo fechamento.
- A mecanica `make menuconfig`/Kconfig foi removida do build SMS-only. O `Makefile` nao le `.config`, nao gera `build/config_build.h`, e o kernel nao usa flags de boot mode/feature enable vindas de Kconfig; o boot mode SMS permanece fixo em `NORM`. As opcoes de build restantes sao flags diretas como `DEBUG=0|1` e `OWNER=<nome>`.
- `Settings -> Joypad Mapping` prioriza o edge de `Enter/Select` antes do tratamento generico de `Back`, evitando que controles HID que reportam os dois sinais no mesmo frame retornem ao menu principal em vez de abrir o submenu.
- Teclado USB tambem alimenta a ponte de joystick em runtime: setas cursoras viram direcional, `Space` vira Botao 2 e `M` vira Botao 1.
- `Settings -> Autofire` controla a autorepeticao do Botao 2, persistida como `autofire=0|1` em `SD:/sms.cfg`; o primeiro teste usa 10 repeticoes por segundo.

## Validacao

```sh
scripts/check-third-party-pristine.sh
git diff --check
bash -n scripts/prepare-sd.sh scripts/make-sdcard-package.sh scripts/update-sd-incremental.sh
make -j4 DEBUG=1
make -j4 DEBUG=0
make -n sdcard
# 2026-04-30: make -j4 DEBUG=1 apos correcao de Joypad Mapping
# 2026-04-30: matriz completa apos framebuffer 2x dinamico e remocao de Scale
```

Resultado atual:

- `artifacts/kernel8-default.img`
- `build/src/sms.img`
- Ultima build local: `BUILD_BID=202604302332-11ed60d`, `TXTDBG BUILD:17BE`, SHA1 `91c2bf5f23ed1e6fae7c97ac71fa8f2ec98d5f92` (`DEBUG=0`).

## Pendencias

- Validar em hardware Raspberry Pi 3B.
- Testar `Load CART` com ROM real em `SD:/sms/roms`.
- Validar em hardware Raspberry Pi 3B o audio da BIOS EU REV1/SEGA logo e `SHINOBI.SMS` com FM sempre ativo.
