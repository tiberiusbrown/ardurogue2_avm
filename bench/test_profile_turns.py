"""Tests for compiled case selection and rejection of invalid measurements."""

import copy
import json
from pathlib import Path
import tempfile
import unittest

import profile_turns as turns


class TurnBenchmarks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = Path(__file__).resolve().parents[1] / "src"
        cls.contract = turns.source_contract(cls.source)

    def test_manifest_is_owned_by_cpp(self):
        cases = turns.read_benchmarks(Path(__file__).with_name("bench.cpp"))
        self.assertEqual(cases, turns.BENCHMARKS)
        self.assertEqual([case.index for case in cases], list(range(len(cases))))
        self.assertEqual(len(cases), 42)
        names = {case.name for case in cases}
        self.assertIn("equip_cursed_amulet", names)
        self.assertTrue(names.isdisjoint({"descend", "throw_harming", "wand_force", "wand_fire"}))
        self.assertEqual(sum(case.setup in ("browse", "projectile_browse") for case in cases), 10)

    def test_contract_needs_no_save_definition(self):
        with tempfile.TemporaryDirectory() as raw:
            project = Path(raw)
            (project / "src").mkdir()
            (project / "bench").mkdir()
            for name in ("main.cpp", "ui.cpp"):
                (project / "src" / name).write_bytes((self.source / name).read_bytes())
            (project / "bench/bench.cpp").write_bytes(Path(__file__).with_name("bench.cpp").read_bytes())
            self.assertEqual(turns.source_contract(project / "src"), self.contract)
            self.assertFalse((project / "src/model.hpp").exists())

    def test_real_inputs_prepare_ui_and_measure_only_final_input(self):
        for case in turns.BENCHMARKS:
            with self.subTest(case=case.name):
                commands = turns.make_commands(case, self.contract, Path("folder with spaces"))
                before, measurement = commands.split("avm profile start", 1)
                self.assertIn("breakpoint set --name bench_select", before)
                self.assertIn(f"bench_case = {case.index}", before)
                self.assertIn("breakpoint set --name bench_" + case.name, before)
                self.assertNotIn("memory write", commands)
                self.assertNotIn("fixture.bin", commands)
                self.assertNotIn("SAVE_VERSION", commands)
                self.assertNotIn("expr -- rogue::ui", commands)
                self.assertIn("avm button set " + case.button, measurement)
                self.assertIn('"folder with spaces/turn.avmp"', measurement)
                self.assertEqual(measurement.count("avm button set"), 1)
                self.assertNotIn("avm replay", measurement)

    def test_inventory_preparation_uses_buttons_and_keeps_modal_endpoint(self):
        for case in turns.BENCHMARKS:
            if case.setup != "browse":
                continue
            with self.subTest(case=case.name):
                commands = turns.make_commands(case, self.contract, Path("folder"))
                before, measurement = commands.split("avm profile start", 1)
                self.assertIn(f"breakpoint set --file ui.cpp --line {self.contract['item_idle']}", before)
                self.assertNotIn("breakpoint enable 3", before)
                self.assertEqual(before.count("avm button set DOWN\n"), 1 + case.down)
                self.assertEqual(before.count("avm button set UP\n"), case.up)
                self.assertEqual(before.count("avm button set A\n"), int(case.button != "A"))
                self.assertEqual(measurement.count("avm button set"), 1)

    def test_cursed_amulet_requires_new_discovery_and_equipping(self):
        case = next(case for case in turns.BENCHMARKS if case.name == "equip_cursed_amulet")
        before = dict(valid=1, hp=100, turns=0, x=32, y=16, expected_item=1,
                      amulet_empty=1, cursed=1, identified=0, magnitude=1, amulet_known=0)
        after = dict(before, turns=1, amulet=0, amulet_empty=0, identified=1, amulet_known=1)
        turns.validate_outcome(case, before, after, b"")
        for key, value in (("amulet", 255), ("cursed", 0), ("identified", 0), ("amulet_known", 0)):
            with self.subTest(missing=key), self.assertRaisesRegex(ValueError, "curse discovered"):
                turns.validate_outcome(case, before, dict(after, **{key: value}), b"")
        with self.assertRaisesRegex(ValueError, "curse unknown"):
            turns.validate_outcome(case, dict(before, identified=1), after, b"")

    def sample(self):
        case = turns.BENCHMARKS[0]
        before = {field: 0 for field in turns.state_fields(case)}
        before.update(valid=1, hp=100, x=32, y=16)
        after = dict(before, x=33, turns=1)
        profile = {
            "window": {"complete": True, "stop_reason": "breakpoint", "partial_cycles": "0",
                       "discontinuities": "0", "start_cycle": "100", "end_cycle": "1600100"},
            "pcs": [{"linkage": "_ZN5rogue6renderEv", "function": "rogue::render()", "cycles": "1600000"}],
            "identity": {}, "end_display_hash": "fixture",
        }
        records = [
            {"seconds_since_reset": 0, "cycles": 50, "pc": 1916},
            {"seconds_since_reset": 0, "cycles": 100, "pc": 1916},
            {"cycle": 100, "pressed_mask": 2},
            {"requested_cycles": 160000000, "reason": "breakpoint"},
            {"seconds_since_reset": 0, "cycles": 1600100, "pc": 1916},
        ]
        return case, before, after, profile, records

    def validate(self, folder, case, before, after, profile, records):
        (folder / "turn.avmp").write_text(json.dumps(profile), encoding="utf-8")
        (folder / "explored.bin").write_bytes(b"\xff" * 256)
        values = [case.index, case.index, *before.values(), *after.values()]
        output = "\n".join(f"(unsigned int) ${index} = {value}" for index, value in enumerate(values))
        output += "\n" + "\n".join(json.dumps(record) for record in records)
        if case.setup == "browse":
            output += "\nBreakpoint 4: where = inventory modal, address = 0x00000dac\n"
            before_frame, after_frame = self.browse_frames(case)
            for name, pixels in (("before.pgm", before_frame), ("frame.pgm", after_frame)):
                (folder / name).write_bytes(b"P5\n128 64\n255\n" + pixels)
        return turns.validate_sample(case, self.contract, folder, output)

    def browse_frames(self, case):
        def frame(top, selected):
            pixels = bytearray(128 * 64)
            for row in range(7):
                # Distinct row content lets the validator detect incorrect scrolling.
                for bit in range(5):
                    if (top + row) & (1 << bit):
                        pixels[(14 + row * 7) * 128 + 10 + bit] = 255
                if row == selected:
                    for y in range(12 + row * 7, 19 + row * 7):
                        for x in range(7, 128):
                            offset = y * 128 + x
                            pixels[offset] = 255 - pixels[offset]
            return bytes(pixels)
        if case.button == "A":
            return bytes(128 * 64), frame(0, 1)
        return frame(9, 6), frame(10, 6)

    def browse_sample(self, name="inventory_down_wands"):
        case, _, _, profile, records = self.sample()
        case = next(case for case in turns.BENCHMARKS if case.name == name)
        before = {field: 0 for field in turns.state_fields(case)}
        before.update(valid=1, hp=100, x=32, y=16, pack_size=16, visible_rows=7, group_count=9)
        for group in range(7):
            before[f"group_first_{group}"] = group + 1
            before[f"group_last_{group}"] = group + 1
            before[f"group_id_{group}"] = group
        before.update(food_type=8, food_group=7, quest_group=8)
        if case.button == "A":
            for group in range(9):
                slot = 15 - group
                before[f"pack_type_{slot}"] = group + 1
        else:
            for slot in range(16):
                before[f"pack_type_{slot}"] = 50
                before[f"pack_info_{slot}"] = 0xdf
            before.update(group_first_4=50, group_last_4=50)
            records[1]["pc"] = 3500
        after = dict(before)
        records[-1]["pc"] = 3500
        profile["pcs"][0].update(linkage="_ZN5rogue16render_inventoryE", function="rogue::render_inventory()")
        return case, before, after, profile, records

    def test_inventory_opening_and_scrolling_validate_modal_render_without_a_turn(self):
        with tempfile.TemporaryDirectory() as raw:
            for name in ("inventory_open_singletons", "inventory_down_wands"):
                with self.subTest(case=name):
                    sample = self.validate(Path(raw), *self.browse_sample(name))
                    self.assertEqual(sample["ms"], 100.0)

    def test_inventory_rejects_wrong_selection_viewport_mutation_and_endpoint(self):
        with tempfile.TemporaryDirectory() as raw:
            folder = Path(raw)
            case, before, after, profile, records = self.browse_sample()
            for key, value in (("pack_info_15", 0), ("turns", 1)):
                with self.subTest(field=key), self.assertRaises(ValueError):
                    self.validate(folder, case, before, dict(after, **{key: value}), profile, records)
            before_frame, after_frame = self.browse_frames(case)
            with self.assertRaisesRegex(ValueError, "highlight"):
                turns.validate_browsing(case, before, before_frame, bytes(128 * 64))
            wrong_viewport = bytearray(after_frame)
            wrong_viewport[14 * 128 + 20] = 255
            with self.assertRaisesRegex(ValueError, "viewport"):
                turns.validate_browsing(case, before, before_frame, wrong_viewport)
            wrong_endpoint = copy.deepcopy(records)
            wrong_endpoint[-1]["pc"] = 1916
            with self.assertRaisesRegex(ValueError, "modal input loop"):
                self.validate(folder, case, before, after, profile, wrong_endpoint)
            wrong_render = copy.deepcopy(profile)
            wrong_render["pcs"][0]["linkage"] = "_ZN5rogue6renderEv"
            with self.assertRaisesRegex(ValueError, "final render"):
                self.validate(folder, case, before, after, wrong_render, records)

    def test_exact_100ms_cycle_conversion(self):
        with tempfile.TemporaryDirectory() as raw:
            folder = Path(raw)
            sample = self.validate(folder, *self.sample())
            self.assertEqual(sample["cycles"], 1600000)
            self.assertEqual(sample["ms"], 100.0)
            self.assertEqual(sample["ms"], turns.DEFAULT_GOAL_MS)
            self.assertEqual(json.loads((folder / "after.json").read_text())["x"], 33)

    def test_incomplete_or_wrong_endpoint_never_counts_as_fast_turn(self):
        with tempfile.TemporaryDirectory() as raw:
            folder = Path(raw)
            case, before, after, profile, records = self.sample()
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
                ("generation", lambda p, r: p["pcs"][0].update(linkage="_ZN5rogue10make_floorEv")),
                ("compiled setup", lambda p, r: p["pcs"][0].update(file="C:\\project\\bench\\bench.cpp")),
            )
            for name, change in changes:
                with self.subTest(reason=name):
                    p, r = copy.deepcopy(profile), copy.deepcopy(records)
                    change(p, r)
                    with self.assertRaises(ValueError):
                        self.validate(folder, case, before, after, p, r)
            with self.assertRaisesRegex(ValueError, "completed game turn"):
                self.validate(folder, case, before, dict(after, turns=0), profile, records)


if __name__ == "__main__":
    unittest.main()
