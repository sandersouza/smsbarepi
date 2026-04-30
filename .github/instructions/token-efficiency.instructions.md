---
description: "Use em todas as tarefas para reduzir tokens sem perder rastreabilidade, seguranca de fluxo ou qualidade de validacao."
name: "Token Efficiency"
applyTo: "**"
---
# Token Efficiency

## Regra operacional

- Preferir `rg`, `git diff --name-only`, `git diff --stat` e trechos pequenos de arquivos.
- Revisar diffs/logs localmente; nao colar diffs ou logs grandes no chat.
- Agrupar leituras independentes em paralelo quando possivel.
- Fazer poucas atualizacoes intermediarias: inicio, antes de editar, antes de comando longo e fechamento.
- Evitar repetir contexto historico ja conhecido na thread.

## Docs, GitHub e validacao

- Atualizar docs/GitHub em lote no checkpoint estavel quando isso reduzir ruido.
- Usar validacao em niveis: checks baratos durante iteracao; matriz completa no checkpoint estavel.
- Nao repetir build completo se a mudanca posterior afetou apenas docs, texto ou gestao.

## Resposta final

- Ser curto: arquivos alterados, principais mudancas, validacao, build IDs e paths/links necessarios.
- Omitir logs completos; informar apenas resultado, erro relevante ou identificadores finais.
