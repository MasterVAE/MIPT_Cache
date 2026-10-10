#!/usr/bin/env python3
"""Deterministic synthetic request-trace generators for MIPT_Cache."""

from __future__ import annotations

import argparse
import random
from pathlib import Path
from typing import Iterable

WORKLOADS = ("uniform", "zipf", "cyclic", "scan_hot", "phase_shift")


def generate_workload(kind: str, request_count: int, keyspace: int, seed: int) -> list[int]:
    """Create a deterministic workload; equal arguments always yield equal traces."""
    if kind not in WORKLOADS:
        raise ValueError(f"Unknown workload {kind!r}; choose from: {', '.join(WORKLOADS)}")
    if request_count < 0:
        raise ValueError("request_count must be >= 0")
    if keyspace < 4:
        raise ValueError("keyspace must be >= 4")

    rng = random.Random(seed)

    if kind == "uniform":
        return [rng.randrange(keyspace) for _ in range(request_count)]

    if kind == "zipf":
        # A discrete Zipf-like distribution with a long tail.
        weights = [1.0 / (rank ** 1.15) for rank in range(1, keyspace + 1)]
        keys = list(range(keyspace))
        return rng.choices(keys, weights=weights, k=request_count)

    if kind == "cyclic":
        # A repeated working set larger than typical L1 capacities.
        working_set = max(2, (keyspace * 3) // 4)
        return [i % working_set for i in range(request_count)]

    if kind == "scan_hot":
        # Eight accesses to a small hot set, followed by a never-reused scan item.
        hot_size = max(2, keyspace // 16)
        result: list[int] = []
        next_scan_key = keyspace
        for i in range(request_count):
            if i % 9 == 8:
                result.append(next_scan_key)
                next_scan_key += 1
            else:
                result.append(rng.randrange(hot_size))
        return result

    # Two distinct hot sets; workload phase changes halfway through the trace.
    hot_size = max(2, keyspace // 16)
    result = []
    midpoint = request_count // 2
    for i in range(request_count):
        if rng.random() < 0.05:
            result.append(rng.randrange(keyspace))
        elif i < midpoint:
            result.append(rng.randrange(hot_size))
        else:
            result.append(hot_size + rng.randrange(hot_size))
    return result


def serialize_requests(requests: Iterable[int]) -> str:
    """Format requests exactly as the C++ program's stdin protocol expects."""
    values = list(requests)
    if not values:
        return "0\n\n"
    return f"{len(values)}\n" + " ".join(map(str, values)) + "\n"


def write_trace(path: Path, requests: list[int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(serialize_requests(requests), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workload", choices=WORKLOADS, default="uniform")
    parser.add_argument("--requests", type=int, default=20_000)
    parser.add_argument("--keyspace", type=int, default=512)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--output", type=Path, required=True, help="Output trace file")
    args = parser.parse_args()

    requests = generate_workload(args.workload, args.requests, args.keyspace, args.seed)
    write_trace(args.output, requests)
    print(
        f"Generated {len(requests)} requests: workload={args.workload}, "
        f"keyspace={args.keyspace}, seed={args.seed}, file={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
