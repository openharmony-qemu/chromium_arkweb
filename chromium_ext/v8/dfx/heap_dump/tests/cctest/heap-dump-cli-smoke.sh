#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
V8_ROOT="$(cd "${SCRIPT_DIR}/../../../../../../.." && pwd)"

OUTDIR="out/x64.release"

usage() {
  cat <<EOF
Usage: $0 [--outdir <path>]

Runs smoke tests that:
1. use d8 to dump raw heaps from JS samples
2. use translate to translate those raw heaps into heapsnapshots

The test checks that the commands succeed, produce output files, and that each
translated heapsnapshot has the expected top-level Chrome heap snapshot
sections.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --outdir)
      if [[ $# -lt 2 ]]; then
        echo "[ERROR] --outdir needs a value" >&2
        exit 2
      fi
      OUTDIR="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[ERROR] Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

D8_BIN="${V8_ROOT}/${OUTDIR}/d8"
TRANSLATE_BIN="${V8_ROOT}/${OUTDIR}/translate"

extract_js_flags() {
  local js_file="$1"
  local line flags
  while IFS= read -r line; do
    if [[ "${line}" =~ ^//[[:space:]]Flags:[[:space:]](.*)$ ]]; then
      flags="${BASH_REMATCH[1]}"
      # shellcheck disable=SC2206
      local -a parsed=( ${flags} )
      printf '%s\n' "${parsed[@]}"
    fi
  done < "${V8_ROOT}/${js_file}"
}

validate_translated_heapsnapshot() {
  local snapshot_path="$1"
  if [[ ! -s "${snapshot_path}" ]]; then
    echo "[ERROR] translate did not produce a heapsnapshot file: ${snapshot_path}" >&2
    return 1
  fi

  local required_section
  for required_section in '"snapshot"' '"nodes"' '"edges"' '"strings"'; do
    if ! grep -q "${required_section}" "${snapshot_path}"; then
      echo "[ERROR] translated heapsnapshot is missing ${required_section}: ${snapshot_path}" >&2
      return 1
    fi
  done
}

mapfile -t JS_SAMPLES < <(
  cd "${V8_ROOT}" && find arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit -maxdepth 1 -type f \
    \( -name '*.js' -o -name '*.mjs' \) | sort
)

if [[ ! -x "${D8_BIN}" ]]; then
  echo "[ERROR] d8 binary not found: ${D8_BIN}" >&2
  exit 2
fi

if [[ ! -x "${TRANSLATE_BIN}" ]]; then
  echo "[ERROR] translate binary not found: ${TRANSLATE_BIN}" >&2
  exit 2
fi

if [[ ${#JS_SAMPLES[@]} -eq 0 ]]; then
  echo "[ERROR] no heap-dump mjsunit samples found" >&2
  exit 2
fi

TMP_ROOT="${V8_ROOT}/tmp"
mkdir -p "${TMP_ROOT}"
export TMPDIR="${TMP_ROOT}"
TMP_DIR="$(mktemp -d "${TMP_ROOT}/heap-dump-cli-smoke.XXXXXX")"
REPO_ROOT_TEMP_BASELINE_FILE="${TMP_DIR}/repo-root-temp-baseline.txt"
REPO_ROOT_TEMP_AFTER_FILE="${TMP_DIR}/repo-root-temp-after.txt"
REPO_ROOT_TEMP_NEW_FILE="${TMP_DIR}/repo-root-temp-new.txt"

cleanup() {
  rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

snapshot_repo_root_temp_artifacts() {
  find "${V8_ROOT}" -maxdepth 1 \
    \( -name 'v8.log' -o -name '*.rawheap' -o -name '*.heapsnapshot' \
       -o -name 'siso*' \) \
    -printf '%f\t%s\t%T@\n' | sort -u
}

validate_no_new_repo_root_temp_artifacts() {
  snapshot_repo_root_temp_artifacts > "${REPO_ROOT_TEMP_AFTER_FILE}"
  comm -13 "${REPO_ROOT_TEMP_BASELINE_FILE}" "${REPO_ROOT_TEMP_AFTER_FILE}" \
    > "${REPO_ROOT_TEMP_NEW_FILE}"
  new_count="$(wc -l < "${REPO_ROOT_TEMP_NEW_FILE}")"
  if [[ "${new_count}" -ne 0 ]]; then
    echo "[ERROR] heap-dump CLI smoke wrote repo-root temp artifacts:" >&2
    cat "${REPO_ROOT_TEMP_NEW_FILE}" >&2
    return 1
  fi
  echo "[PASS] no new repo-root heap-dump temp artifacts"
}

snapshot_repo_root_temp_artifacts > "${REPO_ROOT_TEMP_BASELINE_FILE}"

cd "${V8_ROOT}" || exit 1

case_index=0
for js_sample in "${JS_SAMPLES[@]}"; do
  ((case_index += 1))
  sample_name="$(basename "${js_sample}")"
  printf -v sample_dir "%s/case_%03d_%s" "${TMP_DIR}" "${case_index}" \
    "${sample_name}"
  mkdir -p "${sample_dir}"
  rawheap_path="${sample_dir}/sample.rawheap"
  snapshot_path="${sample_dir}/sample.heapsnapshot"
  logfile_path="${sample_dir}/sample.v8.log"
  mapfile -t js_flags < <(extract_js_flags "${js_sample}")

  "${D8_BIN}" "${js_flags[@]}" --logfile="${logfile_path}" \
    --heapdump-out "${rawheap_path}" test/mjsunit/mjsunit.js "${js_sample}"
  if [[ ! -s "${rawheap_path}" ]]; then
    echo "[ERROR] d8 did not produce a rawheap file: ${rawheap_path}" >&2
    exit 1
  fi

  "${TRANSLATE_BIN}" --translate "${rawheap_path}" \
    --translate-out "${snapshot_path}"
  if ! validate_translated_heapsnapshot "${snapshot_path}"; then
    exit 1
  fi
  echo "[PASS] ${js_sample}"
done

validate_no_new_repo_root_temp_artifacts
echo "[PASS] d8 dump -> translate smoke"
