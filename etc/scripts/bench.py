#!/usr/bin/env python3
"""
Compute-unit and binary-size regression check for the examples.

Runs `caravel test` for every example at each SBPF version, collects the
"<label>: <n> CU" lines the tests print and the size of build/program.so,
and compares them to etc/baseline.json.

    bench.py                compare against the baseline (exit 1 on regression)
    bench.py --update       rewrite the baseline with the current numbers
    bench.py --summary F    also write a markdown table to F (e.g. CI summary)

Numbers depend on the platform-tools version, so the baseline records which
one produced it; a run on a different version reports but never fails.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "etc" / "baseline.json"
EXAMPLES = ["counter", "vault", "token-vault", "amm"]
SBF_VERSIONS = ["v0", "v3"]
CU_LINE = re.compile(r"^\s+([A-Za-z][\w +/-]*): (\d+) CU\s*$", re.M)


def platform_tools_version():
    clang = os.environ.get("SOLANA_CLANG")
    if not clang:
        cache = Path.home() / ".cache" / "solana"
        found = sorted(
            cache.glob("v*/platform-tools/llvm/bin/clang"),
            key=lambda p: [int(x) for x in re.findall(r"\d+", p.parts[-5])],
        )
        clang = str(found[-1]) if found else ""
    m = re.search(r"/(v\d+\.\d+)/", clang)
    return m.group(1) if m else "unknown"


def run_example(caravel, example, sbf_ver):
    cwd = ROOT / "examples" / example
    env = dict(os.environ, CARAVEL_INCLUDE=str(ROOT / "include"))
    proc = subprocess.run(
        [str(caravel), "test", f"--sbf-ver={sbf_ver}"],
        cwd=cwd, env=env, capture_output=True, text=True,
    )
    out = proc.stdout + proc.stderr
    if proc.returncode != 0:
        sys.stderr.write(out)
        raise SystemExit(f"bench: {example} ({sbf_ver}) tests failed")
    cu = {label.strip(): int(n) for label, n in CU_LINE.findall(out)}
    size = (cwd / "build" / "program.so").stat().st_size
    return {"cu": cu, "size": size}


def collect(caravel):
    results = {}
    for ex in EXAMPLES:
        for ver in SBF_VERSIONS:
            print(f"bench: {ex} {ver}", file=sys.stderr)
            results[f"{ex}/{ver}"] = run_example(caravel, ex, ver)
    return results


def fmt_delta(old, new):
    if old is None:
        return "new"
    d = new - old
    if d == 0:
        return "="
    pct = f" ({d / old:+.1%})" if old else ""
    return f"{d:+d}{pct}"


def compare(baseline, current):
    """Returns (markdown rows, regressions, improvements)."""
    rows, regressions, improvements = [], [], []
    for key, cur in current.items():
        base = baseline.get(key, {"cu": {}, "size": None})
        metrics = [(f"{label} (CU)", base["cu"].get(label), n)
                   for label, n in cur["cu"].items()]
        metrics.append(("program.so (bytes)", base.get("size"), cur["size"]))
        for name, old, new in metrics:
            rows.append(f"| {key} | {name} | {'' if old is None else old} "
                        f"| {new} | {fmt_delta(old, new)} |")
            if old is not None and new > old:
                regressions.append(f"{key} {name}: {old} -> {new}")
            elif old is not None and new < old:
                improvements.append(f"{key} {name}: {old} -> {new}")
    return rows, regressions, improvements


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--summary", metavar="FILE")
    ap.add_argument("--caravel", default=str(ROOT / "build" / "caravel"))
    args = ap.parse_args()

    pt = platform_tools_version()
    current = collect(Path(args.caravel).resolve())

    if args.update:
        BASELINE.write_text(json.dumps(
            {"platform_tools": pt, "results": current}, indent=2,
            sort_keys=True) + "\n")
        print(f"bench: baseline written ({pt})")
        return 0

    baseline = json.loads(BASELINE.read_text()) if BASELINE.exists() else {
        "platform_tools": None, "results": {}}
    rows, regressions, improvements = compare(baseline["results"], current)
    same_toolchain = baseline["platform_tools"] == pt

    table = "\n".join([
        f"### Compute units & binary size (platform-tools {pt})",
        "",
        "| example | metric | baseline | current | delta |",
        "|---|---|---:|---:|---:|",
        *rows,
    ])
    print(table)
    if args.summary:
        with open(args.summary, "a") as f:
            f.write(table + "\n")

    if not same_toolchain:
        print(f"\nbench: baseline is from platform-tools "
              f"{baseline['platform_tools']}, this run used {pt}; "
              "not failing on differences")
        return 0
    if improvements:
        print("\nbench: improvements (run `make bench-update` to lock in):")
        print("\n".join(f"  {i}" for i in improvements))
    if regressions:
        print("\nbench: REGRESSIONS:")
        print("\n".join(f"  {r}" for r in regressions))
        return 1
    print("\nbench: no regressions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
