#!/usr/bin/env bash

set -u
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
V8_ROOT="$(cd "${SCRIPT_DIR}/../../../../../../.." && pwd)"

OUTDIR="out/x64.release"
RUN_BUILD=1
RUN_CCTEST=1
RUN_MJSUNIT=1
VERBOSE=0
BUILD_JOBS="${HEAP_DUMP_BUILD_JOBS:-}"

min_positive_integer() {
  local result=""
  local value
  for value in "$@"; do
    if [[ "${value}" =~ ^[1-9][0-9]*$ ]]; then
      if [[ -z "${result}" || "${value}" -lt "${result}" ]]; then
        result="${value}"
      fi
    fi
  done
  if [[ -z "${result}" ]]; then
    result=1
  fi
  echo "${result}"
}

detect_cpuset_cpus() {
  local file value count part start end
  local -a parts
  for file in /sys/fs/cgroup/cpuset.cpus.effective \
              /sys/fs/cgroup/cpuset/cpuset.cpus.effective \
              /sys/fs/cgroup/cpuset/cpuset.cpus; do
    if [[ -r "${file}" ]]; then
      value="$(<"${file}")"
      if [[ -n "${value}" ]]; then
        count=0
        IFS=',' read -ra parts <<< "${value}"
        for part in "${parts[@]}"; do
          if [[ "${part}" =~ ^([0-9]+)-([0-9]+)$ ]]; then
            start="${BASH_REMATCH[1]}"
            end="${BASH_REMATCH[2]}"
            if [[ "${end}" -ge "${start}" ]]; then
              count=$((count + end - start + 1))
            fi
          elif [[ "${part}" =~ ^[0-9]+$ ]]; then
            count=$((count + 1))
          fi
        done
        if [[ "${count}" -gt 0 ]]; then
          echo "${count}"
          return
        fi
      fi
    fi
  done
}

detect_cgroup_cpu_quota() {
  local quota period cpus
  if [[ -r /sys/fs/cgroup/cpu.max ]]; then
    read -r quota period < /sys/fs/cgroup/cpu.max
    if [[ "${quota}" != "max" && "${quota}" =~ ^[0-9]+$ && \
          "${period}" =~ ^[1-9][0-9]*$ ]]; then
      cpus=$(((quota + period - 1) / period))
      if [[ "${cpus}" -gt 0 ]]; then
        echo "${cpus}"
        return
      fi
    fi
  fi
  if [[ -r /sys/fs/cgroup/cpu/cpu.cfs_quota_us && \
        -r /sys/fs/cgroup/cpu/cpu.cfs_period_us ]]; then
    quota="$(< /sys/fs/cgroup/cpu/cpu.cfs_quota_us)"
    period="$(< /sys/fs/cgroup/cpu/cpu.cfs_period_us)"
    if [[ "${quota}" =~ ^[0-9]+$ && "${quota}" -gt 0 && \
          "${period}" =~ ^[1-9][0-9]*$ ]]; then
      cpus=$(((quota + period - 1) / period))
      if [[ "${cpus}" -gt 0 ]]; then
        echo "${cpus}"
      fi
    fi
  fi
}

detect_memory_limited_jobs() {
  local bytes kb jobs file value
  for file in /sys/fs/cgroup/memory.max \
              /sys/fs/cgroup/memory/memory.limit_in_bytes; do
    if [[ -r "${file}" ]]; then
      value="$(<"${file}")"
      if [[ "${value}" =~ ^[0-9]+$ && "${value}" -gt 0 && \
            "${value}" -lt 9223372036854771712 ]]; then
        bytes="${value}"
        # Conservative local compile/link heuristic for WSL: about 2 GiB/job.
        jobs=$((bytes / 2147483648))
        if [[ "${jobs}" -lt 1 ]]; then
          jobs=1
        fi
        echo "${jobs}"
        return
      fi
    fi
  done
  if [[ -r /proc/meminfo ]]; then
    kb="$(awk '/^MemAvailable:/ {print $2; exit}' /proc/meminfo)"
    if [[ "${kb}" =~ ^[0-9]+$ && "${kb}" -gt 0 ]]; then
      jobs=$((kb / 2097152))
      if [[ "${jobs}" -lt 1 ]]; then
        jobs=1
      fi
      echo "${jobs}"
    fi
  fi
}

detect_default_build_jobs() {
  local cores quota_cpus cpuset_cpus memory_jobs jobs
  if command -v nproc >/dev/null 2>&1; then
    cores="$(nproc)"
  else
    cores="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
  fi
  if ! [[ "${cores}" =~ ^[0-9]+$ ]] || [[ "${cores}" -lt 1 ]]; then
    cores=1
  fi

  quota_cpus="$(detect_cgroup_cpu_quota || true)"
  cpuset_cpus="$(detect_cpuset_cpus || true)"
  memory_jobs="$(detect_memory_limited_jobs || true)"
  jobs="$(min_positive_integer "${cores}" "${quota_cpus}" "${cpuset_cpus}" \
                                "${memory_jobs}")"
  echo "${jobs}"
}

is_positive_integer() {
  [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

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

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --outdir <path>   Build output dir relative to v8 root (default: out/x64.release)
  --jobs <n>        Build parallelism for Siso/Ninja (default: environment-
                    adaptive CPU/memory limit; can also be set with
                    HEAP_DUMP_BUILD_JOBS)
  --no-build        Skip build step
  --cctest-only     Run only heap-dump cctest cases
  --mjsunit-only    Run only heap-dump mjsunit cases
  --expected-candidate-count <n>
                    Assert the candidate InstanceType count equals n.
                    By default the runner records the direct count without
                    enforcing a config-specific denominator.
  --verbose         Print test command output directly
  -h, --help        Show this help
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
    --jobs)
      if [[ $# -lt 2 ]]; then
        echo "[ERROR] --jobs needs a value" >&2
        exit 2
      fi
      BUILD_JOBS="$2"
      shift 2
      ;;
    --no-build)
      RUN_BUILD=0
      shift
      ;;
    --cctest-only)
      RUN_CCTEST=1
      RUN_MJSUNIT=0
      shift
      ;;
    --mjsunit-only)
      RUN_CCTEST=0
      RUN_MJSUNIT=1
      shift
      ;;
    --expected-candidate-count)
      if [[ $# -lt 2 ]]; then
        echo "[ERROR] --expected-candidate-count needs a value" >&2
        exit 2
      fi
      EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT="$2"
      shift 2
      ;;
    --verbose)
      VERBOSE=1
      shift
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

if [[ $RUN_CCTEST -eq 0 && $RUN_MJSUNIT -eq 0 ]]; then
  echo "[ERROR] Nothing to run." >&2
  exit 2
fi

if [[ $RUN_BUILD -eq 1 ]]; then
  if [[ -z "${BUILD_JOBS}" ]]; then
    BUILD_JOBS="$(detect_default_build_jobs)"
  fi
  if ! is_positive_integer "${BUILD_JOBS}"; then
    echo "[ERROR] --jobs/HEAP_DUMP_BUILD_JOBS must be a positive integer: ${BUILD_JOBS}" >&2
    exit 2
  fi
fi

OUT_ABS="${V8_ROOT}/${OUTDIR}"
CCTEST_BIN="${OUT_ABS}/cctest"
D8_BIN="${OUT_ABS}/d8"
TRANSLATE_BIN="${OUT_ABS}/translate"
GN_BIN="${GN:-}"
if [[ -z "${GN_BIN}" ]]; then
  case "$(uname -s)" in
    Darwin)
      if [[ -x "${V8_ROOT}/buildtools/mac/gn" ]]; then
        GN_BIN="${V8_ROOT}/buildtools/mac/gn"
      fi
      ;;
    Linux)
      if [[ -x "${V8_ROOT}/buildtools/linux64/gn" ]]; then
        GN_BIN="${V8_ROOT}/buildtools/linux64/gn"
      fi
      ;;
  esac
  if [[ -z "${GN_BIN}" && -x "${V8_ROOT}/buildtools/linux64/gn" ]]; then
    GN_BIN="${V8_ROOT}/buildtools/linux64/gn"
  elif [[ -z "${GN_BIN}" && -x "${V8_ROOT}/buildtools/mac/gn" ]]; then
    GN_BIN="${V8_ROOT}/buildtools/mac/gn"
  elif command -v gn >/dev/null 2>&1; then
    GN_BIN="gn"
  fi
fi

if [[ ! -d "${OUT_ABS}" ]]; then
  echo "[ERROR] Output directory not found: ${OUT_ABS}" >&2
  exit 2
fi

TMP_ROOT="${V8_ROOT}/tmp"
mkdir -p "${TMP_ROOT}"
export TMPDIR="${TMP_ROOT}"
LOG_DIR="$(mktemp -d "${TMP_ROOT}/heap-dump-tests.XXXXXX")"
SUMMARY_FILE="${LOG_DIR}/summary.txt"
REPO_ROOT_TEMP_BASELINE_FILE="${LOG_DIR}/repo-root-temp-baseline.txt"
REPO_ROOT_TEMP_AFTER_FILE="${LOG_DIR}/repo-root-temp-after.txt"
REPO_ROOT_TEMP_NEW_FILE="${LOG_DIR}/repo-root-temp-new.txt"

snapshot_repo_root_temp_artifacts() {
  find "${V8_ROOT}" -maxdepth 1 \
    \( -name 'v8.log' -o -name '*.rawheap' -o -name '*.heapsnapshot' \
       -o -name 'siso*' \) \
    -printf '%f\t%s\t%T@\n' | sort -u
}

snapshot_repo_root_temp_artifacts >"${REPO_ROOT_TEMP_BASELINE_FILE}"

SISO_BIN="${SISO:-}"
if [[ -z "${SISO_BIN}" && -x "${V8_ROOT}/third_party/siso/siso" ]]; then
  SISO_BIN="${V8_ROOT}/third_party/siso/siso"
fi

NINJA_BIN="${NINJA:-}"
if [[ -z "${NINJA_BIN}" ]]; then
  if [[ -x "${V8_ROOT}/third_party/ninja/ninja" ]]; then
    NINJA_BIN="${V8_ROOT}/third_party/ninja/ninja"
  elif command -v ninja >/dev/null 2>&1; then
    NINJA_BIN="ninja"
  fi
fi

AUTONINJA_BIN="${AUTONINJA:-}"
if [[ -z "${AUTONINJA_BIN}" ]]; then
  if command -v autoninja >/dev/null 2>&1; then
    AUTONINJA_BIN="autoninja"
  elif [[ -x "${V8_ROOT}/../depot_tools/autoninja" ]]; then
    AUTONINJA_BIN="${V8_ROOT}/../depot_tools/autoninja"
  elif [[ -x "/home/hh/workspace/depot_tools/autoninja" ]]; then
    AUTONINJA_BIN="/home/hh/workspace/depot_tools/autoninja"
  fi
fi

if [[ $RUN_BUILD -eq 1 ]]; then
  if [[ -n "${SISO_BIN}" && -f "${OUT_ABS}/siso_metadata.json" ]]; then
    echo "[BUILD] ${SISO_BIN} ninja -C ${OUTDIR} -offline -local_jobs ${BUILD_JOBS} cctest d8 translate"
    if ! "${SISO_BIN}" ninja -C "${OUT_ABS}" -offline \
        -local_jobs "${BUILD_JOBS}" cctest d8 translate; then
      echo "[FAIL] Build failed" >&2
      exit 1
    fi
  elif [[ -n "${NINJA_BIN}" ]]; then
    echo "[BUILD] ${NINJA_BIN} -C ${OUTDIR} -j${BUILD_JOBS} cctest d8 translate"
    if ! "${NINJA_BIN}" -C "${OUT_ABS}" "-j${BUILD_JOBS}" \
        cctest d8 translate; then
      echo "[FAIL] Build failed" >&2
      exit 1
    fi
  elif [[ -n "${AUTONINJA_BIN}" ]]; then
    echo "[BUILD] ${AUTONINJA_BIN} -C ${OUTDIR} -j${BUILD_JOBS} cctest d8 translate"
    if ! "${AUTONINJA_BIN}" -C "${OUT_ABS}" "-j${BUILD_JOBS}" \
        cctest d8 translate; then
      echo "[FAIL] Build failed" >&2
      exit 1
    fi
  else
    echo "[ERROR] No build tool found. Set SISO, NINJA, AUTONINJA, or install depot_tools." >&2
    exit 2
  fi
  echo "[PASS] Build finished"
fi

if [[ $RUN_CCTEST -eq 1 && ! -x "${CCTEST_BIN}" ]]; then
  echo "[ERROR] cctest binary not found: ${CCTEST_BIN}" >&2
  exit 2
fi
if [[ $RUN_MJSUNIT -eq 1 && ! -x "${D8_BIN}" ]]; then
  echo "[ERROR] d8 binary not found: ${D8_BIN}" >&2
  exit 2
fi
if [[ $RUN_MJSUNIT -eq 1 && ! -x "${TRANSLATE_BIN}" ]]; then
  echo "[ERROR] translate binary not found: ${TRANSLATE_BIN}" >&2
  exit 2
fi

# Keep this list in sync with heap-dump tests registered in cctest --list.
declare -a CCTESTS=(
  "test-heap-dump-basic/HeapDumpBasicEndToEnd"
  "test-heap-dump-basic/HeapDumpTranslateRecognizesRepresentativeNodeKinds"
  "test-heap-dump-basic/HeapDumpTranslateForeignUsesSystemForeignFallback"
  "test-heap-dump-basic/HeapDumpSnapshotAlignedJsObjectGraphBaseline"
  "test-heap-dump-basic/HeapDumpSnapshotAlignedPersistentHandleBaseline"
  "test-heap-dump-basic/HeapDumpSnapshotAlignedObjectShapeWhiteBoxBaseline"
  "test-heap-dump-basic/HeapDumpRootAndGlobalObjectDescriptions"
  "test-heap-dump-basic/HeapDumpExactByteArrayType"
  "test-heap-dump-basic/HeapDumpExactArrayListType"
  "test-heap-dump-basic/HeapDumpExactCellType"
  "test-heap-dump-basic/HeapDumpCellExactStrongValueEdge"
  "test-heap-dump-basic/HeapDumpCellExactWeakValueEdge"
  "test-heap-dump-basic/HeapDumpExactClosureFeedbackCellArrayType"
  "test-heap-dump-basic/HeapDumpExactContextCellType"
  "test-heap-dump-basic/HeapDumpExactFeedbackCellType"
  "test-heap-dump-basic/HeapDumpFeedbackCellExactValueEdge"
  "test-heap-dump-basic/HeapDumpExactAllocationSiteType"
  "test-heap-dump-basic/HeapDumpAllocationSiteExactTransitionInfoEdge"
  "test-heap-dump-basic/HeapDumpAllocationSiteExactNestedSiteEdge"
  "test-heap-dump-basic/HeapDumpAllocationSiteExactDependentCodeEdge"
  "test-heap-dump-basic/HeapDumpAllocationSiteWeakNextMatchesNativeEdges"
  "test-heap-dump-basic/HeapDumpNativeContextRootShortcutParity"
  "test-heap-dump-basic/HeapDumpPromiseFulfillReactionJobTaskExactMapEdge"
  "test-heap-dump-basic/HeapDumpPromiseRejectReactionJobTaskExactMapEdge"
  "test-heap-dump-basic/HeapDumpCallableTaskExactMapEdge"
  "test-heap-dump-basic/HeapDumpCallbackTaskExactMapEdge"
  "test-heap-dump-basic/HeapDumpPromiseResolveThenableJobTaskExactMapEdge"
  "test-heap-dump-basic/HeapDumpLoadHandlerExactMapEdge"
  "test-heap-dump-basic/HeapDumpStoreHandlerExactMapEdge"
  "test-heap-dump-basic/HeapDumpAccessCheckInfoExactMapEdge"
  "test-heap-dump-basic/HeapDumpAsyncGeneratorRequestExactMapEdge"
  "test-heap-dump-basic/HeapDumpBreakPointExactMapEdge"
  "test-heap-dump-basic/HeapDumpBreakPointInfoExactMapEdge"
  "test-heap-dump-basic/HeapDumpDebugInfoExactMapEdge"
  "test-heap-dump-basic/HeapDumpFunctionTemplateRareDataExactMapEdge"
  "test-heap-dump-basic/HeapDumpTemplateObjectDescriptionExactMapEdge"
  "test-heap-dump-basic/HeapDumpTuple2ExactMapEdge"
  "test-heap-dump-basic/HeapDumpHoleExactMapEdge"
  "test-heap-dump-basic/HeapDumpUncompiledDataExactMapEdges"
  "test-heap-dump-basic/HeapDumpCodeExactMapEdge"
  "test-heap-dump-basic/HeapDumpInterpreterDataExactMapEdge"
  "test-heap-dump-basic/HeapDumpOnHeapBasicBlockProfilerDataExactMapEdge"
  "test-heap-dump-basic/HeapDumpBytecodeWrapperExactMapEdge"
  "test-heap-dump-basic/HeapDumpCodeWrapperExactMapEdge"
  "test-heap-dump-basic/HeapDumpRegExpDataWrapperExactMapEdge"
  "test-heap-dump-basic/HeapDumpSharedFunctionInfoWrapperExactMapEdge"
  "test-heap-dump-basic/HeapDumpProtectedFixedArrayExactMapEdge"
  "test-heap-dump-basic/HeapDumpProtectedWeakFixedArrayExactMapEdge"
  "test-heap-dump-basic/HeapDumpTrustedByteArrayExactMapEdge"
  "test-heap-dump-basic/HeapDumpTrustedFixedArrayExactMapEdge"
  "test-heap-dump-basic/HeapDumpTrustedWeakFixedArrayExactMapEdge"
  "test-heap-dump-basic/HeapDumpEmbedderDataArrayExactMapEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSObjectPrototypeExactEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSObjectOwnDataPropertyEdges"
  "test-heap-dump-js-objects-core/HeapDumpJSObjectDescriptorDataPropertyEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSObjectOwnAccessorPropertyEdges"
  "test-heap-dump-js-objects-core/HeapDumpJSObjectSwissDictionaryPropertyEdges"
  "test-heap-dump-js-objects-core/HeapDumpObjectPrototypeClassicUsageEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSProxyExactTargetAndHandlerEdges"
  "test-heap-dump-js-objects-core/HeapDumpJSApiObjectInternalFieldExactEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSLastDummyApiObjectExactMapEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSSpecialApiObjectExactMapEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSExternalObjectExactMapEdge"
  "test-heap-dump-js-objects-core/HeapDumpJSObjectWithEmbedderSlotsExactInternalFieldEdges"
  "test-heap-dump-js-objects-core/HeapDumpHeapNumberExactMapEdge"
  "test-heap-dump-js-objects-core/HeapDumpBigIntExactMapEdge"
  "test-heap-dump-js-objects-core/HeapDumpOddballExactRootValueEdges"
  "test-heap-dump-js-objects-core/HeapDumpJSSharedArrayAndStructExactMapEdges"
  "test-heap-dump-js-objects-core/HeapDumpSymbolExactNameEdge"
  "test-heap-dump-js-objects-core/HeapDumpSymbolDescriptionNameEdge"
  "test-heap-dump-js-objects-core/HeapDumpPrivateNameSymbolExactNameEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSeqOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSeqTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSharedSeqOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSharedSeqTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSharedExternalOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSharedExternalTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSharedUncachedExternalOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSharedUncachedExternalTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpConsOneByteStringExactFirstSecondEdges"
  "test-heap-dump-js-objects-strings/HeapDumpConsTwoByteStringExactFirstSecondEdges"
  "test-heap-dump-js-objects-strings/HeapDumpSlicedOneByteStringExactParentEdge"
  "test-heap-dump-js-objects-strings/HeapDumpSlicedTwoByteStringExactParentEdge"
  "test-heap-dump-js-objects-strings/HeapDumpThinOneByteStringActualEdge"
  "test-heap-dump-js-objects-strings/HeapDumpThinTwoByteStringActualEdge"
  "test-heap-dump-js-objects-strings/HeapDumpExternalOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpExternalTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpUncachedExternalOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpUncachedExternalTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpExternalInternalizedOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpExternalInternalizedTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpUncachedExternalInternalizedOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpUncachedExternalInternalizedTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpInternalizedOneByteStringExactMapEdge"
  "test-heap-dump-js-objects-strings/HeapDumpInternalizedTwoByteStringExactMapEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSArgumentsObjectExactPrototypeEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSGeneratorObjectExactPrototypeEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSGeneratorObjectExactInternalEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSAsyncFunctionObjectExactPrototypeEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSAsyncGeneratorObjectExactPrototypeEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSClassConstructorExactPrototypeEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpJSArrayExactElementsEdge"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpJSObjectExactPropertiesEdge"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpFixedArrayBackingStoreExactElementEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpFixedArrayMixedSlotInternalEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpExtractFixedArrayReferencesWhiteBoxEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpFixedDoubleArrayExactElementEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpSloppyArgumentsElementsExactInternalEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpJSArrayFastObjectElementsExactElementEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpJSArrayDictionaryElementsExactElementEdges"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpExtractElementReferencesEdgesMatchNativeSnapshot"
  "test-heap-dump-js-objects-arrays-elements/HeapDumpJSArrayFastObjectElementsUsesArrayLength"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionPrototypeAndInitialMapExactEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionDirectPrototypeExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionNonInstancePrototypeExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionSandboxDispatchCodeExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionNoSandboxCodeExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSBoundFunctionExactBindingEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionFeedbackSharedContextExactEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpExtractContextReferencesWhiteBoxNonNativeEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpFunctionContextExtractContextReferences"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpFunctionContextWrongScopeInfoTargetTypeGate"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSGlobalObjectExactGlobalProxyEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSGlobalObjectOwnPropertyEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSGlobalProxyExactCreationContextShortcutEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSWrappedFunctionExactPrototypeEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpSharedFunctionInfoScriptExactEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpScriptExactCoreEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpScriptInfosTagObjectCurrentlyUnsupported"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpCompiledFunctionScopeInfoExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpCompiledFunctionScopeInfoBytecodeExactEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpFeedbackVectorExactSharedFunctionInfoEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpFeedbackVectorTagsFeedbackArrays"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpSharedFunctionInfoFeedbackMetadataExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpSharedFunctionInfoTrustedFunctionDataExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpSharedFunctionInfoUntrustedFunctionDataBuiltinEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpSharedFunctionInfoAllEdgesWhiteBox"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpNativeContextExactGlobalObjectEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpExtractContextReferencesNativeContextWhiteBoxEdges"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpScriptContextExactScopeInfoEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpScriptHostDefinedOptionsTagObjectProbe"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpWithContextExtensionExactEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSContextExtensionObjectExactExtensionEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpScriptContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpScriptContextTableExactNativeContextEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpBlockContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpCatchContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpAwaitContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpEvalContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpDebugEvaluateContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpModuleContextExactPreviousEdge"
  "test-heap-dump-js-objects-functions-contexts/HeapDumpModuleObjectsExactTypeAndNamespaceEdges"
  "test-heap-dump-js-builtins-core/HeapDumpJSDateExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSRegExpExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSRegExpPrototypeExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSRegExpConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-core/HeapDumpJSPromiseExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSPromisePrototypeClassicUsageEdges"
  "test-heap-dump-js-builtins-core/HeapDumpJSPromiseConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-core/HeapDumpJSErrorExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSDisposableStackBuiltinsExactPrototypeEdges"
  "test-heap-dump-js-builtins-core/HeapDumpJSDisposableStackBaseExactMapEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSRawJSONExactRawJSONPropertyEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSAtomicsSynchronizationPrimitiveExactPrototypeEdges"
  "test-heap-dump-js-builtins-core/HeapDumpJSPrimitiveWrapperExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSShadowRealmExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSWrappedFunctionFromShadowRealmExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSCollatorExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSDateTimeFormatExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSLocaleExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSNumberFormatExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSPluralRulesExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSDisplayNamesExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSDurationFormatExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSListFormatExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSRelativeTimeFormatExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSSegmenterExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSSegmentsExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSSegmentIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-core/HeapDumpJSV8BreakIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSArrayIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSStringIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSRegExpStringIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSMapKeyIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSMapValueIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSMapKeyValueIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSMapObjectGroupByExactTableEdges"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSSetValueIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSSetKeyValueIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSIteratorHelpersExactPrototypeEdges"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSAsyncFromSyncIteratorExactPrototypeEdge"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSSetPrototypeClassicUsageEdges"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpIteratorPrototypeObjectsClassicUsageEdges"
  "test-heap-dump-js-builtins-iterators-collections/HeapDumpJSValidIteratorWrapperClassicUsageEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpJSArrayBufferExactPrototypeEdge"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpJSTypedArrayExactBufferEdge"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpJSTypedArrayBufferBackingStoreEdgeParity"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpUint8TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpInt8TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpUint8ClampedTypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpUint16TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpInt16TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpUint32TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpInt32TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpFloat32TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpFloat64TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpFloat16TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpBigInt64TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpBigUint64TypedArrayConstructorExactPrototypeEdges"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpJSTypedArrayPrototypeClassicUsageEdge"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpJSDataViewExactBufferEdge"
  "test-heap-dump-js-builtins-buffers-typedarrays/HeapDumpJSRabGsabDataViewExactBufferEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalDurationExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalInstantExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalPlainDateExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalPlainMonthDayExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalPlainYearMonthExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalPlainDateTimeExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalPlainTimeExactPrototypeEdge"
  "test-heap-dump-js-builtins-temporal/HeapDumpJSTemporalZonedDateTimeExactPrototypeEdge"
  "test-heap-dump-weak-references/HeapDumpWeakArraysExactWeakAndClearedSlotEdges"
  "test-heap-dump-weak-references/HeapDumpFinalizationRegistryNextDirtyExactWeakEdge"
  "test-heap-dump-weak-references/HeapDumpEphemeronHashTableExactWeakKeyValueEdges"
  "test-heap-dump-weak-references/HeapDumpExtractEphemeronHashTableReferencesWhiteBoxEdges"
  "test-heap-dump-weak-references/HeapDumpJSMapExactTableEdge"
  "test-heap-dump-weak-references/HeapDumpJSSetExactTableEdge"
  "test-heap-dump-weak-references/HeapDumpJSWeakCollectionExactTableEdges"
  "test-heap-dump-weak-references/HeapDumpJSWeakRefExactWeakTargetEdge"
  "test-heap-dump-weak-references/HeapDumpWeakCellExactWeakTargetAndTokenEdges"
  "test-heap-dump-weak-references/HeapDumpExtractWeakCellReferencesWhiteBoxEdges"
  "test-heap-dump-weak-references/HeapDumpCustomWeakPointerFamiliesUseWeakEdges"
  "test-heap-dump-properties/HeapDumpAccessorStructuresExactEdges"
  "test-heap-dump-properties/HeapDumpAccessorInfoExactEdges"
  "test-heap-dump-properties/HeapDumpAccessorPairExactEdges"
  "test-heap-dump-properties/HeapDumpDescriptorArrayExactEntryEdges"
  "test-heap-dump-properties/HeapDumpStrongDescriptorArrayExactMapEdge"
  "test-heap-dump-properties/HeapDumpMapDescriptorsExactDescriptorArrayEdge"
  "test-heap-dump-properties/HeapDumpMapPrototypeExactPrototypeEdge"
  "test-heap-dump-properties/HeapDumpMapNativeContextExactNativeContextEdge"
  "test-heap-dump-properties/HeapDumpMapDependentCodeExactDependentCodeEdge"
  "test-heap-dump-properties/HeapDumpMapPrototypeValidityCellExactTagObject"
  "test-heap-dump-properties/HeapDumpMapConstructorExactConstructorEdge"
  "test-heap-dump-properties/HeapDumpMapConstructorFunctionDataExactEdge"
  "test-heap-dump-properties/HeapDumpMapBackPointerExactBackPointerEdge"
  "test-heap-dump-properties/HeapDumpMapFixedArrayTransitionExactTransitionEdge"
  "test-heap-dump-properties/HeapDumpMapWeakTransitionExactTransitionEdge"
  "test-heap-dump-properties/HeapDumpMapTransitionsExactTransitionArrayEdge"
  "test-heap-dump-properties/HeapDumpTransitionArrayPrototypeTransitionsProbe"
  "test-heap-dump-properties/HeapDumpMapPrototypeInfoExactPrototypeInfoEdge"
  "test-heap-dump-properties/HeapDumpAliasedArgumentsEntryExactMapEdge"
  "test-heap-dump-properties/HeapDumpArrayBoilerplateDescriptionExactMapEdge"
  "test-heap-dump-properties/HeapDumpAllocationMementoExactMapEdge"
  "test-heap-dump-properties/HeapDumpClassBoilerplateExactMapEdge"
  "test-heap-dump-properties/HeapDumpTransitionArrayExactMapEdge"
  "test-heap-dump-properties/HeapDumpClassPositionsExactMapEdge"
  "test-heap-dump-properties/HeapDumpSwissNameDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpModuleRequestExactMapEdge"
  "test-heap-dump-properties/HeapDumpPromiseCapabilityExactMapEdge"
  "test-heap-dump-properties/HeapDumpPromiseReactionExactMapEdge"
  "test-heap-dump-properties/HeapDumpPropertyDescriptorObjectExactMapEdge"
  "test-heap-dump-properties/HeapDumpScriptOrModuleExactMapEdge"
  "test-heap-dump-properties/HeapDumpCallSiteInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpStackFrameInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpStackTraceInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpErrorStackDataExactMapEdge"
  "test-heap-dump-properties/HeapDumpJSMessageObjectExactMapAndScriptEdges"
  "test-heap-dump-properties/HeapDumpSourceTextModuleInfoEntryExactMapEdge"
  "test-heap-dump-properties/HeapDumpRegExpBoilerplateDescriptionExactMapEdge"
  "test-heap-dump-properties/HeapDumpRegExpDataExactMapEdge"
  "test-heap-dump-properties/HeapDumpAtomRegExpDataExactMapEdge"
  "test-heap-dump-properties/HeapDumpIrRegExpDataExactMapEdge"
  "test-heap-dump-properties/HeapDumpRegExpMatchInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpObjectBoilerplateDescriptionExactMapEdge"
  "test-heap-dump-properties/HeapDumpEnumCacheExactMapEdge"
  "test-heap-dump-properties/HeapDumpPrototypeInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpPrototypeInfoChainEnumCacheExactTagObject"
  "test-heap-dump-properties/HeapDumpPrototypeInfoUsersExactTagObject"
  "test-heap-dump-properties/HeapDumpHashTableExactMapEdge"
  "test-heap-dump-properties/HeapDumpNameDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpNumberDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpGlobalDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpExactPropertyCellType"
  "test-heap-dump-properties/HeapDumpPropertyCellExactValueEdge"
  "test-heap-dump-properties/HeapDumpPropertyCellExactDependentCodeEdge"
  "test-heap-dump-properties/HeapDumpNameToIndexHashTableExactMapEdge"
  "test-heap-dump-properties/HeapDumpSmallOrderedHashMapExactMapEdge"
  "test-heap-dump-properties/HeapDumpSmallOrderedHashSetExactMapEdge"
  "test-heap-dump-properties/HeapDumpSmallOrderedNameDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpRegisteredSymbolTableExactMapEdge"
  "test-heap-dump-properties/HeapDumpOrderedNameDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpSimpleNameDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpSimpleNumberDictionaryExactMapEdge"
  "test-heap-dump-properties/HeapDumpPropertyArrayExactPropertyEdges"
  "test-heap-dump-properties/HeapDumpTurboshaftTypesExactMapEdges"
  "test-heap-dump-properties/HeapDumpTurbofanTypesExactMapEdges"
  "test-heap-dump-properties/HeapDumpCoverageInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpFunctionTemplateInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpObjectTemplateInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpDictionaryTemplateInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpInterceptorInfoExactMapEdge"
  "test-heap-dump-properties/HeapDumpDoubleStringCacheExactMapEdge"
  "test-heap-dump-dump-categories/HeapDumpObjectCountSkipsFreeSpaceAndFiller"
  "test-heap-dump-dump-categories/HeapDumpHeaderPrefixFieldsAreInitializedAndCompatible"
  "test-heap-dump-dump-categories/HeapDumpRawObjectCategoriesAreEncodedAsExpected"
  "test-heap-dump-dump-categories/HeapDumpTranslateDiagnosticsStoreBoundedSample"
  "test-heap-dump-dump-categories/HeapDumpTranslateDiagnosticsKeepDistinctBoundedSamples"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsTruncatedHeaderGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsBadMagicGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsDumpVersionMismatchGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateAcceptsLegacyV1HeaderGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsMissingTimestampGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsV8VersionMismatchGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsFlagMismatchGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsRootOffsetMismatchGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsRootTableOutOfBoundsGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsRootTypeCountBeyondTableGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsInvalidRootTypeGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsOffheapPayloadOverlappingDirectory"
  "test-heap-dump-dump-categories/HeapDumpTranslateSkipsDuplicateUnknownOffheapKinds"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsTruncatedObjectTableGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsObjectCountBeyondTableGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateMapOnlyObjectsExposeOnlyMapEdge"
  "test-heap-dump-dump-categories/HeapDumpTranslateFakeObjectLookupUsesRawheapTable"
  "test-heap-dump-dump-categories/HeapDumpTranslateUnknownInstanceTypeDoesNotAbort"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsOversizedObjectPayload"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsMalformedMapOnlyPayload"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsMapOnlyVisitorTypeMismatch"
  "test-heap-dump-dump-categories/HeapDumpTranslatePrimitiveWrapperUnexpectedValueDoesNotAbort"
  "test-heap-dump-dump-categories/HeapDumpTranslateRejectsMissingFunctionSfiGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateInstructionStreamUsesSpecialPath"
  "test-heap-dump-dump-categories/HeapDumpTranslateInstructionStreamStrongRelocTargetUsesHiddenEdge"
  "test-heap-dump-dump-categories/HeapDumpTranslateInstructionStreamWeakRelocTargetUsesWeakEdge"
  "test-heap-dump-dump-categories/HeapDumpTranslateInstructionStreamRelocMissingTargetDoesNotFabricateEdge"
  "test-heap-dump-dump-categories/HeapDumpRawHeapHeaderTrustedCageBaseValidation"
  "test-heap-dump-dump-categories/HeapDumpRawHeapHeaderV2MigrationPreservesSectionsAndMarksTrustedBaseUnavailable"
  "test-heap-dump-dump-categories/HeapDumpRawHeapHeaderV3IncludesTrustedCageBase"
  "test-heap-dump-dump-categories/HeapDumpTranslateAcceptsLegacyV2HeaderGracefully"
  "test-heap-dump-dump-categories/HeapDumpSandboxProtectedPointerV2SkipsWithTrustedBaseDiagnostic"
  "test-heap-dump-dump-categories/HeapDumpTranslateCurrentHeaderAcceptsAbsentOffheapSectionConsistentFields"
  "test-heap-dump-dump-categories/HeapDumpTranslateCurrentHeaderRejectsCountZeroWithNonzeroSizeGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateCurrentHeaderRejectsOffheapOutOfBoundsGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateCurrentHeaderRejectsOffheapOverlappingObjectTableGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateCurrentHeaderRejectsSizeZeroWithNonzeroCountGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateCurrentHeaderRejectsZeroOffsetWithNonzeroSizeGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateLegacyV1NormalizesMetadataOffsetCorrectly"
  "test-heap-dump-dump-categories/HeapDumpTranslateLegacyV1RejectsCorruptedObjectCountGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateLegacyV1RejectsCorruptedObjectTableOffsetGracefully"
  "test-heap-dump-dump-categories/HeapDumpTranslateSupportsLegacyV1AndCurrentVersions"
  "test-heap-dump-offheap-tables/HeapDumpNoSandboxCodeWrapperCodeHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpNoSandboxRegExpBoilerplateDataHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpNoSandboxIrRegExpDataCodeHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpDumpsOffheapAndExternalCodeSpacePayloads"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverSandboxPlaceholderWarning"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverSandboxKindFallbackNoSandbox"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverUnknownKindWarning"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverJSDispatchTablePresent"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverJSDispatchTableMissingWarning"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateJSDispatchCodeEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateCodeWrapperCodePointerHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateRegExpBoilerplateTrustedDataHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateIrRegExpDataCodePointerHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateIrRegExpDataUc16CodePointerHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateJSDispatchGetNodeGateDiagnostic"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateCodePointerGetNodeGateDiagnostic"
  "test-heap-dump-extract-references/HeapDumpCodeBaselineSupportExactEdges"
  "test-heap-dump-extract-references/HeapDumpCodeDeoptSupportExactEdges"
  "test-heap-dump-extract-references/HeapDumpRegExpBoilerplateDescriptionTagsRegExpDataOnly"
  "test-heap-dump-extract-references/HeapDumpExtractReferencesBroadCoverage"
  "test-heap-dump-extract-references/HeapDumpExtractReferencesSystemNodeCoverage"
  "test-heap-dump-serializer/HeapDumpSerializerProducesExpectedHeapSnapshotSkeleton"
  "test-heap-dump-visitor-ids/HeapDumpVisitorIdsJavaScriptCoverage"
  "test-heap-dump-visitor-ids/HeapDumpVisitorIdsCoverageReport"
  "test-heap-dump-visitor-ids/HeapDumpVisitorIdsSourceBackedExclusions"
  "test-heap-dump-visitor-ids/HeapDumpCppHeapExternalMegaDomHandlerExactMapEdges"
  "test-heap-dump-visitor-ids/HeapDumpVisitorIdsAllNativeIdsHaveDfxPolicy"
  "test-heap-dump-visitor-ids/HeapDumpVisitorIdsUnsupportedPolicyIsExplicit"
  "test-heap-dump-visitor-ids/HeapDumpBytecodeArrayProtectedPointerHiddenEdges"
  "test-heap-dump-visitor-ids/HeapDumpTranslateInterpreterDataProtectedPointerEdges"
  "test-heap-dump-visitor-ids/HeapDumpSandboxBytecodeArrayProtectedPointerHiddenEdgesWithTrustedCageBase"
  "test-heap-dump-visitor-ids/HeapDumpSandboxInterpreterDataProtectedPointerEdgesWithTrustedCageBase"
  "test-heap-dump-visitor-ids-shared/HeapDumpVisitorIdsSharedCoverage"
  "test-heap-dump-visitor-ids-shared/HeapDumpVisitorIdsSharedCoverageReport"
  "test-heap-dump-visitor-ids-torque/HeapDumpTorqueInternalClassSmiBoxSmiPairExactMapEdge"
  "test-heap-dump-visitor-ids-torque/HeapDumpVisitorIdsTorqueCoverage"
  "test-heap-dump-visitor-ids-torque/HeapDumpVisitorIdsTorqueCoverageReport"
  "test-heap-dump-visitor-ids-wasm/HeapDumpVisitorIdsWasmCoverage"
  "test-heap-dump-visitor-ids-wasm/HeapDumpVisitorIdsWasmCoverageReport"
  "test-heap-dump-visitor-ids-wasm/HeapDumpOffheapResolverParsesWasmCanonicalStructTable"
  "test-heap-dump-visitor-ids-wasm/HeapDumpOffheapResolverRejectsMalformedWasmCanonicalStructTable"
  "test-heap-dump-visitor-ids-wasm/HeapDumpOffheapResolverAcceptsMissingSection"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmCanonicalTypeTableSectionIsDumped"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructRefFieldTranslatesToEdge"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmJsApiObjectsExactPrototypeAndMapEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmExceptionTagValueObjectsExactPrototypeAndMapEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmFunctionImportDataExactMapEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmHardInternalTypesExactMapEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmCapiSuspenderContinuationExactMapEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmArrayRejectsCorruptTypeInfoFieldGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmTrustedInstanceDataProtectedFieldsTranslateToNamedEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmDispatchTypeInfoMemoryMapExactMapEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmTypeInfoExactMapEdge"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmArrayRefElementsTranslateToIndexedEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmArrayScalarElementsDoNotCreateObjectEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmArrayRejectsWrongTypeInfoObjectGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmArrayRejectsInvalidElementTypeGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructExactRefFieldTranslatesToEdge"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructDescriptorRawFieldZeroTranslatesToEdge"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStringViewIterExactMapAndStringEdge"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructMissingCanonicalTablePreservesNodeAndMap"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructMissingCanonicalRecordPreservesNodeAndMap"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructRejectsFieldOffsetOutOfBoundsGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructRejectsDuplicateCanonicalTypeIndexGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructRejectsTruncatedCanonicalFieldsGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmStructRejectsCanonicalFieldsOverlapGracefully"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmInstanceAndModuleObjectExactNamedEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmInstanceMissingModuleObjectDoesNotFabricateEdge"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmScriptExactNamedEdges"
  "test-heap-dump-visitor-ids-wasm/HeapDumpWasmScriptMissingManagedNativeModuleDoesNotFabricateEdge"
)

declare -a CCTESTS_SANDBOX_ONLY=(
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionSandboxDispatchCodeExactEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverSandboxPlaceholderWarning"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateCodeWrapperCodePointerHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateRegExpBoilerplateTrustedDataHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateIrRegExpDataCodePointerHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateIrRegExpDataUc16CodePointerHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateJSDispatchGetNodeGateDiagnostic"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateCodePointerGetNodeGateDiagnostic"
  "test-heap-dump-visitor-ids/HeapDumpSandboxBytecodeArrayProtectedPointerHiddenEdgesWithTrustedCageBase"
  "test-heap-dump-visitor-ids/HeapDumpSandboxInterpreterDataProtectedPointerEdgesWithTrustedCageBase"
)

declare -a CCTESTS_NO_SANDBOX_ONLY=(
  "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionNoSandboxCodeExactEdge"
  "test-heap-dump-offheap-tables/HeapDumpNoSandboxCodeWrapperCodeHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpNoSandboxRegExpBoilerplateDataHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpNoSandboxIrRegExpDataCodeHiddenEdge"
  "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverSandboxKindFallbackNoSandbox"
)

# Dedicated heap-dump mjsunit samples.
declare -a MJSUNITS=(
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/accessor-property-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/arguments-sloppy-mapped-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-concat-spreadability-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-findlast-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-flat-flatmap-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-from-async-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-iteration-callbacks-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-join-tostring-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-slice-typedarray-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-sort-comparator-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-splice-unshift-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/arrays-elements-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/arraybuffer-transfer-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-copy-methods-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/array-elements-transition-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/asm-wasm-data-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/async-from-sync-iterator-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/async-stack-combinators-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/atomics-shared-memory-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/atomics-waitasync-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/bound-functions-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/buffers-typedarrays-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/callsite-stack-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/class-auto-accessors-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/collections-hash-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/collections-methods-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/code-coverage-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/core-builtins-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/compiler-and-regexp-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/contexts-scopes-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/cow-elements-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/date-temporal-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/debugger-profiler-hooks-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/dictionary-elements-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/disposable-stack-states-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/error-stack-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/execution-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/errors-regexp-stack-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/execution-objects-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/external-strings-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/fast-api-calls-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/feedback-ic-allocation-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/float16-dataview-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/function-metadata-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/generator-async-state-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/global-context-churn-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/globals-cells-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/ic-super-private-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/instance-type-catalog-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/intl-and-json-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/intl-duration-format-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/intl-enumeration-locale-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/intl-formatters-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/iterator-helper-abrupt-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/iterators-disposables-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/json-parse-source-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/language-builtins-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/large-literals-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/legacy-escape-unescape-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/lazy-preparse-uncompiled-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/lazy-uncompiled-boundary-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/literal-metadata-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/megadom-ic-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-dynamic-error-variants-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-import-attributes-errors-deep-sample.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-live-binding-helper.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-import-attributes-json-sample.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-namespace-helper.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-namespace-sample.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-reexport-live-binding-sample.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/module-tla-dynamic-sample.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-assign-copy-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-enumeration-descriptors-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-fromentries-iterator-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-integrity-levels-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-keys-ownkeys-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-shape-transition-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/object-map-groupby-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/optimized-deopt-feedback-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/promise-any-aggregate-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/promise-capability-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/promise-reaction-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/promises-async-jobs-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/property-shapes-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/protector-cells-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/proxy-private-brand-errors-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/proxy-reflect-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/raw-json-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/regexp-boilerplate-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/regexp-data-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/regexp-experimental-engine-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/regexp-overrides-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/realms-cross-context-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/resizable-buffers-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/shadowrealm-importvalue-deep-sample.mjs"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/shadowrealm-wrapped-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/shared-objects-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/small-ordered-hash-dictionaries-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/string-property-key-external-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/string-replaceall-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/string-search-boundaries-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/string-split-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/string-wellformed-external-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/strings-symbols-numbers-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/string-number-cache-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/temporal-operations-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/typedarray-detach-errors-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/typedarray-methods-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/tolocalestring-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/uint8array-base64-hex-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/value-serializer-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/visitor-id-catalog-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-api-advanced-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-api-object-variants-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-code-coverage-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-exception-exnref-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-funcref-globals-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-function-table-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-gc-reference-globals-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-jspi-suspending-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-memory64-table64-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-memory-control-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-memory-resizable-buffer-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-reference-globals-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-shared-gc-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-stringrefs-views-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-type-reflection-function-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-gc-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/wasm-struct-crash-repro.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/weak-collections-ephemeron-chains-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/weak-symbol-collections-deep-sample.js"
  "arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/weakrefs-finalization-deep-sample.js"
)

INSTANCE_TYPE_CANDIDATE_MARKER="HEAP_DUMP_CANDIDATE_INSTANCE_TYPE"
INSTANCE_TYPE_COVERAGE_MARKER="HEAP_DUMP_COVERED_INSTANCE_TYPE"
EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT="${EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT:-}"
INSTANCE_TYPE_CANDIDATE_FILE="${LOG_DIR}/candidate-instance-types.txt"
INSTANCE_TYPE_COVERAGE_FILE="${LOG_DIR}/covered-instance-types.txt"
INSTANCE_TYPE_MISSING_FILE="${LOG_DIR}/missing-instance-types.txt"
INSTANCE_TYPE_EXTRA_COVERED_FILE="${LOG_DIR}/covered-instance-types-not-candidate.txt"
CCTEST_LIST_FILE="${LOG_DIR}/cctest-list.txt"
TOTAL=0
PASS=0
FAIL=0
SKIPPED=0
declare -a FAILED_CASES=()

count_lines() {
  local count
  count="$(wc -l <"$1")"
  echo "${count//[[:space:]]/}"
}

print_effective_gn_value() {
  local arg_name="$1"
  if [[ -z "${GN_BIN}" ]]; then
    echo "${arg_name}: <gn not found>"
    return
  fi

  local gn_output
  if ! gn_output="$("${GN_BIN}" args "${OUT_ABS}" \
      --list="${arg_name}" 2>/dev/null)"; then
    echo "${arg_name}: <unavailable>"
    return
  fi

  local current
  current="$(awk -F'= ' '/Current value/ { print $2; exit }' <<<"${gn_output}")"
  if [[ -z "${current}" ]]; then
    current="<unset>"
  fi
  echo "${arg_name}: ${current}"
}

gn_arg_current_value() {
  local arg_name="$1"
  if [[ -z "${GN_BIN}" ]]; then
    return 1
  fi

  local gn_output
  if ! gn_output="$("${GN_BIN}" args "${OUT_ABS}" \
      --list="${arg_name}" 2>/dev/null)"; then
    return 1
  fi

  awk -F'= ' '/Current value/ { print $2; exit }' <<<"${gn_output}"
}

gn_arg_enabled() {
  local arg_name="$1"
  local current
  if ! current="$(gn_arg_current_value "${arg_name}")"; then
    return 1
  fi

  [[ "${current}" == "true" ]]
}

print_config_summary() {
  echo "=== Config ==="
  echo "Output dir : ${OUTDIR}"
  print_effective_gn_value "v8_enable_sandbox"
  print_effective_gn_value "v8_enable_i18n_support"
  print_effective_gn_value "v8_ohos_enable_heapdump"
}

normalize_instance_type_markers() {
  local marker="$1"
  shift

  awk -F: -v marker="${marker}" '
    $1 == marker && $2 ~ /^[0-9]+$/ && $3 ~ /^[A-Za-z0-9_]+$/ &&
        $3 !~ /^(FIRST_|LAST_)/ {
      if (!($2 in names)) names[$2] = $3
    }
    END {
      for (id in names) print id ":" names[id]
    }
  ' "$@" | sort -t: -k1,1n
}

collect_candidate_instance_types() {
  local raw_file="${LOG_DIR}/candidate-instance-types.raw"
  : >"${raw_file}"
  : >"${INSTANCE_TYPE_CANDIDATE_FILE}"

  echo "[INFO] Collecting candidate InstanceTypes from cctest build macros"
  HEAP_DUMP_PRINT_CANDIDATE_INSTANCE_TYPES=1 \
    "${CCTEST_BIN}" --list >"${raw_file}" 2>&1
  local rc=$?
  if [[ ${rc} -ne 0 ]]; then
    echo "[FAIL] Candidate InstanceType collection failed (exit ${rc})"
    echo "       log: ${raw_file}"
    return 1
  fi

  normalize_instance_type_markers "${INSTANCE_TYPE_CANDIDATE_MARKER}" \
    "${raw_file}" >"${INSTANCE_TYPE_CANDIDATE_FILE}"

  local candidate_count
  candidate_count="$(count_lines "${INSTANCE_TYPE_CANDIDATE_FILE}")"
  if [[ ${candidate_count} -eq 0 ]]; then
    echo "[FAIL] Candidate InstanceType collection produced no markers"
    echo "       log: ${raw_file}"
    return 1
  fi

  if [[ -n "${EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT}" &&
        ${candidate_count} -ne ${EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT} ]]; then
    echo "[FAIL] Candidate InstanceType count drifted: actual ${candidate_count}, expected ${EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT}"
    echo "       list: ${INSTANCE_TYPE_CANDIDATE_FILE}"
    echo "       log: ${raw_file}"
    return 1
  fi

  if [[ -n "${EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT}" ]]; then
    echo "[PASS] Candidate InstanceTypes collected: ${candidate_count} (expected ${EXPECTED_INSTANCE_TYPE_CANDIDATE_COUNT})"
  else
    echo "[PASS] Candidate InstanceTypes collected: ${candidate_count}"
  fi
  return 0
}

collect_registered_cctests() {
  : >"${CCTEST_LIST_FILE}"

  if ! "${CCTEST_BIN}" --list >"${CCTEST_LIST_FILE}.raw" 2>&1; then
    echo "[FAIL] cctest --list failed"
    echo "       log: ${CCTEST_LIST_FILE}.raw"
    return 1
  fi

  awk '
    /^test-heap-dump-/ { print $1; next }
    /^\*\*>Test:[[:space:]]+test-heap-dump-/ { print $2; next }
  ' "${CCTEST_LIST_FILE}.raw" | sort -u >"${CCTEST_LIST_FILE}"
  return 0
}

is_registered_cctest() {
  grep -Fxq "$1" "${CCTEST_LIST_FILE}"
}

cctest_array_contains() {
  local needle="$1"
  shift
  local item
  for item in "$@"; do
    if [[ "${item}" == "${needle}" ]]; then
      return 0
    fi
  done
  return 1
}

is_config_expected_missing_cctest() {
  local test_case="$1"
  local sandbox_value
  if ! sandbox_value="$(gn_arg_current_value "v8_enable_sandbox")"; then
    return 1
  fi

  if [[ "${sandbox_value}" == "true" ]]; then
    cctest_array_contains "${test_case}" "${CCTESTS_NO_SANDBOX_ONLY[@]}"
    return
  fi

  if [[ "${sandbox_value}" == "false" ]]; then
    cctest_array_contains "${test_case}" "${CCTESTS_SANDBOX_ONLY[@]}"
    return
  fi

  return 1
}

is_cctest_supported_by_current_gn_config() {
  local test_case="$1"
  ! is_config_expected_missing_cctest "${test_case}"
}

validate_runner_cctest_registration() {
  local missing_file="${LOG_DIR}/cctests-not-registered.txt"
  local expected_missing_file="${LOG_DIR}/cctests-config-skipped.txt"
  local unqueued_file="${LOG_DIR}/cctests-not-in-runner.txt"
  local duplicate_file="${LOG_DIR}/cctests-runner-duplicates.txt"
  : >"${missing_file}"
  : >"${expected_missing_file}"
  : >"${unqueued_file}"
  : >"${duplicate_file}"

  printf '%s\n' "${CCTESTS[@]}" | sort | uniq -d >"${duplicate_file}"
  local duplicate_count
  duplicate_count="$(count_lines "${duplicate_file}")"
  if [[ ${duplicate_count} -gt 0 ]]; then
    echo "[FAIL] ${duplicate_count} duplicate cctest runner entries"
    echo "       list: ${duplicate_file}"
    sed 's/^/       - /' "${duplicate_file}"
    return 1
  fi

  local test_case
  for test_case in "${CCTESTS[@]}"; do
    if ! is_registered_cctest "${test_case}"; then
      if is_config_expected_missing_cctest "${test_case}"; then
        echo "${test_case}" >>"${expected_missing_file}"
      else
        echo "${test_case}" >>"${missing_file}"
      fi
    fi
  done

  while IFS= read -r test_case; do
    if cctest_array_contains "${test_case}" "${CCTESTS[@]}"; then
      continue
    fi
    echo "${test_case}" >>"${unqueued_file}"
  done <"${CCTEST_LIST_FILE}"

  local expected_missing_count
  expected_missing_count="$(count_lines "${expected_missing_file}")"
  if [[ ${expected_missing_count} -gt 0 ]]; then
    echo "[INFO] ${expected_missing_count} cctest runner entries are not registered for this GN config"
    echo "       list: ${expected_missing_file}"
  fi

  local missing_count
  missing_count="$(count_lines "${missing_file}")"
  if [[ ${missing_count} -gt 0 ]]; then
    echo "[FAIL] ${missing_count} cctest runner entries are not registered in this build"
    echo "       list: ${missing_file}"
    sed 's/^/       - /' "${missing_file}"
    return 1
  fi

  local unqueued_count
  unqueued_count="$(count_lines "${unqueued_file}")"
  if [[ ${unqueued_count} -gt 0 ]]; then
    echo "[FAIL] ${unqueued_count} registered non-metadata heap-dump cctest cases are not in the runner"
    echo "       list: ${unqueued_file}"
    sed 's/^/       - /' "${unqueued_file}"
    return 1
  fi

  echo "[PASS] cctest runner registrations verified: ${#CCTESTS[@]} entries (${expected_missing_count} config-skipped)"
  return 0
}

validate_mjsunit_registration() {
  local listed_file="${LOG_DIR}/mjsunit-listed.txt"
  local discovered_file="${LOG_DIR}/mjsunit-discovered.txt"
  local missing_file="${LOG_DIR}/mjsunit-not-in-runner.txt"
  local stale_file="${LOG_DIR}/mjsunit-runner-stale.txt"
  : >"${listed_file}"
  : >"${discovered_file}"
  : >"${missing_file}"
  : >"${stale_file}"

  printf '%s\n' "${MJSUNITS[@]}" | sort -u >"${listed_file}"
  find "${V8_ROOT}/arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit" -maxdepth 1 -type f \
    \( -name '*.js' -o -name '*.mjs' \) \
    -printf 'arkweb/chromium_ext/v8/dfx/heap_dump/tests/mjsunit/%f\n' | sort -u >"${discovered_file}"

  comm -23 "${discovered_file}" "${listed_file}" >"${missing_file}"
  comm -13 "${discovered_file}" "${listed_file}" >"${stale_file}"

  local missing_count
  local stale_count
  missing_count="$(count_lines "${missing_file}")"
  stale_count="$(count_lines "${stale_file}")"

  if [[ ${missing_count} -gt 0 || ${stale_count} -gt 0 ]]; then
    if [[ ${missing_count} -gt 0 ]]; then
      echo "[FAIL] ${missing_count} heap-dump mjsunit samples are not in the runner"
      sed 's/^/       - /' "${missing_file}"
    fi
    if [[ ${stale_count} -gt 0 ]]; then
      echo "[FAIL] ${stale_count} heap-dump mjsunit runner entries are stale"
      sed 's/^/       - /' "${stale_file}"
    fi
    return 1
  fi

  echo "[PASS] mjsunit runner registrations verified: ${#MJSUNITS[@]} entries"
  return 0
}

print_instance_type_coverage() {
  local raw_file="${LOG_DIR}/covered-instance-types.raw"
  : >"${raw_file}"
  : >"${INSTANCE_TYPE_COVERAGE_FILE}"
  : >"${INSTANCE_TYPE_MISSING_FILE}"
  : >"${INSTANCE_TYPE_EXTRA_COVERED_FILE}"

  if [[ ! -s "${INSTANCE_TYPE_CANDIDATE_FILE}" ]]; then
    echo "[FAIL] Candidate InstanceType list is missing or empty"
    echo "       list: ${INSTANCE_TYPE_CANDIDATE_FILE}"
    return 1
  fi

  if compgen -G "${LOG_DIR}/case_*.log" >/dev/null; then
    grep -h "^${INSTANCE_TYPE_COVERAGE_MARKER}:" \
      "${LOG_DIR}"/case_*.log >"${raw_file}" 2>/dev/null || true
  fi

  normalize_instance_type_markers "${INSTANCE_TYPE_COVERAGE_MARKER}" \
    "${raw_file}" >"${INSTANCE_TYPE_COVERAGE_FILE}"

  awk -F: '
    FILENAME == ARGV[1] { covered[$1] = 1; next }
    FILENAME == ARGV[2] && !($1 in covered) { print }
  ' "${INSTANCE_TYPE_COVERAGE_FILE}" "${INSTANCE_TYPE_CANDIDATE_FILE}" \
    >"${INSTANCE_TYPE_MISSING_FILE}"

  awk -F: '
    FILENAME == ARGV[1] { candidate[$1] = 1; next }
    FILENAME == ARGV[2] && !($1 in candidate) { print }
  ' "${INSTANCE_TYPE_CANDIDATE_FILE}" "${INSTANCE_TYPE_COVERAGE_FILE}" \
    >"${INSTANCE_TYPE_EXTRA_COVERED_FILE}"

  local candidate_count
  local covered_count
  local missing_count
  local extra_covered_count
  candidate_count="$(count_lines "${INSTANCE_TYPE_CANDIDATE_FILE}")"
  covered_count="$(count_lines "${INSTANCE_TYPE_COVERAGE_FILE}")"
  missing_count="$(count_lines "${INSTANCE_TYPE_MISSING_FILE}")"
  extra_covered_count="$(count_lines "${INSTANCE_TYPE_EXTRA_COVERED_FILE}")"

  echo
  echo "Candidate InstanceTypes (current build): ${candidate_count}"
  echo "Candidate list: ${INSTANCE_TYPE_CANDIDATE_FILE}"
  echo "Covered InstanceTypes (object-level test assertions): ${covered_count}"
  awk -F: '{ printf "- %s (%s)\n", $2, $1 }' \
    "${INSTANCE_TYPE_COVERAGE_FILE}"
  echo "Covered list: ${INSTANCE_TYPE_COVERAGE_FILE}"
  echo "Missing InstanceTypes (candidate - covered): ${missing_count}"
  echo "Missing list: ${INSTANCE_TYPE_MISSING_FILE}"
  if [[ ${extra_covered_count} -gt 0 ]]; then
    echo "Covered markers outside candidate list: ${extra_covered_count}"
    echo "Outside-candidate list: ${INSTANCE_TYPE_EXTRA_COVERED_FILE}"
  fi
}

run_case() {
  local label="$1"
  shift
  local log_file="${LOG_DIR}/case_${TOTAL}.log"
  ((TOTAL += 1))

  if [[ $VERBOSE -eq 1 ]]; then
    "$@" 2>&1 | tee "${log_file}"
    local rc=${PIPESTATUS[0]}
    if [[ $rc -eq 0 ]]; then
      echo "[PASS] ${label}"
      ((PASS += 1))
    else
      echo "[FAIL] ${label} (exit ${rc})"
      FAILED_CASES+=("${label}")
      ((FAIL += 1))
    fi
    return
  fi

  "$@" >"${log_file}" 2>&1
  local rc=$?
  if [[ $rc -eq 0 ]]; then
    echo "[PASS] ${label}"
    ((PASS += 1))
  else
    echo "[FAIL] ${label} (exit ${rc})"
    echo "       log: ${log_file}"
    FAILED_CASES+=("${label}")
    ((FAIL += 1))
  fi
}

validate_translated_heapsnapshot() {
  local snapshot_path="$1"
  if [[ ! -s "${snapshot_path}" ]]; then
    echo "[ERROR] translate did not produce a heapsnapshot file: ${snapshot_path}"
    return 1
  fi

  local required_section
  for required_section in '"snapshot"' '"nodes"' '"edges"' '"strings"'; do
    if ! grep -q "${required_section}" "${snapshot_path}"; then
      echo "[ERROR] translated heapsnapshot is missing ${required_section}: ${snapshot_path}"
      return 1
    fi
  done
}

run_mjsunit_dump_translate_case() {
  local js_case="$1"
  shift
  local -a js_flags=("$@")
  local label="mjsunit ${js_case}"
  local log_file="${LOG_DIR}/case_${TOTAL}.log"
  local case_number
  local sample_name
  local sample_dir
  local rawheap_path
  local snapshot_path
  local logfile_path
  ((TOTAL += 1))
  case_number="${TOTAL}"
  sample_name="$(basename "${js_case}")"
  printf -v sample_dir "%s/mjsunit_%03d_%s" "${LOG_DIR}" "${case_number}" \
    "${sample_name}"
  mkdir -p "${sample_dir}"
  rawheap_path="${sample_dir}/sample.rawheap"
  snapshot_path="${sample_dir}/sample.heapsnapshot"
  logfile_path="${sample_dir}/sample.v8.log"

  (
    "${D8_BIN}" "${js_flags[@]}" "--logfile=${logfile_path}" \
      --heapdump-out "${rawheap_path}" test/mjsunit/mjsunit.js "${js_case}"
    rc=$?
    if [[ ${rc} -ne 0 ]]; then
      echo "[ERROR] d8 failed for ${js_case} (exit ${rc})"
      exit "${rc}"
    fi
    if [[ ! -s "${rawheap_path}" ]]; then
      echo "[ERROR] d8 did not produce a rawheap file: ${rawheap_path}"
      exit 1
    fi

    "${TRANSLATE_BIN}" --translate "${rawheap_path}" \
      --translate-out "${snapshot_path}"
    rc=$?
    if [[ ${rc} -ne 0 ]]; then
      echo "[ERROR] translate failed for ${js_case} (exit ${rc})"
      exit "${rc}"
    fi
    if ! validate_translated_heapsnapshot "${snapshot_path}"; then
      exit 1
    fi
  ) >"${log_file}" 2>&1
  local rc=$?

  if [[ $VERBOSE -eq 1 ]]; then
    cat "${log_file}"
  fi

  if [[ ${rc} -eq 0 ]]; then
    echo "[PASS] ${label}"
    ((PASS += 1))
  else
    echo "[FAIL] ${label} (exit ${rc})"
    echo "       log: ${log_file}"
    FAILED_CASES+=("${label}")
    ((FAIL += 1))
  fi
}

print_summary() {
  {
    echo
    echo "=== Summary ==="
    echo "Output dir : ${OUTDIR}"
    echo "Log dir    : ${LOG_DIR}"
    echo "Summary    : ${SUMMARY_FILE}"
    echo "Total      : ${TOTAL}"
    echo "Pass       : ${PASS}"
    echo "Fail       : ${FAIL}"
    echo "Skipped    : ${SKIPPED}"

    if [[ ${#FAILED_CASES[@]} -gt 0 ]]; then
      echo
      echo "Failed cases:"
      for item in "${FAILED_CASES[@]}"; do
        echo "- ${item}"
      done
    fi
  } | tee "${SUMMARY_FILE}"
}

validate_no_new_repo_root_temp_artifacts() {
  snapshot_repo_root_temp_artifacts >"${REPO_ROOT_TEMP_AFTER_FILE}"
  comm -13 "${REPO_ROOT_TEMP_BASELINE_FILE}" "${REPO_ROOT_TEMP_AFTER_FILE}" \
    >"${REPO_ROOT_TEMP_NEW_FILE}"
  local new_count
  new_count="$(count_lines "${REPO_ROOT_TEMP_NEW_FILE}")"
  if [[ ${new_count} -gt 0 ]]; then
    echo "[FAIL] ${new_count} new repo-root heap-dump temp artifacts detected"
    echo "       list: ${REPO_ROOT_TEMP_NEW_FILE}"
    sed 's/^/       - /' "${REPO_ROOT_TEMP_NEW_FILE}"
    return 1
  fi
  echo "[PASS] no new repo-root heap-dump temp artifacts"
  return 0
}

cd "${V8_ROOT}" || exit 1

print_config_summary

if [[ $RUN_CCTEST -eq 1 ]]; then
  echo
  echo "=== cctest heap-dump ==="
  if ! collect_registered_cctests; then
    exit 1
  fi
  if ! validate_runner_cctest_registration; then
    exit 1
  fi
  if ! collect_candidate_instance_types; then
    exit 1
  fi
  for test_case in "${CCTESTS[@]}"; do
    if ! is_cctest_supported_by_current_gn_config "${test_case}"; then
      echo "[SKIP] cctest ${test_case} (not for this GN config)"
      ((SKIPPED += 1))
      continue
    fi
    run_case "cctest ${test_case}" "${CCTEST_BIN}" "${test_case}"
  done
  if ! print_instance_type_coverage; then
    exit 1
  fi
fi

if [[ $RUN_MJSUNIT -eq 1 ]]; then
  echo
  echo "=== mjsunit heap-dump ==="
  if ! validate_mjsunit_registration; then
    exit 1
  fi
  mjsunit_index=0
  for js_case in "${MJSUNITS[@]}"; do
    ((mjsunit_index += 1))
    mapfile -t js_flags < <(extract_js_flags "${js_case}")
    run_mjsunit_dump_translate_case "${js_case}" "${js_flags[@]}"
  done

  echo
  echo "=== cli heap-dump ==="
  run_case "cli d8-dump-then-translate" \
    bash "${SCRIPT_DIR}/heap-dump-cli-smoke.sh" --outdir "${OUTDIR}"
fi

print_summary

if ! validate_no_new_repo_root_temp_artifacts; then
  exit 1
fi

if [[ ${#FAILED_CASES[@]} -gt 0 ]]; then
  exit 1
fi

exit 0
