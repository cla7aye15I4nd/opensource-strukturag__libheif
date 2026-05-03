#!/usr/bin/env bash
# Generate every PoC, run every harness under ASAN, collect output.
#
# Run after `./build.sh`. Outputs go to ./out/.

set -uo pipefail   # no -e: we expect some harnesses to abort on the bug

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN_DIR="${REPO_ROOT}/build-audit-asan/audit_repros"
OUT_DIR="${REPO_ROOT}/audit_repros/out"
mkdir -p "${OUT_DIR}"

if [[ ! -d "${BIN_DIR}" ]]; then
  echo "error: ${BIN_DIR} not found — run build.sh first" >&2
  exit 1
fi

export ASAN_OPTIONS=${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0:halt_on_error=1}
export UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}

for d in "${REPO_ROOT}"/audit_repros/bug*/; do
  name="$(basename "$d")"
  num="${name%%_*}"            # bug01, bug02, ...
  poc="${OUT_DIR}/${name}.input"
  log="${OUT_DIR}/${name}.log"

  echo "==> ${name}"
  python3 "${d}/gen_poc.py" "${poc}" >"${log}" 2>&1

  bin="${BIN_DIR}/${num}"
  if [[ ! -x "${bin}" ]]; then
    echo "    skip: ${bin} not built" | tee -a "${log}"
    continue
  fi

  set +e
  "${bin}" "${poc}" >>"${log}" 2>&1
  rc=$?
  set -e

  if (( rc == 0 )); then
    echo "    rc=0 (no crash — see ${log})"
  else
    echo "    rc=${rc} (crashed/aborted — see ${log})"
  fi
done

echo
echo "Logs: ${OUT_DIR}/"
