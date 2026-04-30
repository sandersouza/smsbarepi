---
description: "Use para build, validacao, empacotamento, SD e entrega de artefatos."
name: "Build and Release"
applyTo:
  - "Makefile"
  - "scripts/**"
  - "README.md"
  - "docs/**"
---
# Build and Release

## Build

- Rodar comandos na raiz do repo.
- Executar iteracoes locais necessarias e analisar erros/warnings antes de orientar proximo passo.
- Em checkpoint estavel, preferir `make clean && make` e validacao exigida pela issue.

## Dependencias vendorizadas

- Antes de targets com Circle/fMSX, rodar `scripts/check-third-party-pristine.sh` ou `make guard-third-party`.
- So usar `ALLOW_DIRTY_THIRD_PARTY=1` com motivo tecnico explicito.
- Nao modificar diretamente `third_party/circle` ou `third_party/fmsx`; usar adaptacao/copia/patch separado.

## Artefatos

- Artefato padrao: `artifacts/kernel8-default.img`.
- Em branch de issue, priorizar `artifacts/kernel8-issue<numero>.img`.
- Ao entregar build, informar `BUILD_HASH:<git>-<sha1>`, `BUILD_BID` e `BUILD_ID` de 4 digitos (`TXTDBG BUILD:XXXX`) quando existirem.

## SD e pacote

- `make sdcard` e o comando oficial para preparar/gravar SD.
- Pedir confirmacao explicita antes de executar `make sdcard`.
- Em `make package`, usar `DEBUG=0|1` e `OWNER=\"<nome>\"`; zip esperado: `artifacts/sdcard-package-<OWNER>.zip`.

## Docs

- Se um novo arquivo for necessario no boot, atualizar o fluxo de preparo de SD na mesma mudanca.
- Atualizar README/docs quando flags, comandos, artefatos ou entrega mudarem.
