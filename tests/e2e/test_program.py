#!/usr/bin/env python3
# python3 tests/e2e/test_program.py --binary ./cache
import argparse
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BINARY = None
TIMEOUT = 10

# Все алгоритмы, которые обязана поддерживать программа.
ALGORITHMS = ("LIRS", "ARC", "2Q", "LFU")


class CacheE2ETests(unittest.TestCase):

    def run_app(self, config, requests):
        with tempfile.TemporaryDirectory() as tmp:
            workdir = Path(tmp)
            (workdir / "config.cfg").write_text(config, encoding="utf-8")
            input_data = f"{len(requests)}\n{' '.join(map(str, requests))}\n"
            return subprocess.run(
                [str(BINARY), "config.cfg"],
                input=input_data,
                text=True,
                capture_output=True,
                cwd=workdir,
                timeout=TIMEOUT,
                check=False,
            )

    def check(self, config, requests, misses=None, perfect=None, algo=None):
        r = self.run_app(config, requests)
        tag = f"[{algo}] " if algo else ""
        self.assertEqual(
            r.returncode, 0,
            msg=f"{tag}exit={r.returncode}\nstdout:\n{r.stdout}\nstderr:\n{r.stderr}",
        )

        got_misses = re.findall(r"^Misses:\s*(\d+)\s*$", r.stdout, re.M)
        got_perfect = re.findall(r"^Perfect cache misses:\s*(\d+)\s*$", r.stdout, re.M)
        self.assertTrue(got_misses, msg=f"{tag}no 'Misses' line\nstdout:\n{r.stdout}")
        self.assertTrue(got_perfect, msg=f"{tag}no 'Perfect cache misses' line\nstdout:\n{r.stdout}")

        if misses is not None:
            self.assertEqual(
                int(got_misses[-1]), misses,
                msg=f"{tag}cache misses: expected {misses}, got {got_misses[-1]}",
            )
        if perfect is not None:
            self.assertEqual(
                int(got_perfect[-1]), perfect,
                msg=f"{tag}perfect misses: expected {perfect}, got {got_perfect[-1]}",
            )

    def check_all(self, config_tmpl, requests, misses=None, perfect=None):
        """Прогоняет один и тот же сценарий для каждого поддерживаемого алгоритма.

        config_tmpl должен содержать ровно один placeholder `{algo}`.
        misses / perfect могут быть:
          * None  -> не проверяется;
          * int   -> одно и то же ожидание для всех алгоритмов;
          * dict  -> {algo: int}; отсутствующий ключ означает "не проверять".
        """
        for algo in ALGORITHMS:
            with self.subTest(algorithm=algo):
                config = config_tmpl.format(algo=algo)
                m = misses.get(algo) if isinstance(misses, dict) else misses
                p = perfect.get(algo) if isinstance(perfect, dict) else perfect
                self.check(config, requests, misses=m, perfect=p, algo=algo)

    # ---------- базовое поведение (все алгоритмы) ----------

    def test_repeated_key(self):
        self.check_all("1\n10 {algo}\n", [42, 42, 42, 42, 42], misses=1, perfect=1)

    def test_unique_keys(self):
        self.check_all("1\n10 {algo}\n", [1, 2, 3, 4, 5], misses=5, perfect=5)

    def test_empty(self):
        self.check_all("1\n10 {algo}\n", [], misses=0, perfect=0)

    def test_zero_and_negative(self):
        self.check_all("1\n10 {algo}\n", [0, -1, 0, -1], misses=2, perfect=2)

    def test_fits_exactly(self):
        # capacity == рабочему множеству -> у любого алгоритма ровно 3 промаха
        self.check_all("1\n3 {algo}\n", [1, 2, 3, 1, 2, 3], misses=3, perfect=3)

    def test_two_levels(self):
        # Суммарная ёмкость 10 + 20 = 30 -> у perfect-кэша ровно 1 промах.
        # LIRS в этой реализации на первом обращении даёт лишний промах.
        self.check_all(
            "2\n10 {algo}\n20 {algo}\n",
            [7, 7, 7],
            misses={"LIRS": 2, "ARC": 2, "2Q": 2, "LFU": 2},
            perfect=2,
        )

    # ---------- perfect (Belady) ----------

    def test_perfect_belady_trace(self):
        # классика: cap 3, [1,2,3,4,1,2,5,1,2,3,4,5] -> 7 промахов у Belady
        self.check_all(
            "1\n3 {algo}\n",
            [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5],
            perfect=7,
        )

    def test_perfect_small(self):
        # cap 2, [1,2,3,1,2,3] -> 4 промаха у Belady
        self.check_all("1\n2 {algo}\n", [1, 2, 3, 1, 2, 3], perfect=4)

    # ---------- ошибки ----------

    def test_missing_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run(
                [str(BINARY)], input="1\n42\n", text=True,
                capture_output=True, cwd=tmp, timeout=TIMEOUT, check=False,
            )
        self.assertNotEqual(r.returncode, 0)

    def test_unknown_algorithm(self):
        for algo in ("UNKNOWN", "LRU", "FIFO"):
            with self.subTest(algorithm=algo):
                r = self.run_app(f"1\n10 {algo}\n", [1])
                self.assertNotEqual(
                    r.returncode, 0,
                    msg=f"algorithm {algo!r} unexpectedly accepted",
                )


def main():
    global BINARY
    p = argparse.ArgumentParser(description="E2E tests for MIPT_Cache")
    p.add_argument("--binary", required=True)
    args, rest = p.parse_known_args()

    binary = Path(args.binary).resolve()
    if not binary.is_file():
        p.error(f"Executable not found: {binary}")
    BINARY = binary

    sys.argv = [sys.argv[0]] + rest
    unittest.main()


if __name__ == "__main__":
    main()