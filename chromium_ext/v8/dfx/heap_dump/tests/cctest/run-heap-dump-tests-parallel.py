#!/usr/bin/env python3
"""Parallel heap-dump test runner.

Drop-in replacement for run-heap-dump-tests.sh with parallel test execution.
Discovers tests dynamically from ``cctest --list`` and the filesystem, then
dispatches them across a configurable thread pool.

Usage:
    python3 run-heap-dump-tests-parallel.py [options]
    python3 run-heap-dump-tests-parallel.py --test-jobs 8 --no-build
    python3 run-heap-dump-tests-parallel.py --cctest-only --filter '*Weak*'
"""

from __future__ import annotations

import argparse
import concurrent.futures
import fnmatch
import os
import pathlib
import re
import signal
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass, field
from typing import Dict, FrozenSet, List, Optional, Sequence, Tuple

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
V8_ROOT = SCRIPT_DIR.parents[6]

INSTANCE_TYPE_CANDIDATE_MARKER = "HEAP_DUMP_CANDIDATE_INSTANCE_TYPE"
INSTANCE_TYPE_COVERAGE_MARKER = "HEAP_DUMP_COVERED_INSTANCE_TYPE"

SANDBOX_ONLY_TESTS: FrozenSet[str] = frozenset({
    "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionSandboxDispatchCodeExactEdge",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverSandboxPlaceholderWarning",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateCodeWrapperCodePointerHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateRegExpBoilerplateTrustedDataHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateIrRegExpDataCodePointerHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateIrRegExpDataUc16CodePointerHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateJSDispatchGetNodeGateDiagnostic",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTranslateCodePointerGetNodeGateDiagnostic",
    "test-heap-dump-visitor-ids/HeapDumpSandboxBytecodeArrayProtectedPointerHiddenEdgesWithTrustedCageBase",
    "test-heap-dump-visitor-ids/HeapDumpSandboxInterpreterDataProtectedPointerEdgesWithTrustedCageBase",
})

NO_SANDBOX_ONLY_TESTS: FrozenSet[str] = frozenset({
    "test-heap-dump-js-objects-functions-contexts/HeapDumpJSFunctionNoSandboxCodeExactEdge",
    "test-heap-dump-offheap-tables/HeapDumpNoSandboxCodeWrapperCodeHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpNoSandboxRegExpBoilerplateDataHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpNoSandboxIrRegExpDataCodeHiddenEdge",
    "test-heap-dump-offheap-tables/HeapDumpOffheapTableResolverSandboxKindFallbackNoSandbox",
})

# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------

@dataclass
class TestResult:
    name: str
    status: str  # PASS, FAIL, SKIP
    exit_code: int = 0
    duration: float = 0.0
    log_path: str = ""
    skip_reason: str = ""


@dataclass
class CommandResult:
    returncode: int
    stdout: str
    stderr: str


# ---------------------------------------------------------------------------
# Utility helpers
# ---------------------------------------------------------------------------

def detect_cpu_count() -> int:
    try:
        return os.cpu_count() or 1
    except Exception:
        return 1


def run_captured(
    cmd: Sequence[str],
    timeout: int,
    cwd: Optional[str] = None,
    env: Optional[Dict[str, str]] = None,
) -> CommandResult:
    """Run a command and kill its whole process group on timeout."""
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=cwd,
        env=env,
        start_new_session=True,
    )
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
        return CommandResult(proc.returncode, stdout, stderr)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = proc.communicate()
        raise subprocess.TimeoutExpired(
            cmd, timeout, output=stdout, stderr=stderr)


def find_gn_binary() -> Optional[str]:
    candidates = [
        V8_ROOT / "buildtools" / "linux64" / "gn",
        V8_ROOT / "buildtools" / "mac" / "gn",
    ]
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    return shutil.which("gn")


def get_gn_arg(gn_bin: Optional[str], out_abs: str, arg_name: str) -> Optional[str]:
    if not gn_bin:
        return None
    try:
        result = run_captured(
            [gn_bin, "args", out_abs, f"--list={arg_name}"], timeout=10)
        if result.returncode != 0:
            return None
        for line in result.stdout.splitlines():
            m = re.match(r"\s*Current value\s*=\s*(\S+)", line)
            if m:
                return m.group(1)
    except Exception:
        pass
    return None


def is_sandbox_enabled(gn_bin: Optional[str], out_abs: str) -> Optional[bool]:
    val = get_gn_arg(gn_bin, out_abs, "v8_enable_sandbox")
    if val == "true":
        return True
    if val == "false":
        return False
    return None


def should_skip_cctest(test_name: str, sandbox: Optional[bool]) -> Optional[str]:
    if sandbox is True and test_name in NO_SANDBOX_ONLY_TESTS:
        return "no-sandbox only"
    if sandbox is False and test_name in SANDBOX_ONLY_TESTS:
        return "sandbox only"
    return None


def sanitize_filename(name: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_-]", "_", name)[:80]


def make_run_timestamp() -> str:
    return time.strftime("%Y%m%d-%H%M%S")


def timestamped_log_name(prefix: str, run_timestamp: str, suffix: str) -> str:
    return f"{prefix}_{run_timestamp}_{suffix}.log"


def cleanup_pycache() -> None:
    pycache = SCRIPT_DIR / "__pycache__"
    if pycache.exists():
        shutil.rmtree(pycache, ignore_errors=True)


def extract_js_flags(js_file: str) -> List[str]:
    full_path = V8_ROOT / js_file
    flags: List[str] = []
    try:
        with open(full_path) as f:
            for line in f:
                m = re.match(r"^//\s*Flags:\s*(.*)$", line)
                if m:
                    flags.extend(m.group(1).split())
    except Exception:
        pass
    return flags


def snapshot_repo_root_temp_artifacts() -> set:
    patterns = ("v8.log", "*.rawheap", "*.heapsnapshot", "siso*")
    found = set()
    for p in patterns:
        for f in V8_ROOT.glob(p):
            if f.is_file():
                try:
                    stat = f.stat()
                    found.add((f.name, stat.st_size, stat.st_mtime))
                except OSError:
                    pass
    return found


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def find_build_tool(out_abs: str) -> Tuple[Optional[str], str]:
    """Return (binary_path, tool_name) for the best available build tool."""
    siso = os.environ.get("SISO", "")
    if not siso and (V8_ROOT / "third_party" / "siso" / "siso").is_file():
        siso = str(V8_ROOT / "third_party" / "siso" / "siso")
    if siso and os.path.isfile(os.path.join(out_abs, "siso_metadata.json")):
        return siso, "siso"

    ninja = os.environ.get("NINJA", "")
    if not ninja:
        candidate = V8_ROOT / "third_party" / "ninja" / "ninja"
        if candidate.is_file() and os.access(candidate, os.X_OK):
            ninja = str(candidate)
        else:
            ninja = shutil.which("ninja") or ""
    if ninja:
        return ninja, "ninja"

    autoninja = os.environ.get("AUTONINJA", "")
    if not autoninja:
        autoninja = shutil.which("autoninja") or ""
    if not autoninja:
        for p in (
            V8_ROOT.parent / "depot_tools" / "autoninja",
            pathlib.Path("/home/hh/workspace/depot_tools/autoninja"),
        ):
            if p.is_file() and os.access(p, os.X_OK):
                autoninja = str(p)
                break
    if autoninja:
        return autoninja, "autoninja"

    return None, ""


def detect_default_build_jobs() -> int:
    cpus = detect_cpu_count()
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    kb = int(line.split()[1])
                    mem_jobs = max(1, kb // 2097152)
                    return min(cpus, mem_jobs)
    except Exception:
        pass
    return cpus


def run_build(out_abs: str, build_jobs: int) -> None:
    tool_path, tool_name = find_build_tool(out_abs)
    if not tool_path:
        print("[ERROR] No build tool found. Set SISO, NINJA, AUTONINJA, "
              "or install depot_tools.", file=sys.stderr)
        sys.exit(2)

    targets = ["cctest", "d8", "translate"]
    if tool_name == "siso":
        cmd = [tool_path, "ninja", "-C", out_abs, "-offline",
               "-local_jobs", str(build_jobs)] + targets
    else:
        cmd = [tool_path, "-C", out_abs, f"-j{build_jobs}"] + targets

    print(f"[BUILD] {' '.join(cmd)}")
    rc = subprocess.call(cmd)
    if rc != 0:
        print("[FAIL] Build failed", file=sys.stderr)
        sys.exit(1)
    print("[PASS] Build finished")


# ---------------------------------------------------------------------------
# Test discovery
# ---------------------------------------------------------------------------

def discover_heap_dump_cctests(cctest_bin: str) -> List[str]:
    result = run_captured([cctest_bin, "--list"], timeout=30)
    tests: set = set()
    for line in result.stdout.splitlines():
        line = line.strip()
        m = re.match(r"(?:\*\*>Test:\s+)?(test-heap-dump-\S+)", line)
        if m:
            tests.add(m.group(1))
    for line in result.stderr.splitlines():
        m = re.match(r"(?:\*\*>Test:\s+)?(test-heap-dump-\S+)", line)
        if m:
            tests.add(m.group(1))
    return sorted(tests)


def discover_heap_dump_mjsunits() -> List[str]:
    mjsunit_dir = (V8_ROOT / "arkweb" / "chromium_ext" / "v8" / "dfx" /
                   "heap_dump" / "tests" / "mjsunit")
    if not mjsunit_dir.is_dir():
        return []
    files = []
    for f in sorted(mjsunit_dir.iterdir()):
        if f.suffix in (".js", ".mjs") and f.is_file():
            files.append(str(f.relative_to(V8_ROOT)))
    return files


# ---------------------------------------------------------------------------
# Instance-type marker helpers
# ---------------------------------------------------------------------------

def parse_instance_type_markers(marker: str, text: str) -> Dict[int, str]:
    types: Dict[int, str] = {}
    for line in text.splitlines():
        parts = line.split(":")
        if (len(parts) >= 3 and parts[0] == marker
                and re.fullmatch(r"[0-9]+", parts[1])
                and re.fullmatch(r"[A-Za-z0-9_]+", parts[2])
                and not parts[2].startswith(("FIRST_", "LAST_"))):
            tid = int(parts[1])
            if tid not in types:
                types[tid] = parts[2]
    return types


def collect_candidate_instance_types(
    cctest_bin: str, log_dir: pathlib.Path,
) -> Optional[Dict[int, str]]:
    raw_file = log_dir / "candidate-instance-types.raw"
    env = os.environ.copy()
    env["HEAP_DUMP_PRINT_CANDIDATE_INSTANCE_TYPES"] = "1"
    try:
        result = run_captured([cctest_bin, "--list"], timeout=30, env=env)
    except Exception as exc:
        print(f"[FAIL] Candidate InstanceType collection failed: {exc}")
        return None

    combined = result.stdout + "\n" + result.stderr
    raw_file.write_text(combined)
    types = parse_instance_type_markers(INSTANCE_TYPE_CANDIDATE_MARKER, combined)
    if not types:
        print("[FAIL] Candidate InstanceType collection produced no markers")
        print(f"       log: {raw_file}")
        return None
    return types


def collect_coverage_from_logs(log_dir: pathlib.Path) -> Dict[int, str]:
    combined = []
    for log_file in sorted(log_dir.glob("cctest_*.log")):
        try:
            combined.append(log_file.read_text(errors="replace"))
        except Exception:
            pass
    return parse_instance_type_markers(
        INSTANCE_TYPE_COVERAGE_MARKER, "\n".join(combined),
    )


def write_instance_type_file(path: pathlib.Path, types: Dict[int, str]) -> None:
    with open(path, "w") as f:
        for tid in sorted(types):
            f.write(f"{tid}:{types[tid]}\n")


def print_instance_type_coverage(
    log_dir: pathlib.Path,
    candidates: Dict[int, str],
) -> None:
    coverage_file = log_dir / "covered-instance-types.txt"
    missing_file = log_dir / "missing-instance-types.txt"
    extra_file = log_dir / "covered-instance-types-not-candidate.txt"

    covered = collect_coverage_from_logs(log_dir)
    write_instance_type_file(coverage_file, covered)

    missing = {tid: name for tid, name in candidates.items() if tid not in covered}
    write_instance_type_file(missing_file, missing)

    extra = {tid: name for tid, name in covered.items() if tid not in candidates}
    write_instance_type_file(extra_file, extra)

    print()
    print(f"Candidate InstanceTypes (current build): {len(candidates)}")
    print(f"Candidate list: {log_dir / 'candidate-instance-types.txt'}")
    print(f"Covered InstanceTypes (object-level test assertions): {len(covered)}")
    for tid in sorted(covered):
        print(f"- {covered[tid]} ({tid})")
    print(f"Covered list: {coverage_file}")
    print(f"Missing InstanceTypes (candidate - covered): {len(missing)}")
    print(f"Missing list: {missing_file}")
    if extra:
        print(f"Covered markers outside candidate list: {len(extra)}")
        print(f"Outside-candidate list: {extra_file}")


# ---------------------------------------------------------------------------
# Single-test execution
# ---------------------------------------------------------------------------

def run_single_cctest(
    cctest_bin: str,
    test_name: str,
    log_path: pathlib.Path,
    timeout: int,
) -> TestResult:
    label = f"cctest {test_name}"
    t0 = time.monotonic()
    try:
        result = run_captured(
            [cctest_bin, test_name], timeout=timeout, cwd=str(V8_ROOT))
        log_path.write_text(result.stdout + result.stderr)
        duration = time.monotonic() - t0
        if result.returncode == 0:
            return TestResult(label, "PASS", duration=duration,
                              log_path=str(log_path))
        return TestResult(label, "FAIL", exit_code=result.returncode,
                          duration=duration, log_path=str(log_path))
    except subprocess.TimeoutExpired as exc:
        duration = time.monotonic() - t0
        out = ""
        if exc.stdout:
            out += exc.stdout if isinstance(exc.stdout, str) else exc.stdout.decode(errors="replace")
        if exc.stderr:
            out += exc.stderr if isinstance(exc.stderr, str) else exc.stderr.decode(errors="replace")
        log_path.write_text(out + f"\n--- TIMEOUT after {timeout}s ---\n")
        return TestResult(label, "FAIL", exit_code=-1, duration=duration,
                          log_path=str(log_path))
    except Exception as exc:
        duration = time.monotonic() - t0
        log_path.write_text(f"ERROR: {exc}\n")
        return TestResult(label, "FAIL", exit_code=-1, duration=duration,
                          log_path=str(log_path))


def run_single_mjsunit(
    d8_bin: str,
    translate_bin: str,
    js_case: str,
    js_flags: List[str],
    log_path: pathlib.Path,
    sample_dir: pathlib.Path,
    timeout: int,
) -> TestResult:
    label = f"mjsunit {js_case}"
    t0 = time.monotonic()
    sample_dir.mkdir(parents=True, exist_ok=True)
    rawheap = sample_dir / "sample.rawheap"
    snapshot = sample_dir / "sample.heapsnapshot"
    logfile = sample_dir / "sample.v8.log"
    output_parts: List[str] = []

    try:
        # d8 run
        cmd = ([d8_bin] + js_flags
               + [f"--logfile={logfile}", "--heapdump-out", str(rawheap),
                  "test/mjsunit/mjsunit.js", js_case])
        result = run_captured(cmd, timeout=timeout, cwd=str(V8_ROOT))
        output_parts.append(result.stdout)
        output_parts.append(result.stderr)
        if result.returncode != 0:
            log_path.write_text("\n".join(output_parts))
            return TestResult(label, "FAIL", exit_code=result.returncode,
                              duration=time.monotonic() - t0,
                              log_path=str(log_path))
        if not rawheap.exists() or rawheap.stat().st_size == 0:
            output_parts.append(
                f"[ERROR] d8 did not produce rawheap: {rawheap}")
            log_path.write_text("\n".join(output_parts))
            return TestResult(label, "FAIL", exit_code=1,
                              duration=time.monotonic() - t0,
                              log_path=str(log_path))

        # translate run
        cmd = [translate_bin, "--translate", str(rawheap),
               "--translate-out", str(snapshot)]
        result = run_captured(cmd, timeout=timeout)
        output_parts.append(result.stdout)
        output_parts.append(result.stderr)
        if result.returncode != 0:
            log_path.write_text("\n".join(output_parts))
            return TestResult(label, "FAIL", exit_code=result.returncode,
                              duration=time.monotonic() - t0,
                              log_path=str(log_path))

        # validate heapsnapshot
        if not snapshot.exists() or snapshot.stat().st_size == 0:
            output_parts.append("[ERROR] translate did not produce heapsnapshot")
            log_path.write_text("\n".join(output_parts))
            return TestResult(label, "FAIL", exit_code=1,
                              duration=time.monotonic() - t0,
                              log_path=str(log_path))
        snapshot_text = snapshot.read_text(errors="replace")
        for section in ('"snapshot"', '"nodes"', '"edges"', '"strings"'):
            if section not in snapshot_text:
                output_parts.append(
                    f"[ERROR] heapsnapshot missing {section}")
                log_path.write_text("\n".join(output_parts))
                return TestResult(label, "FAIL", exit_code=1,
                                  duration=time.monotonic() - t0,
                                  log_path=str(log_path))

        log_path.write_text("\n".join(output_parts))
        return TestResult(label, "PASS", duration=time.monotonic() - t0,
                          log_path=str(log_path))
    except subprocess.TimeoutExpired:
        output_parts.append(f"--- TIMEOUT after {timeout}s ---")
        log_path.write_text("\n".join(output_parts))
        return TestResult(label, "FAIL", exit_code=-1,
                          duration=time.monotonic() - t0,
                          log_path=str(log_path))
    except Exception as exc:
        output_parts.append(f"ERROR: {exc}")
        log_path.write_text("\n".join(output_parts))
        return TestResult(label, "FAIL", exit_code=-1,
                          duration=time.monotonic() - t0,
                          log_path=str(log_path))


# ---------------------------------------------------------------------------
# Parallel dispatch
# ---------------------------------------------------------------------------

def run_tests_parallel(
    tasks: List[Tuple],
    runner,
    max_workers: int,
    verbose: bool,
) -> List[TestResult]:
    results: List[TestResult] = []
    lock = threading.Lock()
    completed = [0]
    total = len(tasks)

    def on_done(future: concurrent.futures.Future) -> None:
        r = future.result()
        with lock:
            completed[0] += 1
            n = completed[0]
            if r.status == "PASS":
                tag = f"[PASS] ({n}/{total})"
                print(f"{tag} {r.name} ({r.duration:.1f}s)")
            else:
                tag = f"[FAIL] ({n}/{total})"
                print(f"{tag} {r.name} (exit {r.exit_code}, "
                      f"{r.duration:.1f}s)")
                if r.log_path:
                    print(f"       log: {r.log_path}")
            if verbose and r.status == "FAIL" and r.log_path:
                try:
                    text = pathlib.Path(r.log_path).read_text(errors="replace")
                    for line in text.splitlines()[-40:]:
                        print(f"       | {line}")
                except Exception:
                    pass
            results.append(r)

    with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as pool:
        futures = []
        for task in tasks:
            f = pool.submit(runner, *task)
            f.add_done_callback(on_done)
            futures.append(f)
        concurrent.futures.wait(futures)

    return results


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

def print_config_summary(
    outdir: str, gn_bin: Optional[str], out_abs: str, test_jobs: int,
) -> None:
    print("=== Config ===")
    print(f"Output dir      : {outdir}")
    print(f"Test parallelism: {test_jobs}")
    for arg in ("v8_enable_sandbox", "v8_enable_i18n_support",
                "v8_ohos_enable_heapdump"):
        val = get_gn_arg(gn_bin, out_abs, arg)
        print(f"{arg}: {val or '<unavailable>'}")


def print_final_summary(
    outdir: str,
    log_dir: pathlib.Path,
    total: int,
    passed: int,
    failed: int,
    skipped: int,
    failed_cases: List[str],
    wall_time: float,
) -> None:
    summary_file = log_dir / "summary.txt"
    lines = [
        "",
        "=== Summary ===",
        f"Output dir : {outdir}",
        f"Log dir    : {log_dir}",
        f"Summary    : {summary_file}",
        f"Total      : {total}",
        f"Pass       : {passed}",
        f"Fail       : {failed}",
        f"Skipped    : {skipped}",
        f"Wall time  : {wall_time:.1f}s",
    ]
    if failed_cases:
        lines.append("")
        lines.append("Failed cases:")
        for item in failed_cases:
            lines.append(f"- {item}")
    text = "\n".join(lines)
    print(text)
    summary_file.write_text(text + "\n")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Parallel heap-dump test runner")
    parser.add_argument("--outdir", default="out/x64.release",
                        help="Build output dir relative to V8 root "
                             "(default: out/x64.release)")
    parser.add_argument("--jobs", type=int, default=0,
                        help="Build parallelism (default: auto)")
    parser.add_argument("--test-jobs", "-t", type=int, default=0,
                        help="Test parallelism (default: min(cpus, 8))")
    parser.add_argument("--no-build", action="store_true",
                        help="Skip the build step")
    parser.add_argument("--cctest-only", action="store_true",
                        help="Run only cctest cases")
    parser.add_argument("--mjsunit-only", action="store_true",
                        help="Run only mjsunit cases")
    parser.add_argument("--filter", default="",
                        help="Glob pattern to filter test names "
                             "(e.g. '*Weak*', 'test-heap-dump-basic/*')")
    parser.add_argument("--verbose", action="store_true",
                        help="Print tail of log output for failed tests")
    parser.add_argument("--timeout", type=int, default=120,
                        help="Per-test timeout in seconds (default: 120)")
    parser.add_argument("--expected-candidate-count", type=int, default=0,
                        help="Assert the candidate InstanceType count equals N")
    parser.add_argument("--full-cli-smoke", action="store_true",
                        help="Run the legacy shell CLI smoke after mjsunit. "
                             "This serially re-runs all heap-dump mjsunit "
                             "samples.")
    args = parser.parse_args()

    if args.cctest_only and args.mjsunit_only:
        print("[ERROR] --cctest-only and --mjsunit-only are mutually "
              "exclusive", file=sys.stderr)
        sys.exit(2)

    run_cctest_flag = not args.mjsunit_only
    run_mjsunit_flag = not args.cctest_only
    out_abs = str(V8_ROOT / args.outdir)
    cctest_bin = os.path.join(out_abs, "cctest")
    d8_bin = os.path.join(out_abs, "d8")
    translate_bin = os.path.join(out_abs, "translate")
    gn_bin = find_gn_binary()

    if not os.path.isdir(out_abs):
        print(f"[ERROR] Output directory not found: {out_abs}", file=sys.stderr)
        sys.exit(2)

    test_jobs = args.test_jobs if args.test_jobs > 0 else min(detect_cpu_count(), 8)
    build_jobs = args.jobs if args.jobs > 0 else detect_default_build_jobs()

    # Log directory
    tmp_root = V8_ROOT / "tmp"
    tmp_root.mkdir(exist_ok=True)
    os.environ["TMPDIR"] = str(tmp_root)
    run_timestamp = make_run_timestamp()
    log_dir = pathlib.Path(tempfile.mkdtemp(
        prefix=f"heap-dump-tests.{run_timestamp}.", dir=str(tmp_root)))

    # Snapshot repo-root temp artifacts (pre)
    baseline_artifacts = snapshot_repo_root_temp_artifacts()

    # Build
    if not args.no_build:
        run_build(out_abs, build_jobs)

    # Verify binaries
    if run_cctest_flag and not os.access(cctest_bin, os.X_OK):
        print(f"[ERROR] cctest binary not found: {cctest_bin}", file=sys.stderr)
        sys.exit(2)
    if run_mjsunit_flag:
        if not os.access(d8_bin, os.X_OK):
            print(f"[ERROR] d8 binary not found: {d8_bin}", file=sys.stderr)
            sys.exit(2)
        if not os.access(translate_bin, os.X_OK):
            print(f"[ERROR] translate binary not found: {translate_bin}",
                  file=sys.stderr)
            sys.exit(2)

    print_config_summary(args.outdir, gn_bin, out_abs, test_jobs)

    sandbox = is_sandbox_enabled(gn_bin, out_abs)
    wall_t0 = time.monotonic()
    total = 0
    passed = 0
    failed = 0
    skipped = 0
    failed_cases: List[str] = []
    candidates: Optional[Dict[int, str]] = None

    # ---- cctest phase ----
    if run_cctest_flag:
        print()
        print("=== cctest heap-dump ===")

        all_cctests = discover_heap_dump_cctests(cctest_bin)
        cctests = list(all_cctests)
        print(f"[INFO] Discovered {len(cctests)} heap-dump cctest cases")

        # Candidate instance types
        print("[INFO] Collecting candidate InstanceTypes from cctest "
              "build macros")
        candidates = collect_candidate_instance_types(cctest_bin, log_dir)
        if candidates is None:
            sys.exit(1)
        write_instance_type_file(
            log_dir / "candidate-instance-types.txt", candidates)

        ccount = len(candidates)
        if args.expected_candidate_count > 0:
            if ccount != args.expected_candidate_count:
                print(f"[FAIL] Candidate InstanceType count drifted: "
                      f"actual {ccount}, expected "
                      f"{args.expected_candidate_count}")
                sys.exit(1)
            print(f"[PASS] Candidate InstanceTypes collected: {ccount} "
                  f"(expected {args.expected_candidate_count})")
        else:
            print(f"[PASS] Candidate InstanceTypes collected: {ccount}")

        # Prepare tasks
        cctest_tasks = []
        for i, test_name in enumerate(cctests):
            # Filter
            if args.filter and not fnmatch.fnmatch(test_name, args.filter):
                continue
            skip_reason = should_skip_cctest(test_name, sandbox)
            if skip_reason:
                total += 1
                skipped += 1
                print(f"[SKIP] cctest {test_name} ({skip_reason})")
                continue
            log_path = log_dir / timestamped_log_name(
                "cctest", run_timestamp,
                f"{i:04d}_{sanitize_filename(test_name)}")
            cctest_tasks.append((cctest_bin, test_name, log_path, args.timeout))

        # Parallel execution
        print(f"[INFO] Running {len(cctest_tasks)} cctest cases "
              f"({test_jobs} workers) ...")
        t0 = time.monotonic()
        results = run_tests_parallel(
            cctest_tasks, run_single_cctest, test_jobs, args.verbose)
        elapsed = time.monotonic() - t0

        for r in results:
            total += 1
            if r.status == "PASS":
                passed += 1
            elif r.status == "FAIL":
                failed += 1
                failed_cases.append(r.name)

        print(f"[INFO] cctest phase completed in {elapsed:.1f}s")

        # Instance type coverage
        if candidates:
            print_instance_type_coverage(log_dir, candidates)

    # ---- mjsunit phase ----
    if run_mjsunit_flag:
        print()
        print("=== mjsunit heap-dump ===")

        mjsunits = discover_heap_dump_mjsunits()
        if args.filter:
            mjsunits = [m for m in mjsunits if fnmatch.fnmatch(m, args.filter)
                        or fnmatch.fnmatch(os.path.basename(m), args.filter)]
        print(f"[INFO] Discovered {len(mjsunits)} mjsunit samples")

        # Prepare tasks
        mjsunit_tasks = []
        for i, js_case in enumerate(mjsunits):
            js_flags = extract_js_flags(js_case)
            short = sanitize_filename(os.path.basename(js_case))
            log_path = log_dir / timestamped_log_name(
                "mjsunit", run_timestamp, f"{i:04d}_{short}")
            sample_dir = log_dir / f"mjsunit_{i:04d}_{short}"
            mjsunit_tasks.append(
                (d8_bin, translate_bin, js_case, js_flags,
                 log_path, sample_dir, args.timeout))

        # Parallel execution
        print(f"[INFO] Running {len(mjsunit_tasks)} mjsunit cases "
              f"({test_jobs} workers) ...")
        t0 = time.monotonic()
        results = run_tests_parallel(
            mjsunit_tasks, run_single_mjsunit, test_jobs, args.verbose)
        elapsed = time.monotonic() - t0

        for r in results:
            total += 1
            if r.status == "PASS":
                passed += 1
            elif r.status == "FAIL":
                failed += 1
                failed_cases.append(r.name)

        print(f"[INFO] mjsunit phase completed in {elapsed:.1f}s")

        # CLI smoke test. By default keep this to one representative case
        # because the legacy shell smoke serially re-runs every mjsunit sample.
        print()
        print("=== cli heap-dump ===")
        cli_smoke = SCRIPT_DIR / "heap-dump-cli-smoke.sh"
        if args.full_cli_smoke and cli_smoke.exists():
            cli_log = log_dir / timestamped_log_name(
                "cli_smoke", run_timestamp, "full")
            try:
                result = run_captured(
                    ["bash", str(cli_smoke), "--outdir", args.outdir],
                    timeout=args.timeout, cwd=str(V8_ROOT),
                )
                cli_log.write_text(result.stdout + result.stderr)
                total += 1
                if result.returncode == 0:
                    passed += 1
                    print("[PASS] cli d8-dump-then-translate")
                else:
                    failed += 1
                    failed_cases.append("cli d8-dump-then-translate")
                    print(f"[FAIL] cli d8-dump-then-translate "
                          f"(exit {result.returncode})")
                    print(f"       log: {cli_log}")
            except subprocess.TimeoutExpired:
                total += 1
                failed += 1
                failed_cases.append("cli d8-dump-then-translate")
                print("[FAIL] cli d8-dump-then-translate (timeout)")
        elif mjsunits:
            print("[INFO] Running one representative CLI smoke case; use "
                  "--full-cli-smoke to run the legacy shell smoke that "
                  "serially re-runs every mjsunit sample")
            js_case = next(
                (m for m in mjsunits
                 if os.path.basename(m) == "buffers-typedarrays-sample.js"),
                mjsunits[0],
            )
            cli_log = log_dir / timestamped_log_name(
                "cli_smoke", run_timestamp, "representative")
            sample_dir = log_dir / "cli_smoke_representative"
            result = run_single_mjsunit(
                d8_bin, translate_bin, js_case, extract_js_flags(js_case),
                cli_log, sample_dir, args.timeout)
            result.name = f"cli representative {js_case}"
            total += 1
            if result.status == "PASS":
                passed += 1
                print(f"[PASS] {result.name}")
            else:
                failed += 1
                failed_cases.append(result.name)
                print(f"[FAIL] {result.name} (exit {result.exit_code})")
                print(f"       log: {cli_log}")

    # ---- Final summary ----
    wall_time = time.monotonic() - wall_t0
    print_final_summary(args.outdir, log_dir, total, passed, failed,
                        skipped, failed_cases, wall_time)

    # Validate no new repo-root temp artifacts
    after_artifacts = snapshot_repo_root_temp_artifacts()
    new_artifacts = after_artifacts - baseline_artifacts
    if new_artifacts:
        print(f"[FAIL] {len(new_artifacts)} new repo-root heap-dump "
              "temp artifacts detected")
        for name, size, _ in sorted(new_artifacts):
            print(f"       - {name} ({size} bytes)")
        sys.exit(1)
    print("[PASS] no new repo-root heap-dump temp artifacts")

    sys.exit(1 if failed_cases else 0)


if __name__ == "__main__":
    try:
        main()
    finally:
        cleanup_pycache()
