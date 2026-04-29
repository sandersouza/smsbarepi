---
description: "Use para diretrizes base de comparação de código, referências técnicas e alinhamento de implementação no smsbarepi."
name: "Code Reference and Implementation Alignment"
applyTo:
  - "src/**"
  - "include/**"
  - "docs/**"
  - "third_party/**"
---

# Code Reference and Implementation Alignment

## Objetivo

- Ter como base, outros códigos de referência, sejam eles de terceiros (ex.: `third_party/smsplus`, `third_party/circle`) ou internos (ex.: `src/`, `include/`, `docs/`), para garantir alinhamento técnico, consistência e evitar retrabalho; usando como base a semântica de código, organização, padrões de implementação e documentação desses códigos de referência, incluindo mas não somente:
  - Estrutura de código e organização de arquivos.
  - Padrões de implementação e estilo de código.
  - Documentação técnica e comentários.
  - Abordagem para resolução de problemas técnicos específicos (ex.: emulação de VDP, gerenciamento de memória, etc.).
  - Código implementado e funcional em outros emuladores SMS, especialmente `third_party/smsplus`, para comportamento de hardware específico como Z80, VDP, PSG/SN76489, YM2413/FM, cartuchos e portas de I/O.

## Diretrizes de implementacao

- Antes de implementar uma funcionalidade ou resolver um problema técnico, consultar os códigos de referência para entender como eles abordam a questão.
- Se uma solução já existe em um código de referência e é aplicável ao contexto do `smsbarepi`, usá-la como base, adaptando-a conforme necessário para o ambiente baremetal e requisitos específicos do projeto.
- Documentar claramente no código e/ou documentação técnica quando uma implementação é inspirada ou baseada em um código de referência, incluindo qual código de referência foi usado e quais partes foram adaptadas ou modificadas.
- Evitar reinventar a roda: se um código de referência já implementa uma funcionalidade de forma eficaz, priorizar o uso dessa implementação em vez de criar uma nova do zero, a menos que haja uma justificativa técnica clara para isso (ex.: requisitos de desempenho, limitações do ambiente baremetal, etc.).
- Manter um alinhamento técnico consistente com os códigos de referência, especialmente em áreas críticas como emulação de hardware, gerenciamento de memória, audio, video e arquitetura de software, para garantir que o projeto se beneficie das melhores práticas e soluções já testadas em emuladores SMS e emuladores baremetal.
- Para funcionalidades específicas de hardware SMS (ex.: Z80, VDP, PSG/SN76489, YM2413/FM, mappers de cartucho e portas de I/O), priorizar a consulta cruzada entre `third_party/smsplus` e referencias tecnicas documentadas antes de criar ajustes ad hoc.

## Diretriz estrategica do backend local

- Tratar o `third_party/smsplus` como referencia semantica primaria para convergencia funcional do backend local SMS.
- Nesta fase do projeto, a migracao completa do backend para SMS Plus GX deve ser incremental; o caminho oficial e aproximar o backend local do comportamento observado no SMS Plus GX, preservando adaptacoes necessarias ao ambiente baremetal.
- Essa regra vale para os subsistemas centrais do emulador, incluindo mas nao somente Z80, VDP, PSG/SN76489, YM2413/FM, memoria, audio, cartuchos e portas de I/O.
- Quando houver divergencia entre o backend local e o SMS Plus GX, priorizar primeiro a auditoria/diff direto contra o SMS Plus GX antes de abrir heuristicas locais ou novos repros ad hoc.
- Usar referencias adicionais disponiveis em `third_party/` para desempate e validacao cruzada quando existirem.
- Ao ajustar o backend local com base nessas referencias, registrar na documentacao tecnica e/ou no handoff:
  - qual comportamento do SMS Plus GX serviu de alvo principal
  - quais referencias secundarias foram usadas para confirmar ou disputar o comportamento
  - quais restricoes arquiteturais do backend local impediram um espelhamento literal da implementacao de referencia
