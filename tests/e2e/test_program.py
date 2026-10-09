
#!/usr/bin/env python3
# python3 tests/e2e/test_program.py --binary ./cache -v
import argparse
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


BINARY = None
TIMEOUT = 10


class CacheE2ETests(unittest.TestCase):

    def run_app(self, config, requests):

        with tempfile.TemporaryDirectory() as tmp:
            workdir = Path(tmp)
            (workdir / "config.cfg").write_text(
                config, encoding="utf-8"
            )

            input_data = (
                str(len(requests))
                + "\n"
                + " ".join(map(str, requests))
                + "\n"
            )

            result = subprocess.run(
                [str(BINARY)],
                input=input_data,
                text=True,
                capture_output=True,
                cwd=workdir,
                timeout=TIMEOUT,
                check=False,
            )

            return result

    def assert_misses(self, config, requests, expected):
        result = self.run_app(config, requests)

        self.assertEqual(
            result.returncode,
            0,
            msg=(
                f"Application exited with code {result.returncode}\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

        matches = re.findall(
            r"Misses:\s*(\d+)",
            result.stdout,
        )

        self.assertTrue(
            matches,
            msg=(
                "Could not find 'Misses: N' in stdout.\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            ),
        )

        actual = int(matches[-1])
        self.assertEqual(
            actual,
            expected,
            msg=f"Expected {expected} misses, got {actual}",
        )

    def test_repeated_key(self):
        """Repeated requests for one key should miss only once."""
        self.assert_misses(
            "1\n10 LIRS\n",
            [42, 42, 42, 42, 42],
            1,
        )

    def test_all_keys_are_unique(self):
        """Each first-time request should be a miss."""
        self.assert_misses(
            "1\n10 LIRS\n",
            [1, 2, 3, 4, 5],
            5,
        )

    def test_empty_request_sequence(self):
        """An empty sequence should produce zero misses."""
        self.assert_misses(
            "1\n10 LIRS\n",
            [],
            0,
        )

    def test_negative_and_zero_keys(self):
        """Integer keys should support zero and negative values."""
        self.assert_misses(
            "1\n10 LIRS\n",
            [0, -1, 0, -1],
            2,
        )

    def test_two_level_hierarchy(self):
        """Repeated keys should be handled by a two-level hierarchy."""
        self.assert_misses(
            "2\n10 LIRS\n20 LIRS\n",
            [7, 7, 7],
            2,
        )

    def test_missing_config(self):
        """The application should fail when config.cfg is absent."""
        with tempfile.TemporaryDirectory() as tmp:
            result = subprocess.run(
                [str(BINARY)],
                input="1\n42\n",
                text=True,
                capture_output=True,
                cwd=tmp,
                timeout=TIMEOUT,
                check=False,
            )

        self.assertNotEqual(
            result.returncode,
            0,
            msg="Application unexpectedly succeeded without config.cfg",
        )

    def test_unknown_algorithm(self):
        """An unsupported cache algorithm should be rejected."""
        result = self.run_app(
            "1\n10 UNKNOWN\n",
            [1],
        )

        self.assertNotEqual(
            result.returncode,
            0,
            msg="Application unexpectedly accepted an unknown algorithm",
        )


def main():
    global BINARY

    parser = argparse.ArgumentParser(
        description="End-to-end tests for MIPT_Cache"
    )
    parser.add_argument(
        "--binary",
        required=True,
        help="Path to the compiled application executable",
    )
    args, remaining = parser.parse_known_args()

    binary = Path(args.binary).resolve()

    if not binary.is_file():
        parser.error(f"Executable not found: {binary}")

    BINARY = binary

    # Let unittest handle its own options, such as -v.
    import sys
    sys.argv = [sys.argv[0]] + remaining
    unittest.main()


if __name__ == "__main__":
    main()