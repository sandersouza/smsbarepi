#!/usr/bin/env bash
set -euo pipefail

if [[ "${ALLOW_DIRTY_THIRD_PARTY:-0}" == "1" ]]; then
  echo "[WARN] Guardrail desativado por ALLOW_DIRTY_THIRD_PARTY=1"
  exit 0
fi

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPOS=(
  "$PROJECT_ROOT/third_party/circle"
)

check_repo_clean() {
  local repo="$1"

  if [[ ! -d "$repo" ]]; then
    echo "[ERRO] Repositório third_party não encontrado: $repo"
    return 1
  fi

  local status
  if [[ -d "$repo/.git" ]]; then
    status="$(git -C "$repo" status --porcelain)"
  elif [[ -d "$repo/.gitbak" ]]; then
    status="$(git --git-dir="$repo/.gitbak" --work-tree="$repo" status --porcelain --untracked-files=no)"
  elif [[ -d "$repo/.git.bak" ]]; then
    status="$(git --git-dir="$repo/.git.bak" --work-tree="$repo" status --porcelain --untracked-files=no)"
  else
    echo "[ERRO] Guardrail exige repositório git (.git, .gitbak ou .git.bak) em: $repo"
    return 1
  fi
  if [[ -n "$status" ]]; then
    echo "[ERRO] Alterações locais detectadas em third_party: $repo"
    echo "$status"
    echo
    echo "Restaure o upstream antes de continuar (exemplos):"
    echo "  git -C $repo restore ."
    echo "  git -C $repo clean -fd"
    echo
    echo "Se precisar de patch funcional, replique em camada local fora de third_party (ex.: src/)."
    return 1
  fi

  return 0
}

for repo in "${REPOS[@]}"; do
  check_repo_clean "$repo"
done

echo "[OK] Guardrail: third_party está íntegro (sem alterações locais)."
