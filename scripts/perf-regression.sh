#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_ROOT="$ROOT_DIR/artifacts/benchmarks"
TEMPLATE_PATH="$ROOT_DIR/docs/templates/perf-report-template.md"

usage() {
  cat <<'EOF'
Usage:
  scripts/perf-regression.sh init --tag <name> [--make-target <target>] [--make-flags "<flags>"]
  scripts/perf-regression.sh compare --base <session-dir> --candidate <session-dir>

Commands:
  init       Cria uma sessao de benchmark com build + template de coleta manual.
  compare    Compara metrics.csv de duas sessoes e gera comparison.md.
EOF
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Erro: comando obrigatorio nao encontrado: $1" >&2
    exit 1
  fi
}

timestamp() {
  date +"%Y%m%d-%H%M%S"
}

safe_tag() {
  echo "$1" \
    | tr '[:upper:]' '[:lower:]' \
    | tr -cs 'a-z0-9._-' '-' \
    | sed -E 's/^-+//; s/-+$//'
}

write_default_metrics_csv() {
  local file="$1"
  cat > "$file" <<'EOF'
scenario,target_hz,avg_fps,min_fps,audio_ok,input_ok,boot_ms,notes
boot:norm->basic,60,,,,,,
boot:fast->basic,60,,,,,,
video:wd-des-ov000-sc-des,60,,,,,,
video:wd-lig-ov100-sc-des,60,,,,,,
video:wd-lig-ov100-sc-lcd,60,,,,,,
video:wd-lig-ov100-sc-crt,60,,,,,,
video:wd-des-ov100-sc-crt,60,,,,,,
refresh:50hz-baseline,50,,,,,,
input:keyboard-basic,60,,,,,,
input:joystick-basic,60,,,,,,
audio:mute-off-hdmi,60,,,,,,
audio:mute-on-hdmi,60,,,,,,
EOF
}

init_cmd() {
  require_cmd git
  require_cmd make
  require_cmd stat
  require_cmd shasum

  local tag=""
  local make_target="default"
  local make_flags=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --tag)
        tag="${2:-}"
        shift 2
        ;;
      --make-target)
        make_target="${2:-}"
        shift 2
        ;;
      --make-flags)
        make_flags="${2:-}"
        shift 2
        ;;
      *)
        echo "Parametro invalido: $1" >&2
        usage
        exit 1
        ;;
    esac
  done

  if [[ -z "$tag" ]]; then
    echo "Erro: --tag e obrigatorio no comando init." >&2
    usage
    exit 1
  fi

  local ts
  ts="$(timestamp)"
  local session_name
  session_name="${ts}-$(safe_tag "$tag")"
  local session_dir="$OUT_ROOT/$session_name"
  mkdir -p "$session_dir"

  local build_cmd="make clean && make ${make_target}"
  if [[ -n "$make_flags" ]]; then
    build_cmd+=" ${make_flags}"
  fi

  local build_start
  build_start="$(date +%s)"
  (
    cd "$ROOT_DIR"
    bash -lc "$build_cmd"
  ) >"$session_dir/build.log" 2>&1
  local build_end
  build_end="$(date +%s)"

  local artifact="$ROOT_DIR/artifacts/kernel8-bootstrap.img"
  if [[ "$make_target" == "stable" ]]; then
    artifact="$ROOT_DIR/artifacts/kernel8.img"
  fi
  if [[ ! -f "$artifact" ]]; then
    echo "Erro: artefato nao encontrado apos build: $artifact" >&2
    exit 1
  fi

  local kernel_size
  kernel_size="$(stat -f%z "$artifact")"
  local kernel_sha
  kernel_sha="$(shasum -a 256 "$artifact" | awk '{print $1}')"
  local git_commit
  git_commit="$(cd "$ROOT_DIR" && git rev-parse HEAD)"
  local git_branch
  git_branch="$(cd "$ROOT_DIR" && git branch --show-current)"
  local build_seconds
  build_seconds="$((build_end-build_start))"

  cat > "$session_dir/metadata.env" <<EOF
SESSION_NAME=$session_name
SESSION_TAG=$tag
MAKE_TARGET=$make_target
MAKE_FLAGS=$make_flags
BUILD_SECONDS=$build_seconds
KERNEL_ARTIFACT=$artifact
KERNEL_SIZE_BYTES=$kernel_size
KERNEL_SHA256=$kernel_sha
GIT_BRANCH=$git_branch
GIT_COMMIT=$git_commit
EOF

  cat > "$session_dir/build-command.txt" <<EOF
$build_cmd
EOF

  write_default_metrics_csv "$session_dir/metrics.csv"
  cp "$artifact" "$session_dir/kernel8.img"

  if [[ -f "$TEMPLATE_PATH" ]]; then
    sed \
      -e "s|{{SESSION_NAME}}|$session_name|g" \
      -e "s|{{SESSION_TAG}}|$tag|g" \
      -e "s|{{GIT_BRANCH}}|$git_branch|g" \
      -e "s|{{GIT_COMMIT}}|$git_commit|g" \
      -e "s|{{MAKE_TARGET}}|$make_target|g" \
      -e "s|{{MAKE_FLAGS}}|$make_flags|g" \
      -e "s|{{BUILD_SECONDS}}|$build_seconds|g" \
      -e "s|{{KERNEL_SIZE_BYTES}}|$kernel_size|g" \
      -e "s|{{KERNEL_SHA256}}|$kernel_sha|g" \
      "$TEMPLATE_PATH" > "$session_dir/report.md"
  fi

  echo "Sessao criada: $session_dir"
  echo "Proximo passo: preencher metrics.csv e report.md com dados coletados no hardware."
}

compare_cmd() {
  require_cmd awk

  local base=""
  local candidate=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --base)
        base="${2:-}"
        shift 2
        ;;
      --candidate)
        candidate="${2:-}"
        shift 2
        ;;
      *)
        echo "Parametro invalido: $1" >&2
        usage
        exit 1
        ;;
    esac
  done

  if [[ -z "$base" || -z "$candidate" ]]; then
    echo "Erro: --base e --candidate sao obrigatorios no comando compare." >&2
    usage
    exit 1
  fi

  local base_csv="$base/metrics.csv"
  local cand_csv="$candidate/metrics.csv"
  if [[ ! -f "$base_csv" || ! -f "$cand_csv" ]]; then
    echo "Erro: metrics.csv ausente em base/candidate." >&2
    exit 1
  fi

  local out="$candidate/comparison.md"
  {
    echo "# Comparativo de Performance"
    echo
    echo "- Base: $base"
    echo "- Candidate: $candidate"
    echo
    echo "| Scenario | Base Avg FPS | Candidate Avg FPS | Delta | Base Min FPS | Candidate Min FPS | Delta |"
    echo "|---|---:|---:|---:|---:|---:|---:|"
    awk -F',' '
      NR==FNR {
        if (FNR==1) next;
        base_avg[$1]=$3;
        base_min[$1]=$4;
        next;
      }
      FNR==1 { next; }
      {
        sc=$1;
        cavg=$3;
        cmin=$4;
        bavg=base_avg[sc];
        bmin=base_min[sc];
        davg="-";
        dmin="-";
        if (bavg!="" && cavg!="") davg=sprintf("%.2f", cavg-bavg);
        if (bmin!="" && cmin!="") dmin=sprintf("%.2f", cmin-bmin);
        printf("| %s | %s | %s | %s | %s | %s | %s |\n", sc, bavg, cavg, davg, bmin, cmin, dmin);
      }
    ' "$base_csv" "$cand_csv"
  } > "$out"

  echo "Comparativo gerado: $out"
}

main() {
  if [[ $# -lt 1 ]]; then
    usage
    exit 1
  fi

  local cmd="$1"
  shift
  case "$cmd" in
    init) init_cmd "$@" ;;
    compare) compare_cmd "$@" ;;
    -h|--help|help) usage ;;
    *)
      echo "Comando invalido: $cmd" >&2
      usage
      exit 1
      ;;
  esac
}

main "$@"
