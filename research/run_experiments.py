#!/usr/bin/env python3
"""Run cache configurations over reproducible traces and record miss counts in CSV."""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Iterator

from generate_workloads import WORKLOADS, generate_workload, write_trace

CSV_FIELDS = [
    "case_id", "workload", "seed", "request_count", "keyspace",
    "algorithms", "capacities", "total_capacity", "total_miss",
    "perfect_miss", "status", "error",
]
TOTAL_MISS_RE = re.compile(
    r"^\s*(?:total[_ ]?miss(?:es)?|miss(?:es)?)\s*[:=]\s*(\d+)\s*$", re.IGNORECASE | re.MULTILINE
)
PERFECT_MISS_RE = re.compile(
    r"^\s*(?:perfect[_ ]?miss(?:es)?|perfect\s+cache\s+miss(?:es)?)\s*[:=]\s*(\d+)\s*$",
    re.IGNORECASE | re.MULTILINE,
)


def parse_csv_list(value: str, cast=str) -> list:
    try:
        parsed = [cast(item.strip()) for item in value.split(",") if item.strip()]
    except ValueError as exc:
        raise argparse.ArgumentTypeError(str(exc)) from exc
    if not parsed:
        raise argparse.ArgumentTypeError("List must not be empty")
    return parsed


def capacity_weight_patterns(level_count: int) -> list[tuple[int, ...]]:
    """A small set of non-decreasing L1->Ln capacity distributions."""
    if level_count == 1:
        return [(1,)]
    if level_count == 2:
        return [(1, 1), (1, 2), (1, 3)]
    if level_count == 3:
        return [(1, 1, 1), (1, 1, 2), (1, 2, 2), (1, 2, 4)]

    patterns = [tuple(1 for _ in range(level_count))]
    patterns.append(tuple(2 ** i for i in range(level_count)))
    patterns.append(tuple([1] * (level_count - 1) + [2]))
    patterns.append(tuple([1] + [2] * (level_count - 1)))
    return list(dict.fromkeys(patterns))


def split_capacity(total: int, weights: tuple[int, ...]) -> tuple[int, ...] | None:
    """Split total capacity proportionally, keeping every level non-empty."""
    levels = len(weights)
    if total < levels:
        return None
    weight_sum = sum(weights)
    exact = [total * weight / weight_sum for weight in weights]
    sizes = [int(x) for x in exact]
    remainder = total - sum(sizes)
    order = sorted(range(levels), key=lambda i: (exact[i] - sizes[i], i), reverse=True)
    for i in order[:remainder]:
        sizes[i] += 1
    if any(size < 1 for size in sizes):
        return None
    return tuple(sizes)


def make_config(algorithms: tuple[str, ...], capacities: tuple[int, ...]) -> str:
    lines = [str(len(algorithms))]
    lines.extend(f"{capacity} {algorithm}" for capacity, algorithm in zip(capacities, algorithms))
    return "\n".join(lines) + "\n"


def iter_configs(algorithms: list[str], levels: list[int], totals: list[int]):
    for level_count in levels:
        for total in totals:
            if total < level_count:
                continue
            capacity_options: set[tuple[int, ...]] = set()
            for weights in capacity_weight_patterns(level_count):
                split = split_capacity(total, weights)
                if split:
                    capacity_options.add(split)
            for capacities in sorted(capacity_options):
                for algorithm_tuple in itertools.product(algorithms, repeat=level_count):
                    yield algorithm_tuple, capacities, total


def case_hash(payload: dict) -> str:
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()[:16]


def parse_misses(stdout: str) -> tuple[int, int]:
    total_match = TOTAL_MISS_RE.search(stdout)
    perfect_match = PERFECT_MISS_RE.search(stdout)
    if not total_match:
        raise ValueError("Could not parse total misses from program output")
    if not perfect_match:
        raise ValueError("Could not parse perfect-cache misses from program output")
    return int(total_match.group(1)), int(perfect_match.group(1))


def read_completed_cases(path: Path) -> set[str]:
    if not path.exists() or path.stat().st_size == 0:
        return set()
    with path.open(newline="", encoding="utf-8") as file:
        return {
            row.get("case_id", "")
            for row in csv.DictReader(file)
            if row.get("status") == "ok" and row.get("case_id")
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True, help="Path to compiled C++ executable")
    parser.add_argument("--algorithms", default="LIRS", help="Comma-separated supported algorithms (default: LIRS)")
    parser.add_argument("--levels", default="1,2,3", help="Level counts to test (default: 1,2,3)")
    parser.add_argument("--total-capacities", default="32,64,128", help="Total capacities to test")
    parser.add_argument("--workloads", default=",".join(WORKLOADS), help="Comma-separated workload names")
    parser.add_argument("--requests", type=int, default=20_000)
    parser.add_argument("--keyspace", type=int, default=512)
    parser.add_argument("--seeds", default="1,2,3", help="Comma-separated RNG seeds")
    parser.add_argument("--output-dir", type=Path, default=Path("research/results"))
    parser.add_argument("--max-runs", type=int, default=0, help="Optional cap for a quick smoke test; 0 means no cap")
    parser.add_argument("--timeout", type=float, default=30.0, help="Timeout for one C++ run, in seconds")
    parser.add_argument("--no-resume", action="store_true", help="Do not skip successful cases already present in CSV")
    args = parser.parse_args()

    args.algorithms = parse_csv_list(args.algorithms, str)
    args.levels = parse_csv_list(args.levels, int)
    args.total_capacities = parse_csv_list(args.total_capacities, int)
    args.workloads = parse_csv_list(args.workloads, str)
    args.seeds = parse_csv_list(args.seeds, int)
    if any(x < 1 for x in args.levels):
        parser.error("Every level count must be >= 1")
    if any(x < 1 for x in args.total_capacities):
        parser.error("Every total capacity must be >= 1")
    if args.requests < 0 or args.keyspace < 4:
        parser.error("--requests must be >= 0 and --keyspace must be >= 4")
    if args.max_runs < 0 or args.timeout <= 0:
        parser.error("--max-runs must be >= 0 and --timeout must be > 0")
    invalid_workloads = sorted(set(args.workloads) - set(WORKLOADS))
    if invalid_workloads:
        parser.error(f"Unknown workload(s): {', '.join(invalid_workloads)}")
    return args


def main() -> int:
    args = parse_args()
    binary = args.binary.expanduser().resolve()
    if not binary.is_file():
        print(f"ERROR: binary not found: {binary}", file=sys.stderr)
        return 2

    output_dir = args.output_dir.expanduser().resolve()
    traces_dir = output_dir / "traces"
    configs_dir = output_dir / "configs"
    output_dir.mkdir(parents=True, exist_ok=True)
    traces_dir.mkdir(parents=True, exist_ok=True)
    configs_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "results.csv"

    stat = binary.stat()
    binary_signature = {"path": str(binary), "mtime_ns": stat.st_mtime_ns, "size": stat.st_size}
    completed = set() if args.no_resume else read_completed_cases(csv_path)
    needs_header = not csv_path.exists() or csv_path.stat().st_size == 0

    configurations = list(iter_configs(args.algorithms, args.levels, args.total_capacities))
    planned_runs = len(configurations) * len(args.workloads) * len(args.seeds)
    if args.max_runs:
        planned_runs = min(planned_runs, args.max_runs)
    print(f"Configurations: {len(configurations)}; planned runs: {planned_runs}; output: {csv_path}")
    print(f"Algorithms selected: {', '.join(args.algorithms)}")
    print("Only use algorithms that the compiled C++ program currently supports.")

    executed = skipped = failed = 0
    with csv_path.open("a", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=CSV_FIELDS)
        if needs_header:
            writer.writeheader()
            csv_file.flush()

        stop = False
        for algorithm_tuple, capacities, total_capacity in configurations:
            config_content = make_config(algorithm_tuple, capacities)
            config_id = hashlib.sha256(config_content.encode("utf-8")).hexdigest()[:12]
            config_path = configs_dir / f"config_{config_id}.cfg"
            if not config_path.exists():
                config_path.write_text(config_content, encoding="utf-8")

            for workload in args.workloads:
                for seed in args.seeds:
                    trace_id = f"{workload}_n{args.requests}_k{args.keyspace}_seed{seed}"
                    trace_path = traces_dir / f"{trace_id}.txt"
                    if not trace_path.exists():
                        requests = generate_workload(workload, args.requests, args.keyspace, seed)
                        write_trace(trace_path, requests)
                    stdin_text = trace_path.read_text(encoding="utf-8")

                    payload = {
                        "binary": binary_signature,
                        "config": config_content,
                        "workload": workload,
                        "seed": seed,
                        "requests": args.requests,
                        "keyspace": args.keyspace,
                    }
                    case_id = case_hash(payload)
                    if case_id in completed:
                        skipped += 1
                        continue
                    if args.max_runs and executed + failed >= args.max_runs:
                        stop = True
                        break

                    row = {
                        "case_id": case_id,
                        "workload": workload,
                        "seed": seed,
                        "request_count": args.requests,
                        "keyspace": args.keyspace,
                        "algorithms": ">".join(algorithm_tuple),
                        "capacities": ">".join(map(str, capacities)),
                        "total_capacity": total_capacity,
                        "total_miss": "",
                        "perfect_miss": "",
                        "status": "error",
                        "error": "",
                    }
                    try:
                        result = subprocess.run(
                            [str(binary), str(config_path)],
                            input=stdin_text,
                            text=True,
                            capture_output=True,
                            timeout=args.timeout,
                            check=False,
                        )
                        if result.returncode != 0:
                            detail = (result.stderr or result.stdout or "no output").strip()
                            raise RuntimeError(f"process exited {result.returncode}: {detail[-1000:]}")
                        total_miss, perfect_miss = parse_misses(result.stdout)
                        row["total_miss"] = total_miss
                        row["perfect_miss"] = perfect_miss
                        row["status"] = "ok"
                        completed.add(case_id)
                        executed += 1
                    except (subprocess.TimeoutExpired, OSError, RuntimeError, ValueError) as exc:
                        row["error"] = str(exc).replace("\n", " ")[:1000]
                        failed += 1

                    writer.writerow(row)
                    csv_file.flush()
                    print(
                        f"[{executed + failed + skipped}/{planned_runs}] "
                        f"{workload} seed={seed} alg={'>'.join(algorithm_tuple)} "
                        f"cap={'>' .join(map(str, capacities))} "
                        f"total_miss={row['total_miss']} perfect_miss={row['perfect_miss']} "
                        f"status={row['status']}"
                    )
                if stop:
                    break
            if stop:
                break

    print(f"Finished: successful={executed}, failed={failed}, skipped={skipped}")
    print(f"Raw results: {csv_path}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
