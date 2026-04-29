# kconfig-linux (mconf frontend)

Copia local do frontend Kconfig do Linux usada para fornecer `make menuconfig` com a mesma interface classica do kernel.

## Origem upstream

- Repositorio: https://github.com/torvalds/linux
- Caminho: `scripts/kconfig/`
- Snapshot base usado nesta integracao: branch `master` (commit local registrado no handoff da issue #177)
- Licenca: GPL-2.0 (arquivo `LICENSE.GPL-2.0`)

## Uso no projeto

- Build do frontend: `scripts/build-kconfig-mconf.sh`
- Binario gerado: `build/tools/kconfig-linux/mconf`
- Spec de configuracao da plataforma: `config/Kconfig.smsbarepi`
- Entrada principal: `scripts/menuconfig.sh`

## Observacoes

- O fluxo gera `.config` no formato Kconfig e converte para `.config.mk` + `build/config.mk` para consumo no `Makefile`.
- O parser/lexer (`parser.y`, `lexer.l`) sao gerados localmente com `bison` e `flex` durante o build do `mconf`.
