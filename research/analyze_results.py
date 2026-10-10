#!/usr/bin/env python3
"""Summarize successful cache experiments using total and perfect-cache misses."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    return [row for row in rows if row.get("status") == "ok" and row.get("total_miss") and row.get("perfect_miss")]


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

    output_dir = args.output_dir or args.results.parent
    by_workload = summarize(rows, ("workload", "algorithms", "capacities", "total_capacity"))
    by_all = summarize(rows, ("algorithms", "capacities", "total_capacity"))
    workload_path = output_dir / "summary_by_workload.csv"
    overall_path = output_dir / "summary_overall.csv"
    write_csv(workload_path, by_workload)
    write_csv(overall_path, by_all)

    workloads = sorted({row["workload"] for row in by_workload})
    for workload in workloads:
        print(f"\n=== {workload}: best {args.top} configurations ===")
        candidates = [row for row in by_workload if row["workload"] == workload]
        candidates.sort(key=lambda row: (
            float(row["gap_percent"]) if row["gap_percent"] != "" else float("inf"),
            float(row["avg_total_miss"]),
        ))
        for row in candidates[:args.top]:
            print(
                f"alg={row['algorithms']:<18} cap={row['capacities']:<12} "
                f"avg_total_miss={row['avg_total_miss']:<10} "
                f"avg_perfect_miss={row['avg_perfect_miss']:<10} "
                f"gap={row['avg_gap']:<10} gap%={row['gap_percent']}"
            )

    print(f"\nSummary by workload: {workload_path}")
    print(f"Overall summary:    {overall_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
