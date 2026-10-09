#!/usr/bin/env python3
# python3 tests/e2e/test_program.py --binary ./cache -v
import argparse
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BINARY = None
TIMEOUT = 10


class CacheE2ETests(unittest.TestCase):

    def run_app(self, config, requests):
        with tempfile.TemporaryDirectory() as tmp:
            workdir = Path(tmp)
            (workdir / "config.cfg").write_text(config, encoding="utf-8")
            input_data = f"{len(requests)}\n{' '.join(map(str, requests))}\n"
            return subprocess.run(
                [str(BINARY)],
                input=input_data,
                text=True,
                capture_output=True,
                cwd=workdir,
                timeout=TIMEOUT,
                check=False,
            )

    def check(self, config, requests, misses=None, perfect=None):
        r = self.run_app(config, requests)
        self.assertEqual(
            r.returncode, 0,
            msg=f"exit={r.returncode}\nstdout:\n{r.stdout}\nstderr:\n{r.stderr}",
        )

        got_misses = re.findall(r"^Misses:\s*(\d+)\s*$", r.stdout, re.M)
        got_perfect = re.findall(r"^Perfect cache misses:\s*(\d+)\s*$", r.stdout, re.M)
        self.assertTrue(got_misses, msg=f"no 'Misses' line\nstdout:\n{r.stdout}")
        self.assertTrue(got_perfect, msg=f"no 'Perfect cache misses' line\nstdout:\n{r.stdout}")

        if misses is not None:
            self.assertEqual(int(got_misses[-1]), misses,
                             msg=f"LIRS misses: expected {misses}, got {got_misses[-1]}")
        if perfect is not None:
            self.assertEqual(int(got_perfect[-1]), perfect,
                             msg=f"perfect misses: expected {perfect}, got {got_perfect[-1]}")

    # ---------- basic LIRS ----------

    def test_repeated_key(self):
        self.check("1\n10 LIRS\n", [42, 42, 42, 42, 42], misses=1, perfect=1)

    def test_unique_keys(self):
        self.check("1\n10 LIRS\n", [1, 2, 3, 4, 5], misses=5, perfect=5)

    def test_empty(self):
        self.check("1\n10 LIRS\n", [], misses=0, perfect=0)

    def test_zero_and_negative(self):
        self.check("1\n10 LIRS\n", [0, -1, 0, -1], misses=2, perfect=2)

    def test_two_levels(self):
        # total perfect capacity = 10 + 20 = 30 -> single miss for [7,7,7]
        self.check("2\n10 LIRS\n20 LIRS\n", [7, 7, 7], misses=2, perfect=1)

    # ---------- perfect (Belady) cache ----------

    def test_perfect_belady_trace(self):
        # classic: cap 3, [1,2,3,4,1,2,5,1,2,3,4,5] -> 7 misses
        self.check(
            "1\n3 LIRS\n",
            [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5],
            perfect=7,
        )

    def test_perfect_small(self):
        # cap 2, [1,2,3,1,2,3] -> 4 misses; LIRS result is impl-specific
        self.check("1\n2 LIRS\n", [1, 2, 3, 1, 2, 3], perfect=4)

    # ---------- error paths ----------

    def test_missing_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            r = subprocess.run(
                [str(BINARY)], input="1\n42\n", text=True,
                capture_output=True, cwd=tmp, timeout=TIMEOUT, check=False,
            )
        self.assertNotEqual(r.returncode, 0)

    def test_unknown_algorithm(self):
        r = self.run_app("1\n10 UNKNOWN\n", [1])
        self.assertNotEqual(r.returncode, 0)


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