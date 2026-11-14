#!/usr/bin/env python3
"""Plot workload rankings and algorithm aggregates from analyze_results.py CSVs.

Also accepts the published README snapshot in research/data/. Only rows present
in the input are compared; missing configurations are never filled with zeros.
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter


LEVEL_COLORS = {1: "#007C83", 2: "#4363B5", 3: "#AD5B19"}
PERFECT_COLOR = "#ABB5C5"
WORKLOAD_TITLES = {
    "cyclic": "Циклический обход",
    "phase_shift": "Смена горячего набора",
    "scan_hot": "Горячий набор и одноразовые ключи",
    "uniform": "Равномерные случайные запросы",
    "zipf": "Частые ключи и длинный хвост",
}


def read_summary(path: Path, required: set[str]) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")
        # The analyzer leaves gap_percent empty when perfect_miss is zero.
        rows = [row for row in reader if row["gap_percent"] != ""]
    if not rows:
        raise ValueError(f"{path}: no rows with a defined gap_percent")
    return rows


def number(value: float, decimals: int = 0) -> str:
    return f"{value:,.{decimals}f}".replace(",", " ").replace(".", ",")


def style_axis(ax: plt.Axes, count: int, maximum: float) -> None:
    ax.set_axisbelow(True)
    ax.grid(axis="x", color="#E4E8EF", linewidth=0.8)
    ax.spines[["top", "right", "left"]].set_visible(False)
    ax.spines["bottom"].set_color("#CED5DF")
    ax.tick_params(axis="both", length=0, labelsize=10)
    ax.set_ylim(count - 0.5, -0.5)
    ax.set_xlim(0, maximum * 1.28 if maximum else 1)


def plot_workload(workload: str, rows: list[dict[str, str]], output: Path, top: int) -> None:
    rows = sorted(rows, key=lambda row: (
        float(row["gap_percent"]), float(row["avg_total_miss"]),
        row["algorithms"], row["capacities"],
    ))[:top]
    count = len(rows)
    positions = list(range(count))
    levels = [len(row["algorithms"].split(">")) for row in rows]
    colors = [LEVEL_COLORS.get(level, "#756299") for level in levels]
    gaps = [float(row["gap_percent"]) for row in rows]
    actual = [float(row["avg_total_miss"]) for row in rows]
    perfect = [float(row["avg_perfect_miss"]) for row in rows]
    labels = [
        f"{row['algorithms'].replace('>', ' → ')}\n"
        f"ёмкости: {row['capacities'].replace('>', ' → ')}"
        for row in rows
    ]

    fig, (gap_ax, miss_ax) = plt.subplots(
        1, 2, figsize=(13.5, max(3.8, 1.7 + count * 0.72)),
        gridspec_kw={"width_ratios": [1, 1.3]},
    )
    fig.subplots_adjust(left=0.29, right=0.98, top=0.79, bottom=0.19, wspace=0.18)
    title = WORKLOAD_TITLES.get(workload, workload)
    fig.suptitle(f"{workload} · {title}", x=0.025, y=0.98, ha="left", fontsize=17, weight="bold")
    fig.text(0.025, 0.89, f"Топ-{count} по gap% · порядок уровней: L1 → Ln · меньше — лучше", fontsize=11, color="#526174")

    gap_ax.barh(positions, gaps, color=colors, height=0.48)
    # A marker keeps exact zero results visible without inventing a bar length.
    for position, gap, color in zip(positions, gaps, colors):
        if gap == 0:
            gap_ax.plot(0, position, "o", color=color, markersize=5, clip_on=False)
        gap_ax.annotate(f"{number(gap, 3)}%", (gap, position), xytext=(5, 0),
                        textcoords="offset points", va="center", fontsize=10)
    gap_ax.set_yticks(positions, labels)
    gap_ax.tick_params(axis="y", pad=12)
    gap_ax.set_xlabel("Отклонение от каскада Белади, %", labelpad=10)
    gap_ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}".replace(".", ",")))
    style_axis(gap_ax, count, max(gaps))

    for offset, values, bar_colors in ((-0.17, actual, colors), (0.17, perfect, PERFECT_COLOR)):
        y = [position + offset for position in positions]
        miss_ax.barh(y, values, color=bar_colors, height=0.28)
        for position, value in zip(y, values):
            miss_ax.annotate(number(value, 1), (value, position), xytext=(5, 0),
                             textcoords="offset points", va="center", fontsize=9)
    miss_ax.set_yticks([])
    miss_ax.set_xlabel("Суммарные промахи по всем уровням (среднее)", labelpad=10)
    miss_ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: number(value)))
    style_axis(miss_ax, count, max(actual + perfect))

    handles = [Patch(facecolor=LEVEL_COLORS.get(level, "#756299"), label=f"Уровней: {level}")
               for level in sorted(set(levels))]
    handles.append(Patch(facecolor=PERFECT_COLOR, label="Белади (каскад тех же размеров)"))
    fig.legend(handles=handles, loc="lower center", ncol=len(handles), frameon=False, fontsize=10)
    fig.savefig(output, dpi=160, facecolor="white")
    plt.close(fig)


def plot_algorithms(rows: list[dict[str, str]], output: Path) -> None:
    rows = sorted(rows, key=lambda row: (float(row["gap_percent"]), row["algorithm"]))
    gaps = [float(row["gap_percent"]) for row in rows]
    labels = [f"{row['algorithm']}  ·  {int(row['configs']):,} прогонов".replace(",", " ") for row in rows]
    fig, ax = plt.subplots(figsize=(11, max(3.8, 1.8 + len(rows) * 0.65)))
    fig.subplots_adjust(left=0.25, right=0.97, top=0.74, bottom=0.16)
    fig.suptitle("Общий результат конфигураций с каждым алгоритмом", x=0.025, y=0.97,
                 ha="left", fontsize=16, weight="bold")
    fig.text(0.025, 0.86, "Группы пересекаются: смешанная цепочка входит сразу в несколько групп.",
             fontsize=11, color="#526174")
    positions = list(range(len(rows)))
    ax.barh(positions, gaps, height=0.5, color=["#007C83"] + ["#7085A5"] * (len(rows) - 1))
    ax.set_yticks(positions, labels)
    for position, gap in zip(positions, gaps):
        ax.annotate(f"{number(gap, 3)}%", (gap, position), xytext=(7, 0),
                    textcoords="offset points", va="center", fontsize=11)
    ax.set_xlabel("Отклонение от каскада Белади, % · меньше — лучше", labelpad=10)
    style_axis(ax, len(rows), max(gaps))
    fig.savefig(output, dpi=160, facecolor="white")
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("by_workload", type=Path, help="summary_by_workload.csv or published snapshot")
    parser.add_argument("by_algorithm", type=Path, help="summary_by_algorithm.csv or published snapshot")
    parser.add_argument("--output-dir", type=Path, default=Path("research/plots"))
    parser.add_argument("--top", type=int, default=5, help="Configurations per workload (default: 5)")
    args = parser.parse_args()
    if args.top < 1:
        parser.error("--top must be >= 1")
    try:
        workloads = read_summary(args.by_workload, {
            "workload", "algorithms", "capacities", "gap_percent", "avg_total_miss", "avg_perfect_miss",
        })
        algorithms = read_summary(args.by_algorithm, {"algorithm", "configs", "gap_percent"})
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    plt.rcParams.update({"font.family": "DejaVu Sans", "text.color": "#213047", "axes.labelcolor": "#213047"})
    args.output_dir.mkdir(parents=True, exist_ok=True)
    grouped = defaultdict(list)
    for row in workloads:
        grouped[row["workload"]].append(row)
    for workload, rows in sorted(grouped.items()):
        # Workload names are also filenames; reject path components from CSV.
        if not workload or any(char not in "abcdefghijklmnopqrstuvwxyz0123456789_-" for char in workload):
            parser.error(f"Invalid workload name: {workload!r}")
        path = args.output_dir / f"top_{workload}.png"
        plot_workload(workload, rows, path, args.top)
        print(path)
    path = args.output_dir / "algorithm_gap.png"
    plot_algorithms(algorithms, path)
    print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
