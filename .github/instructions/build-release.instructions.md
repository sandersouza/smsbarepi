---
description: "Use quando trabalhar em build, validacao, empacotamento e entrega de artefatos no smsbarepi (make, scripts/, Makefile, README)."
name: "SMSBarePI Build and Release"
applyTo:
  - "Makefile"
  - "scripts/**"
  - "README.md"
  - "docs/**"
---
# SMSBarePI Build and Release

## Escopo

- Padronizar iteracoes de build/teste local, artefatos esperados e entrega para teste em hardware.

## Workflow de build

- Confirmar `PWD` em `/Users/sandersouza/Developer/smsbarepi` antes de build/debug.
- Executar iteracoes locais necessarias e analisar erros/warnings antes de orientar proximo passo.
- Em gates de milestone ou antes de avancos maiores, preferir `make clean && make` e validacao em hardware.

## Guardrails de dependencias

- Antes de targets dependentes de Circle/SMS Plus GX, executar `scripts/check-third-party-pristine.sh` ou `make guard-third-party`.
- So flexibilizar com override explicito (`ALLOW_DIRTY_THIRD_PARTY=1`) quando houver motivo tecnico claro.
- Nunca modificar diretamente `third_party/circle` e `third_party/smsplus`; usar copia/patch separado.

## Artefatos e rastreio

- Preservar expectativa de artefatos: `artifacts/kernel8-default.img`.
- Em branch de issue (`<numero>-...`), garantir tambem `artifacts/kernel8-issue<numero>.img` e prioriza-lo para a issue ativa.
- Ao entregar build para teste, informar identificador curto `BUILD_HASH:<git>-<sha1>`.
- Ao entregar build para teste, informar tambem o `BUILD_BID` usado na compilacao.
- Ao entregar build para teste, informar sempre o `BUILD_ID` exato de 4 digitos (`TXTDBG BUILD:XXXX`), mesmo antes do teste em hardware; quando ainda nao houver boot em campo, calcular o valor com o mesmo algoritmo usado no kernel a partir de `SMSBARE_BUILD_BID`.
- Em validacao de hardware, sempre informar tambem o `BUILD_ID` de 4 digitos exibido no overlay (`TXTDBG BUILD:XXXX`), pois esse e o identificador visual canonico em campo.

## SD e empacotamento

- Usar `make sdcard` como comando oficial para preparo/gravação de SD em docs/comandos.
- Evitar orientar `scripts/prepare-sd.sh` diretamente no chat quando o objetivo for gravar SD; preferir sempre `make sdcard`.
- Antes de executar `make sdcard`, pedir confirmacao explicita do usuario no chat.
- Ao finalizar build de teste, sempre incluir o comando `make sdcard`.
- Em `make package`, usar `DEBUG=0|1` e `OWNER=\"<nome>\"`; nome esperado do zip: `artifacts/sdcard-package-<OWNER>.zip`.

## Alinhamento com docs

- Se um novo arquivo for necessario no boot, atualizar `scripts/prepare-sd.sh` na mesma mudanca.
- Atualizar `README.md`/`docs/` sempre que flags de build, comandos ou fluxo de entrega mudarem.
