"""Tests for the benchmark's measurement contract and rejection of bad samples."""

import copy
import json
from pathlib import Path
import tempfile
import unittest

import profile_turns as turns


class TurnBenchmarks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = turns.source_contract(Path(__file__).resolve().parents[1] / "src")

    def test_deterministic_fixtures_and_start_tiles(self):
        for case in turns.BENCHMARKS:
            with self.subTest(case=case.name):
                fixture = turns.make_fixture(case, self.contract)
                self.assertEqual(fixture, turns.make_fixture(case, self.contract))
                self.assertEqual(len(fixture), 821)
                self.assertEqual(fixture[783], self.contract["version"])
                self.assertEqual(fixture[801], 0)
                x, y = fixture[786:788]
                self.assertFalse(fixture[(y * 64 + x) // 8] & (1 << (x & 7)))
                self.assertEqual(fixture[810:815], b"\xff" * 5)

    def test_hit_and_miss_seeds_exercise_different_paths(self):
        for name, expected in (("attack_hit", True), ("attack_kill", True), ("attack_miss", False)):
            case = next(case for case in turns.BENCHMARKS if case.name == name)
            fixture = turns.make_fixture(case, self.contract)
            seed = int.from_bytes(fixture[776:778], "little")
            self.assertEqual(turns.next_random(seed) % (fixture[797] * 2 + 5) >= 4, expected)

    def test_dense_workload_really_has_twelve_enemies(self):
        case = next(case for case in turns.BENCHMARKS if case.name == "wait_dense")
        fixture = turns.make_fixture(case, self.contract)
        self.assertEqual(sum(bool(fixture[584 + 8 * index]) for index in range(12)), 12)
        self.assertEqual(fixture[809], 2)
        self.assertEqual(fixture[256:512], b"\xff" * 256)

    def test_real_inputs_prepare_ui_and_measure_only_final_input(self):
        for case in turns.BENCHMARKS:
            commands = turns.make_commands(case, self.contract, Path("folder with spaces"))
            before, measurement = commands.split("avm profile start", 1)
            self.assertIn("avm button set A", before)  # Start through the real title UI.
            self.assertNotIn("expr -- rogue::ui", commands)  # LTO may fragment Ui globals.
            self.assertIn("avm button set " + case.button, measurement)
            self.assertIn("avm profile stop", measurement)
            self.assertIn('"folder with spaces/turn.avmp"', measurement)
            self.assertNotIn("avm replay", measurement)
            self.assertEqual(measurement.count("avm button set"), 1)

    def test_suite_excludes_generation_and_animation(self):
        self.assertEqual(len(turns.BENCHMARKS), 21)
        names = {case.name for case in turns.BENCHMARKS}
        self.assertTrue(names.isdisjoint({"descend", "throw_harming", "wand_force", "wand_fire"}))

    def test_cursed_amulet_requires_new_discovery_and_equipping(self):
        case = next(case for case in turns.BENCHMARKS if case.name == "equip_cursed_amulet")
        before = turns.make_fixture(case, self.contract)
        offset, mask = turns.amulet_knowledge_bit(case.item, self.contract)
        after = bytearray(before)
        after[801] = 1
        after[812] = 0
        after[743] |= 0x80
        after[offset] |= mask
        turns.validate_outcome(case, before, after, self.contract)
        for field, value in ((812, 0xff), (743, 0x41), (offset, before[offset])):
            with self.subTest(missing_discovery_field=field):
                invalid = bytearray(after)
                invalid[field] = value
                with self.assertRaisesRegex(ValueError, "curse discovered"):
                    turns.validate_outcome(case, before, invalid, self.contract)
        known_before = bytearray(before)
        known_before[743] |= 0x80
        with self.assertRaisesRegex(ValueError, "curse unknown"):
            turns.validate_outcome(case, known_before, after, self.contract)

    def sample(self, folder):
        case = turns.BENCHMARKS[0]
        before = turns.make_fixture(case, self.contract)
        (folder / "fixture.bin").write_bytes(before)
        after = bytearray(before)
        after[786] += 1
        after[801] = 1
        (folder / "before.bin").write_bytes(before)
        (folder / "after.bin").write_bytes(after)
        profile = {
            "window": {"complete": True, "stop_reason": "breakpoint", "partial_cycles": "0",
                       "discontinuities": "0", "start_cycle": "100", "end_cycle": "2400100"},
            "pcs": [{"linkage": "_ZN5rogue6renderEv", "function": "rogue::render()", "cycles": "2400000"}],
            "identity": {}, "end_display_hash": "fixture",
        }
        records = [
            {"seconds_since_reset": 0, "cycles": 50, "pc": 1916},
            {"seconds_since_reset": 0, "cycles": 100, "pc": 1916},
            {"cycle": 100, "pressed_mask": 2},
            {"requested_cycles": 160000000, "reason": "breakpoint"},
            {"seconds_since_reset": 0, "cycles": 2400100, "pc": 1916},
        ]
        values = "\n".join(f"(unsigned int) ${index} = {value}" for index, (_, value) in enumerate(turns.layout_checks()))
        return case, profile, records, values

    def validate(self, folder, case, profile, records, values):
        (folder / "turn.avmp").write_text(json.dumps(profile), encoding="utf-8")
        output = values + "\n" + "\n".join(json.dumps(record) for record in records)
        return turns.validate_sample(case, self.contract, folder, output)

    def test_exact_150ms_cycle_conversion(self):
        with tempfile.TemporaryDirectory() as raw:
            folder = Path(raw)
            case, profile, records, values = self.sample(folder)
            sample = self.validate(folder, case, profile, records, values)
            self.assertEqual(sample["cycles"], 2400000)
            self.assertEqual(sample["ms"], turns.DEFAULT_GOAL_MS)

    def test_incomplete_or_wrong_endpoint_never_counts_as_fast_turn(self):
        with tempfile.TemporaryDirectory() as raw:
            folder = Path(raw)
            case, profile, records, values = self.sample(folder)
            changes = (
                ("partial interval", lambda p, r: p["window"].update(partial_cycles="1")),
                ("PC discontinuity", lambda p, r: p["window"].update(discontinuities="1")),
                ("incomplete", lambda p, r: p["window"].update(complete=False)),
                ("wrong final PC", lambda p, r: r[-1].update(pc=123)),
                ("deadline", lambda p, r: r[-2].update(reason="deadline")),
                ("missing final render", lambda p, r: p["pcs"][0].update(linkage="main")),
                ("mismatched cycles", lambda p, r: p["pcs"][0].update(cycles="1")),
                ("missing input", lambda p, r: r[2].update(pressed_mask=0)),
                ("extra input", lambda p, r: r.insert(3, {"cycle": 200, "pressed_mask": 16})),
                ("animation", lambda p, r: p["pcs"][0].update(function="rogue::animate_ray()")),
                ("generation", lambda p, r: p["pcs"].append({"linkage": "_ZN5rogue10make_floorEv", "function": "rogue::make_floor()", "cycles": "0"})),
            )
            for name, change in changes:
                with self.subTest(reason=name):
                    p, r = copy.deepcopy(profile), copy.deepcopy(records)
                    change(p, r)
                    with self.assertRaises(ValueError):
                        self.validate(folder, case, p, r, values)
            after = bytearray((folder / "after.bin").read_bytes())
            after[801] = 0
            (folder / "after.bin").write_bytes(after)
            with self.assertRaisesRegex(ValueError, "completed game turn"):
                self.validate(folder, case, profile, records, values)


if __name__ == "__main__":
    unittest.main()
