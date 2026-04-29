# Debug Layer

Guia operacional da layer de debug HDMI/UART do SMSBarePI.

## Estado Atual

- O overlay HDMI permanece ativo em builds `DEBUG=1`.
- O identificador visual canonico continua sendo `TXTDBG BUILD:XXXX`.
- O backend default exibido e `sms.smsplus`.
- Parte da telemetria generica ainda usa campos herdados do framework BarePI e pode retornar zero enquanto a instrumentacao SMS dedicada nao for criada.
- Os logs periodicos UART `SMSAUD` do audio SMS foram removidos apos a correcao do PSG. UART continua disponivel para reinstrumentacao pontual quando o overlay HDMI nao for suficiente.

## Regras

- Desenhar overlay apos o blit do emulador.
- Manter linhas visiveis com ate 64 caracteres.
- Usar UART para telemetria detalhada quando o overlay nao for suficiente.
- Ao adicionar campos SMS reais, atualizar este documento no mesmo ciclo.

## Pendencias

- Substituir campos historicos por telemetria SMS: Z80, VDP, PSG/FM, mapper, ROM carregada e timing por frame.
