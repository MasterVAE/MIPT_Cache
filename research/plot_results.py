#!/usr/bin/env python3
"""Plot equal-capacity comparisons and optional gap rankings from summary CSVs.

Only rows present in the input are compared; missing configurations are never
filled with zeros. The main comparisons require the full experiment summary.
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
from matplotlib.ticker import FuncFormatter, MaxNLocator


LEVEL_COLORS = {1: "#007C83", 2: "#4363B5", 3: "#AD5B19"}
PERFECT_COLOR = "#ABB5C5"
ALGORITHM_COLORS = {"LIRS": "#007C83", "ARC": "#4363B5", "2Q": "#AD5B19", "LFU": "#9061AD"}
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


def plot_single_level(workload: str, rows: list[dict[str, str]], output: Path) -> None:
    rows = [row for row in rows if ">" not in row["algorithms"]]
    if not rows:
        return
    capacities = sorted({int(row["total_capacity"]) for row in rows})
    algorithms = [algorithm for algorithm in ALGORITHM_COLORS
                  if any(row["algorithms"] == algorithm for row in rows)]
    indexed = {(int(row["total_capacity"]), row["algorithms"]): row for row in rows}
    if len(indexed) != len(rows):
        raise ValueError(f"{workload}: multiple workload settings; plot one request_count/keyspace at a time")
    if len(indexed) != len(capacities) * len(algorithms):
        raise ValueError(f"{workload}: incomplete single-level comparison")
    runs = sorted({int(row["runs"]) for row in rows})
    if len(runs) != 1:
        raise ValueError(f"{workload}: inconsistent seed counts")
    request_counts = {int(row["request_count"]) for row in rows}
    if len(request_counts) != 1:
        raise ValueError(f"{workload}: inconsistent request counts")
    request_count = request_counts.pop()
    maximum = max(float(row["max_total_miss"]) for row in rows)
    fig, axes = plt.subplots(1, len(capacities), figsize=(13.5, 5.2), squeeze=False, sharex=True)
    fig.subplots_adjust(left=0.08, right=0.98, top=0.75, bottom=0.22, wspace=0.18)
    fig.suptitle(f"{workload} · {WORKLOAD_TITLES.get(workload, workload)}",
                 x=0.025, y=0.98, ha="left", fontsize=17, weight="bold")
    fig.text(0.025, 0.88,
             f"Один уровень · {number(request_count)} запросов · среднее по {runs[0]} seed · меньше — лучше",
             fontsize=11, color="#526174")
    labels = algorithms + ["Белади"]
    for ax, capacity in zip(axes[0], capacities):
        selected = [indexed[(capacity, algorithm)] for algorithm in algorithms]
        baselines = {float(row["avg_perfect_miss"]) for row in selected}
        if len(baselines) != 1:
            raise ValueError(f"{workload}: inconsistent perfect-cache baseline at capacity {capacity}")
        values = [float(row["avg_total_miss"]) for row in selected] + [baselines.pop()]
        positions = list(range(len(labels)))
        ax.barh(positions, values, color=[ALGORITHM_COLORS[a] for a in algorithms] + [PERFECT_COLOR], height=0.5)
        lower = [float(row["min_total_miss"]) for row in selected] + [float(selected[0]["min_perfect_miss"])]
        upper = [float(row["max_total_miss"]) for row in selected] + [float(selected[0]["max_perfect_miss"])]
        ax.errorbar(values, positions, xerr=[
            [max(0, mean - low) for mean, low in zip(values, lower)],
            [max(0, high - mean) for mean, high in zip(values, upper)],
        ], fmt="none", ecolor="#213047", elinewidth=1, capsize=3)
        for position, value in zip(positions, values):
            ax.annotate(number(value, 1), (value, position), xytext=(5, 8),
                        textcoords="offset points", va="center", fontsize=9)
        ax.set_yticks(positions, labels if capacity == capacities[0] else [""] * len(labels))
        ax.set_title(f"Ёмкость: {capacity}", fontsize=12, pad=16, weight="bold")
        ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: number(value)))
        ax.xaxis.set_major_locator(MaxNLocator(nbins=4))
        style_axis(ax, len(labels), maximum)
    fig.text(0.5, 0.09, "Число промахов · засечки: минимум–максимум по seed (не доверительный интервал)",
             ha="center", fontsize=11, color="#526174")
    fig.savefig(output, dpi=160, facecolor="white")
    plt.close(fig)


def plot_hierarchy(rows: list[dict[str, str]], output: Path) -> None:
    workloads = [workload for workload in WORKLOAD_TITLES if any(row["workload"] == workload for row in rows)]
    capacities = sorted({int(row["total_capacity"]) for row in rows})
    levels = sorted({len(row["algorithms"].split(">")) for row in rows})
    fig, axes = plt.subplots(len(workloads), len(capacities), figsize=(13.5, 12), squeeze=False)
    fig.subplots_adjust(left=0.13, right=0.98, top=0.88, bottom=0.08, wspace=0.12, hspace=0.45)
    fig.suptitle("Как число уровней меняет сумму промахов", x=0.025, y=0.98,
                 ha="left", fontsize=17, weight="bold")
    fig.text(0.025, 0.94, "В каждой группе выбран минимум среднего total_miss среди проверенных конфигураций.",
             fontsize=11, color="#526174")
    for row_index, workload in enumerate(workloads):
        maximum = 0.0
        for column, capacity in enumerate(capacities):
            ax = axes[row_index][column]
            for position, level in enumerate(levels):
                candidates = [row for row in rows if row["workload"] == workload
                              and int(row["total_capacity"]) == capacity
                              and len(row["algorithms"].split(">")) == level]
                if not candidates:
                    continue
                best = min(candidates, key=lambda row: (float(row["avg_total_miss"]), row["algorithms"], row["capacities"]))
                value = float(best["avg_total_miss"])
                maximum = max(maximum, value)
                ax.barh(position, value, height=0.55, color=LEVEL_COLORS.get(level, "#756299"))
                ax.annotate(number(value, 1), (value, position), xytext=(5, 0),
                            textcoords="offset points", va="center", fontsize=9)
            ax.set_yticks(range(len(levels)), [f"Уровней: {level}" for level in levels] if column == 0 else [""] * len(levels))
            if column == 0:
                ax.set_ylabel(workload, fontsize=12, weight="bold", labelpad=15)
            if row_index == 0:
                ax.set_title(f"Общая ёмкость: {capacity}", fontsize=12, pad=16, weight="bold")
            ax.xaxis.set_major_formatter(FuncFormatter(lambda value, _: number(value)))
            ax.xaxis.set_major_locator(MaxNLocator(nbins=4))
        for ax in axes[row_index]:
            style_axis(ax, len(levels), maximum)
    fig.text(0.5, 0.025, "Суммарные промахи по всем уровням · среднее по seed · меньше — лучше",
             ha="center", fontsize=11, color="#526174")
    fig.savefig(output, dpi=160, facecolor="white")
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("by_workload", type=Path, help="summary_by_workload.csv")
    parser.add_argument("by_algorithm", type=Path, help="summary_by_algorithm.csv")
    parser.add_argument("--output-dir", type=Path, default=Path("research/plots"))
    parser.add_argument("--top", type=int, default=5, help="Configurations per workload (default: 5)")
    parser.add_argument("--rankings", action="store_true", help="Also write top-N gap and overlapping algorithm-group plots")
    args = parser.parse_args()
    if args.top < 1:
        parser.error("--top must be >= 1")
    try:
        workloads = read_summary(args.by_workload, {
            "workload", "algorithms", "capacities", "gap_percent", "avg_total_miss", "avg_perfect_miss",
            "request_count", "total_capacity", "runs", "min_total_miss", "max_total_miss",
            "min_perfect_miss", "max_perfect_miss",
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
        path = args.output_dir / f"single_{workload}.png"
        plot_single_level(workload, rows, path)
        print(path)
        if args.rankings:
            path = args.output_dir / f"top_{workload}.png"
            plot_workload(workload, rows, path, args.top)
            print(path)
    path = args.output_dir / "hierarchy_best.png"
    plot_hierarchy(workloads, path)
    print(path)
    if args.rankings:
        path = args.output_dir / "algorithm_gap.png"
        plot_algorithms(algorithms, path)
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
