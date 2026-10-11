#!/usr/bin/env python3
"""Summarize successful cache experiments using total and perfect-cache misses.

Produces three summary tables:
  * summary_by_workload.csv  - best per (workload, exact algorithm tuple, capacities)
  * summary_overall.csv      - best per (exact algorithm tuple, capacities) across workloads
  * summary_by_algorithm.csv - aggregate per individual algorithm, regardless of
                               its position in a multi-level stack or the other
                               algorithms it was combined with.
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    return [row for row in rows if row.get("status") == "ok" and row.get("total_miss") and row.get("perfect_miss")]


def _sort_key(row: dict[str, object]) -> tuple:
    gap = row["gap_percent"]
    return (
        float(gap) if gap != "" else float("inf"),
        float(row["avg_total_miss"]),
    )


def summarize(rows: list[dict[str, str]], group_fields: tuple[str, ...]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, ...], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[tuple(row.get(field, "") for field in group_fields)].append(row)

    result: list[dict[str, object]] = []
    for key, group in grouped.items():
        total_sum = sum(int(row["total_miss"]) for row in group)
        perfect_sum = sum(int(row["perfect_miss"]) for row in group)
        total_count = len(group)
        gap_sum = total_sum - perfect_sum
        item: dict[str, object] = {field: value for field, value in zip(group_fields, key)}
        item.update({
            "runs": total_count,
            "avg_total_miss": round(total_sum / total_count, 3),
            "avg_perfect_miss": round(perfect_sum / total_count, 3),
            "avg_gap": round(gap_sum / total_count, 3),
            "gap_percent": round(100.0 * gap_sum / perfect_sum, 3) if perfect_sum else "",
        })
        result.append(item)

    result.sort(key=_sort_key)
    return result


def summarize_by_algorithm(rows: list[dict[str, str]]) -> list[dict[str, object]]:
    """Aggregate stats for every algorithm that appears anywhere in a config.

    A row using, say, LIRS>ARC>LFU contributes to the buckets for LIRS, ARC
    and LFU exactly once each (duplicates within a single stack are collapsed).
    """
    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        for algorithm in set(row["algorithms"].split(">")):
            if algorithm:
                grouped[algorithm].append(row)

    result: list[dict[str, object]] = []
    for algorithm, group in grouped.items():
        total_sum = sum(int(row["total_miss"]) for row in group)
        perfect_sum = sum(int(row["perfect_miss"]) for row in group)
        count = len(group)
        gap_sum = total_sum - perfect_sum
        result.append({
            "algorithm": algorithm,
            "configs": count,
            "avg_total_miss": round(total_sum / count, 3),
            "avg_perfect_miss": round(perfect_sum / count, 3),
            "avg_gap": round(gap_sum / count, 3),
            "gap_percent": round(100.0 * gap_sum / perfect_sum, 3) if perfect_sum else "",
        })

    result.sort(key=lambda row: (
        float(row["gap_percent"]) if row["gap_percent"] != "" else float("inf"),
        float(row["avg_total_miss"]),
    ))
    return result


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        print(f"No successful results to write to {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(rows[0].keys())
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path, help="Path to results.csv created by run_experiments.py")
    parser.add_argument("--output-dir", type=Path, help="Output directory (default: alongside input CSV)")
    parser.add_argument("--top", type=int, default=5, help="How many top configurations to print per workload")
    args = parser.parse_args()

    if not args.results.is_file():
        parser.error(f"Results file does not exist: {args.results}")
    if args.top < 1:
        parser.error("--top must be >= 1")

    rows = read_rows(args.results)
    if not rows:
        print("No successful result rows found.")
        return 1

    algorithms_seen = sorted({alg for row in rows for alg in row["algorithms"].split(">") if alg})
    print(f"Algorithms present in results: {', '.join(algorithms_seen)}")

    output_dir = args.output_dir or args.results.parent
    by_workload = summarize(rows, ("workload", "algorithms", "capacities", "total_capacity"))
    by_all = summarize(rows, ("algorithms", "capacities", "total_capacity"))
    by_algorithm = summarize_by_algorithm(rows)

    workload_path = output_dir / "summary_by_workload.csv"
    overall_path = output_dir / "summary_overall.csv"
    algorithm_path = output_dir / "summary_by_algorithm.csv"
    write_csv(workload_path, by_workload)
    write_csv(overall_path, by_all)
    write_csv(algorithm_path, by_algorithm)

    workloads = sorted({row["workload"] for row in by_workload})
    for workload in workloads:
        print(f"\n=== {workload}: best {args.top} configurations ===")
        candidates = [row for row in by_workload if row["workload"] == workload]
        candidates.sort(key=_sort_key)
        for row in candidates[:args.top]:
            print(
                f"alg={row['algorithms']:<24} cap={row['capacities']:<12} "
                f"avg_total_miss={row['avg_total_miss']:<10} "
                f"avg_perfect_miss={row['avg_perfect_miss']:<10} "
                f"gap={row['avg_gap']:<10} gap%={row['gap_percent']}"
            )

    if by_algorithm:
        print("\n=== Per-algorithm aggregate (across every config that uses it) ===")
        for row in by_algorithm:
            print(
                f"alg={row['algorithm']:<10} configs={row['configs']:<5} "
                f"avg_total_miss={row['avg_total_miss']:<10} "
                f"avg_perfect_miss={row['avg_perfect_miss']:<10} "
                f"gap={row['avg_gap']:<10} gap%={row['gap_percent']}"
            )

    print(f"\nSummary by workload:   {workload_path}")
    print(f"Overall summary:       {overall_path}")
    print(f"Per-algorithm summary: {algorithm_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())