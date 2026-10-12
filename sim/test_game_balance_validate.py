#!/usr/bin/env python3
"""Exercise streamed census validation with real sidecars and damaged counters."""
import csv
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from game_balance_validate import validate

SIMULATOR = sys.argv.pop(1)


def rewrite(path, edit):
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        fields, rows = reader.fieldnames, list(reader)
    edit(rows)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        writer.writerows(rows)


class StreamingValidation(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.directory = Path(self.scratch.name) / "batch"
        subprocess.run([SIMULATOR, "--agent", "explorer-v1", "--seeds", "1:2", "--jobs", "2",
                        "--max-actions", "100000", "--output", str(self.directory)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    def test_real_parallel_sidecars(self):
        result = validate(self.directory)
        self.assertEqual(result["runs"], 2)
        self.assertTrue(result["counter_reconciliation"])
        self.assertGreater(result["row_counts"]["visit_items"], 0)
        self.assertTrue(result["exposure"])

    def test_new_effect_counter_mismatch(self):
        rewrite(self.directory / "items.csv", lambda rows: rows[0].update(active_turns=str(int(rows[0]["active_turns"]) + 1)))
        with self.assertRaisesRegex(ValueError, "counter mismatch"):
            validate(self.directory)

    def test_duplicate_typed_visit(self):
        rewrite(self.directory / "visit_items.csv", lambda rows: rows.insert(1, dict(rows[0])))
        with self.assertRaisesRegex(ValueError, "duplicate or invalid typed visit"):
            validate(self.directory)

    def test_wrong_visit_key(self):
        rewrite(self.directory / "visit_items.csv", lambda rows: rows[0].update(visit="999"))
        with self.assertRaisesRegex(ValueError, "duplicate or invalid typed visit"):
            validate(self.directory)

    def test_invalid_outcome(self):
        rewrite(self.directory / "runs.csv", lambda rows: rows[0].update(result="SIM_STUCK", stuck="1"))
        with self.assertRaisesRegex(ValueError, "invalid run outcome"):
            validate(self.directory)


if __name__ == "__main__":
    unittest.main()
