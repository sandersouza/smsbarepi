# third_party

Esta pasta reune referencias externas usadas pelo CoCoBarePI.

## Conteudo esperado

- `third_party/circle`
  - ambiente baremetal para Raspberry Pi.
  - usado para boot, framebuffer, audio, USB, SD/FAT e infraestrutura de baixo nivel.
- `third_party/VCC`
  - referencia semantica primaria para emulacao TRS-80 Color Computer.
  - deve ser consultado para CPU `MC6809/HD6309`, GIME, SAM, PIA, audio, cartuchos e disco.

## Como popular

```bash
git clone --depth 1 https://github.com/rsta2/circle.git third_party/circle
git clone --depth 1 https://github.com/VCCE/VCC.git third_party/VCC
```

## Guardrail

Nao modificar diretamente `third_party/circle` nem `third_party/VCC` durante a integracao.

Adapte codigo em `src/backend/coco/` e documente a referencia usada quando uma implementacao for baseada em trechos ou comportamento de terceiros.
