"""Regression checks for retry handling and benchmark comparisons (stdlib only)."""

import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "analysis", Path(__file__).resolve().parents[2] / "research/analyze_results.py"
)
analysis = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analysis)


def case(**changes):
    return {
        "case_id": "first-binary", "workload": "scan_hot", "seed": "1",
        "request_count": "20000", "keyspace": "512", "algorithms": "ARC",
        "capacities": "32", "total_capacity": "32", "status": "ok",
        "total_miss": "2800", "perfect_miss": "2386", **changes,
    }


class AnalysisTest(unittest.TestCase):
    def read(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.csv"
            with path.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            return analysis.read_rows(path)

    def test_rebuild_retry_replaces_prior_result(self):
        latest = case(case_id="second-binary", total_miss="2810")
        rows = self.read([case(), latest, case(seed="2")])
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0], latest)

    def test_latest_failure_does_not_reuse_old_success(self):
        rows = self.read([case(), case(status="error", total_miss="", perfect_miss="")])
        self.assertEqual(rows, [])

    def test_different_workload_parameters_are_not_retries(self):
        rows = self.read([case(), case(request_count="10000"), case(keyspace="256")])
        self.assertEqual(len(rows), 3)

    def test_seed_range_and_mean_ignore_superseded_attempt(self):
        rows = self.read([
            case(total_miss="9999"), case(total_miss="100"),
            case(seed="2", total_miss="130"), case(seed="3", total_miss="160"),
        ])
        summary = analysis.summarize(rows, (
            "workload", "request_count", "keyspace", "algorithms", "capacities", "total_capacity",
        ))[0]
        self.assertEqual(summary["runs"], 3)
        self.assertEqual(summary["avg_total_miss"], 130)
        self.assertEqual((summary["min_total_miss"], summary["max_total_miss"]), (100, 160))

    def test_budget_winners_preserve_ties_and_separate_level_counts(self):
        rows = [
            case(total_miss="2254", algorithms="ARC"),
            case(total_miss="2254", algorithms="LIRS"),
            case(total_miss="2286", algorithms="2Q"),
            case(total_miss="4508", algorithms="ARC>ARC", capacities="16>16"),
        ]
        summaries = analysis.summarize(rows, (
            "workload", "request_count", "keyspace", "algorithms", "capacities", "total_capacity",
        ))
        winners = analysis.best_by_budget(summaries)
        self.assertEqual({row["algorithms"] for row in winners}, {"ARC", "LIRS", "ARC>ARC"})
        self.assertEqual({row["levels"] for row in winners}, {1, 2})


if __name__ == "__main__":
    unittest.main()
