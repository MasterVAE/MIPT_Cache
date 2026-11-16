#!/usr/bin/env python3
"""Summarize successful cache experiments using total and perfect-cache misses.

Produces four summary tables:
  * summary_by_workload.csv  - means and ranges per workload and configuration
  * summary_overall.csv      - means per configuration across workloads
  * summary_by_algorithm.csv - aggregate per individual algorithm, regardless of
                               its position in a multi-level stack or the other
                               algorithms it was combined with.
  * summary_best_by_budget.csv - minimum mean total_miss per workload/budget/level count
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


CASE_FIELDS = (
    "workload", "seed", "request_count", "keyspace",
    "algorithms", "capacities", "total_capacity",
)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        required = set(CASE_FIELDS) | {"status", "total_miss", "perfect_miss"}
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")
        rows = list(reader)
    # case_id includes a binary signature, so it changes on rebuilds. Use the
    # experiment parameters to identify retries, keeping the last attempt even
    # if it failed: an earlier successful binary must not silently stand in.
    latest = {tuple(row[field] for field in CASE_FIELDS): row for row in rows}
    successful = [
        row for row in latest.values()
        if row["status"] == "ok" and row["total_miss"] and row["perfect_miss"]
    ]
    print(
        f"CSV rows: {len(rows)}; superseded attempts: {len(rows) - len(latest)}; "
        f"latest successful cases: {len(successful)}; "
        f"latest failed/incomplete cases: {len(latest) - len(successful)}"
    )
    return successful


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
        totals = [int(row["total_miss"]) for row in group]
        perfects = [int(row["perfect_miss"]) for row in group]
        gap_sum = total_sum - perfect_sum
        item: dict[str, object] = {field: value for field, value in zip(group_fields, key)}
        item.update({
            "runs": total_count,
            "avg_total_miss": round(total_sum / total_count, 3),
            "min_total_miss": min(totals),
            "max_total_miss": max(totals),
            "avg_perfect_miss": round(perfect_sum / total_count, 3),
            "min_perfect_miss": min(perfects),
            "max_perfect_miss": max(perfects),
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


def best_by_budget(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    grouped = defaultdict(list)
    for row in rows:
        levels = len(str(row["algorithms"]).split(">"))
        key = tuple(row[field] for field in ("workload", "request_count", "keyspace", "total_capacity")) + (levels,)
        grouped[key].append({**row, "levels": levels})
    winners = []
    for group in grouped.values():
        minimum = min(float(row["avg_total_miss"]) for row in group)
        # Preserve all ties; ordering must not invent a unique winning algorithm.
        winners.extend(row for row in group if float(row["avg_total_miss"]) == minimum)
    return sorted(winners, key=lambda row: (
        row["workload"], row["request_count"], row["keyspace"],
        int(row["total_capacity"]), row["levels"], row["algorithms"], row["capacities"],
    ))


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

    try:
        rows = read_rows(args.results)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    if not rows:
        print("No successful result rows found.")
        return 1

    algorithms_seen = sorted({alg for row in rows for alg in row["algorithms"].split(">") if alg})
    print(f"Algorithms present in results: {', '.join(algorithms_seen)}")

    output_dir = args.output_dir or args.results.parent
    by_workload = summarize(rows, (
        "workload", "request_count", "keyspace", "algorithms", "capacities", "total_capacity",
    ))
    by_all = summarize(rows, (
        "request_count", "keyspace", "algorithms", "capacities", "total_capacity",
    ))
    by_algorithm = summarize_by_algorithm(rows)

    workload_path = output_dir / "summary_by_workload.csv"
    overall_path = output_dir / "summary_overall.csv"
    algorithm_path = output_dir / "summary_by_algorithm.csv"
    best_path = output_dir / "summary_best_by_budget.csv"
    write_csv(workload_path, by_workload)
    write_csv(overall_path, by_all)
    write_csv(algorithm_path, by_algorithm)
    write_csv(best_path, best_by_budget(by_workload))

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
    print(f"Best by memory budget and level count: {best_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
