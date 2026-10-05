#!/usr/bin/env python3
"""Profile input-to-render turns prepared by the compiled bench.cpp scenarios.

Each case runs once. Uses avm-lldb's command interface, not its optional Python bindings. No save
files, serialization offsets, or save-version parsing are used.
"""

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

CLOCK_HZ = 16_000_000
DEFAULT_GOAL_MS = 150.0


@dataclass(frozen=True)
class Benchmark:
    name: str
    description: str
    setup: str
    button: str
    item: str
    index: int


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_benchmarks(path):
    source = path.read_text(encoding="utf-8")
    entries = re.findall(r'^\s*X\((\w+),\s*"([^"\n]+)",\s*(\w+),\s*(\w+),\s*(\w+),\s*(\w+)\)', source, re.M)
    require(entries and len(entries) == len(re.findall(r'^\s*X\(', source, re.M)) and
            len({entry[0] for entry in entries}) == len(entries), "invalid compiled benchmark manifest")
    cases = tuple(Benchmark(name, description, setup, button, item, index)
                  for index, (name, description, setup, button, terrain, item) in enumerate(entries))
    require(all(case.setup in ("play", "wait", "use", "drop", "wand", "pickup") and
                case.button in ("RIGHT", "A") for case in cases), "unsupported benchmark input preparation")
    return cases


BENCHMARKS = read_benchmarks(Path(__file__).with_name("bench.cpp"))


def source_contract(source_dir):
    main_lines = (source_dir / "main.cpp").read_text(encoding="utf-8").splitlines()
    ui_lines = (source_dir / "ui.cpp").read_text(encoding="utf-8").splitlines()

    def idle_after(lines, marker):
        start = next(i for i, line in enumerate(lines) if marker in line)
        return next(i + 1 for i in range(start, len(lines)) if "avm_idle();" in lines[i])

    return {
        "main_idle": idle_after(main_lines, "Sleep until an interrupt"),
        "item_idle": idle_after(ui_lines, "uint8_t selection = view.first_slot();"),
        "yesno_idle": idle_after(ui_lines, "static bool yesno_modal("),
        "bench_sha256": hashlib.sha256((source_dir.parent / "bench/bench.cpp").read_bytes()).hexdigest(),
    }


def state_fields(case):
    # All reads use DWARF names; there is no dependency on Game byte layout.
    fields = {
        "valid": "game.valid", "hp": "game.hp", "turns": "game.turns",
        "x": "game.player.x", "y": "game.player.y", "score": "game.score",
        "monster_type": "game.monsters[0].type", "monster_hp": "game.monsters[0].hp",
        "door_open": "(game.doors[0].pos.y & 0x80) != 0",
        "item_type": "game.inventory[0].type", "item_info": "game.inventory[0].info",
        "weapon": "game.weapon_slot", "armor": "game.armor_slot",
        "amulet": "game.amulet_slot", "ring0": "game.ring_slots[0]", "ring1": "game.ring_slots[1]",
        "ground_type": "game.ground[0].item.type",
        "digging_wall": "(game.walls[(16 * MAP_W + 35) >> 3] & (1u << (35 & 7))) != 0",
    }
    fields = {key: re.sub(r'\b(game|MAP_W)\b', r'rogue::\1', expr) for key, expr in fields.items()}
    if case.item != "NO_ITEM":
        fields["expected_item"] = "rogue::game.inventory[0].type == rogue::ItemType::" + case.item
    if case.name == "pickup_food":
        fields["food"] = "rogue::game.inventory[0].type == rogue::ItemType::FOOD"
    if case.name == "equip_cursed_amulet":
        index = "(INVISIBILITY - HEALING + 1 + SCROLL_MASS_POISON - SCROLL_IDENTIFY + 1 + RING_INVISIBILITY - RING_SEE_INVISIBLE + 1)"
        index = re.sub(r'\b[A-Z][A-Z_0-9]+\b', lambda m: "rogue::ItemType::" + m[0], index)
        fields["amulet_known"] = f"(rogue::game.identified_items[{index} >> 3] & (1u << ({index} & 7))) != 0"
        fields["cursed"] = "(rogue::game.inventory[0].info & rogue::ITEM_CURSED) != 0"
        fields["identified"] = "(rogue::game.inventory[0].info & rogue::ITEM_IDENTIFIED) != 0"
        fields["magnitude"] = "rogue::game.inventory[0].info & rogue::ITEM_VALUE_MASK"
        fields["amulet_empty"] = "rogue::game.amulet_slot == rogue::NONE"
    if case.name == "wand_digging":
        fields["charges"] = "rogue::game.inventory[0].info & rogue::WAND_CHARGE_MASK"
    return fields


def quote(path):
    # LLDB command parsing, never a shell command.
    return '"' + str(path).replace("\\", "/").replace('"', '\\"') + '"'


def make_commands(case, contract, folder, native=False, deadline_ms=10000):
    run = f"avm run-for {deadline_ms}ms"

    def press(button):
        return ["avm button set " + button, run, "avm button set", run]

    def snapshot():
        return [f"expr -- (unsigned int)({expr})" for expr in state_fields(case).values()]

    commands = [
        "breakpoint set --name bench_select", "run",
        f"expr -- (unsigned int)(bench_case = {case.index})", "breakpoint disable 1",
        f"breakpoint set --name bench_{case.name}", run,
        "expr -- (unsigned int)(bench_case)", "breakpoint disable 2",
        f"breakpoint set --file main.cpp --line {contract['main_idle']}", run,
        "avm time",  # Compiled setup and initial render have finished.
    ]
    if case.setup == "wait":
        commands += press("B")
    if case.setup in ("use", "drop", "wand"):
        commands += press("B")
        selection = 2 if case.setup == "drop" else 1
        for _ in range(selection):
            commands += press("DOWN")
    if case.setup in ("use", "drop", "pickup", "wand"):
        line = contract["yesno_idle"] if case.setup == "pickup" else contract["item_idle"]
        commands += [
            "breakpoint disable 3", f"breakpoint set --file ui.cpp --line {line}",
            "avm button set " + ("RIGHT" if case.setup == "pickup" else "A"), run,
            "avm button set", run,  # Poll release, ready for the final A edge.
            "breakpoint disable 4", "breakpoint enable 3",
        ]
        if case.setup == "wand":
            commands += press("A")  # Confirm slot; stop after direction prompt is ready.
    commands += [
        *snapshot(), "avm time",
        "avm profile start" + (" --native" if native else ""),
        "avm button set " + case.button, run, "avm time", "avm profile stop",
        f"avm profile save {quote(folder / 'turn.avmp')}", "avm profile report --top 12",
        *snapshot(),
        # Read this named array after profiling to check mapping, regardless of its size.
        f"memory read --binary --outfile {quote(folder / 'explored.bin')} --size 1 "
        "--count `sizeof(rogue::game.explored)` `(unsigned int)&rogue::game.explored + 0x01000000`",
        "thread backtrace",
        f"avm display save {quote(folder / 'frame.pgm')} --mode controller",
    ]
    return "\n".join(commands) + "\n"


def records_from(output):
    return [json.loads(line) for line in output.splitlines()
            if line.startswith("{") and line.endswith("}")]


def validate_outcome(case, before, after, explored):
    require(after["turns"] == (before["turns"] + 1) % 256, "expected one completed game turn")
    require(after["valid"] and after["hp"], "turn unexpectedly ended the run")
    position_before = (before["x"], before["y"])
    position_after = (after["x"], after["y"])
    if case.item != "NO_ITEM":
        require(before["expected_item"], "compiled scenario prepared the wrong item")
    if case.name.startswith("move_"):
        require(position_after == (before["x"] + 1, before["y"]), "player did not move right")
    if case.name == "attack_hit":
        require(0 < after["monster_hp"] < before["monster_hp"], "attack did not hit a surviving enemy")
    if case.name == "attack_miss":
        require(after["monster_hp"] == before["monster_hp"], "attack was not a miss")
    if case.name == "attack_kill":
        require(after["monster_type"] == 0 and after["score"] != before["score"], "enemy was not killed/scored")
    if case.name == "open_door":
        require(after["door_open"] and position_after == position_before, "door was not opened in place")
    if case.name in ("eat_food", "drink_healing", "scroll_mapping", "scroll_teleport", "drop_food"):
        require(after["item_type"] == 0, "consumable/drop was not used")
    if case.name == "drink_healing":
        require(after["hp"] > before["hp"], "potion did not heal")
    for name, slot in (("equip_weapon", "weapon"), ("equip_armor", "armor")):
        if case.name == name:
            require(after[slot] == 0 and after["expected_item"], "item was not equipped")
    if case.name == "equip_ring":
        require(0 in (after["ring0"], after["ring1"]) and after["expected_item"], "ring was not equipped")
    if case.name == "equip_cursed_amulet":
        require(before["amulet_empty"] and before["cursed"] and not before["identified"] and
                before["magnitude"] == 1 and not before["amulet_known"],
                "cursed amulet must start unequipped with its type and curse unknown")
        require(after["amulet"] == 0 and after["expected_item"] and
                after["cursed"] and after["identified"] and after["magnitude"] == 1 and after["amulet_known"],
                "amulet was not equipped with its type and curse discovered")
    if case.name == "scroll_mapping":
        require(explored and all(byte == 0xff for byte in explored), "mapping did not reveal the map")
    if case.name == "scroll_teleport":
        require(position_after != position_before, "teleport did not move the player")
    if case.name == "drop_food":
        require(after["ground_type"] == before["item_type"], "dropped food is missing")
    if case.name == "wand_digging":
        require(after["charges"] == before["charges"] - 1 and before["digging_wall"] and not after["digging_wall"],
                "digging did not spend a charge and carve the wall")
    if case.name == "pickup_food":
        require(after["ground_type"] == 0 and after["food"], "food was not picked up")


def validate_sample(case, contract, folder, output):
    values = [int(value) for value in re.findall(r"\(unsigned int\) \$\d+ = (\d+)", output)]
    fields = list(state_fields(case))
    require(len(values) == 2 + 2 * len(fields) and values[:2] == [case.index, case.index],
            "missing compiled case selection or typed state capture")
    before = dict(zip(fields, values[2:2 + len(fields)]))
    after = dict(zip(fields, values[2 + len(fields):]))
    records = records_from(output)
    require(not any(record.get("ok") is False for record in records), "debugger command failed")
    stops = [record for record in records if "requested_cycles" in record]
    require(stops and all(stop["reason"] == "breakpoint" for stop in stops),
            "setup or turn did not reach its expected input boundary before the deadline")
    times = [record for record in records if "seconds_since_reset" in record]
    require(len(times) == 3, "missing ready/start/end clock records")
    ready, start, end = times
    require(end["pc"] == ready["pc"], "stopped before final render returned to main input loop")
    profile = json.loads((folder / "turn.avmp").read_text(encoding="utf-8"))
    window = profile["window"]
    cycles = int(window["end_cycle"]) - int(window["start_cycle"])
    require(window["complete"] and window["stop_reason"] == "breakpoint" and
            int(window["partial_cycles"]) == 0 and int(window["discontinuities"]) == 0,
            "profile window is incomplete or discontinuous")
    require(int(window["start_cycle"]) == start["cycles"] and
            int(window["end_cycle"]) == end["cycles"] and cycles > 0 and
            sum(int(row["cycles"]) for row in profile["pcs"]) == cycles,
            "profile and input-to-ready clock interval do not reconcile")
    require(any(row["linkage"] == "_ZN5rogue6renderEv" for row in profile["pcs"]),
            "the measured turn did not execute the final render")
    require(not any("rogue::animate_" in row["function"] or
                    row["linkage"] == "_ZN5rogue10make_floorEv" or
                    row["function"].lstrip(":").startswith("bench_") or
                    Path(row.get("file", "").replace("\\", "/")).name == "bench.cpp"
                    for row in profile["pcs"]),
            "setup, generation or animation occurred inside the measured turn")
    buttons = [record for record in records if "pressed_mask" in record and
               start["cycles"] <= record.get("cycle", -1) <= end["cycles"]]
    require(len(buttons) == 1 and buttons[0]["cycle"] == start["cycles"] and buttons[0]["pressed_mask"] != 0,
            "a measured turn must contain exactly one submitted input")
    validate_outcome(case, before, after, (folder / "explored.bin").read_bytes())
    for name, state in (("before", before), ("after", after)):
        (folder / (name + ".json")).write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    return {"cycles": cycles, "ms": cycles * 1000 / CLOCK_HZ,
            "start_cycle": start["cycles"], "end_cycle": end["cycles"],
            "start_pc": start["pc"], "end_pc": end["pc"],
            "profile": str(folder / "turn.avmp"), "identity": profile["identity"],
            "start_state_sha256": hashlib.sha256(json.dumps(before, sort_keys=True).encode()).hexdigest(),
            "display_hash": profile["end_display_hash"]}


def write_summary(folder, cases, goal_ms, elf, contract):
    summary = {"schema": 2, "metric": "input-to-ready elapsed emulated cycles",
               "clock_hz": CLOCK_HZ, "goal_ms": goal_ms, "elf": str(elf),
               "source_bench_sha256": contract["bench_sha256"], "benchmarks": cases}
    (folder / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    with (folder / "summary.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(("benchmark", "cycles", "ms", "goal_ms", "meets_goal"))
        for case in cases:
            writer.writerow((case["name"], case["cycles"], case["ms"], goal_ms, case["meets_goal"]))
    lines = [f"# Turn benchmarks: {goal_ms:g} ms initial goal", "",
             "Elapsed emulated time at 16 MHz, from submitted button to the main input wait after final render/display.", "",
             "| Benchmark | Cycles | ms | Goal |", "| --- | ---: | ---: | --- |"]
    for case in cases:
        lines.append(f"| {case['name']} | {case['cycles']} | {case['ms']:.3f} | {'PASS' if case['meets_goal'] else 'OVER'} |")
    (folder / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, help="compiled ardurogue2-bench.elf")
    parser.add_argument("--lldb", type=Path, help="installed avm-lldb executable")
    parser.add_argument("--sdk-root", type=Path, help="SDK containing bin/avm-lldb")
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1] / "src")
    parser.add_argument("--output", type=Path, default=Path("build/turn-benchmarks"), help="parent of a new, unique run directory")
    parser.add_argument("--benchmark", action="append", choices=[case.name for case in BENCHMARKS], help="select cases; may be specified multiple times")
    parser.add_argument("--goal-ms", type=float, default=DEFAULT_GOAL_MS)
    parser.add_argument("--deadline-ms", type=int, default=10000, help="emulated safety deadline, not the performance goal")
    parser.add_argument("--timeout", type=float, default=120, help="host seconds allowed for each debugger session")
    parser.add_argument("--native", action="store_true", help="also collect AVR interpreter hotspots")
    parser.add_argument("--html", action="store_true", help="render each profile with the SDK's avm-prof")
    parser.add_argument("--emit-only", action="store_true", help="write LLDB scripts without running")
    parser.add_argument("--check", action="store_true", help="exit 2 if any benchmark exceeds the goal")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    if args.list:
        for case in BENCHMARKS:
            print(f"{case.index:2} {case.name:22} {case.description}")
        return 0
    require(args.elf and args.elf.is_file(), "--elf must name an existing benchmark ELF")
    require(args.deadline_ms > 0 and args.timeout > 0 and
            math.isfinite(args.goal_ms) and args.goal_ms > 0, "budgets must be positive")
    suffix = ".exe" if sys.platform == "win32" else ""
    lldb = args.lldb or (args.sdk_root / "bin" / ("avm-lldb" + suffix) if args.sdk_root else shutil.which("avm-lldb"))
    require(args.emit_only or (lldb and Path(lldb).is_file()), "provide --lldb or --sdk-root, or put avm-lldb on PATH")
    contract = source_contract(args.source_dir.resolve())
    cases = [case for case in BENCHMARKS if not args.benchmark or case.name in args.benchmark]
    args.output.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
    folder = Path(tempfile.mkdtemp(prefix=stamp, dir=args.output.resolve()))
    print(f"Artifacts: {folder}", flush=True)
    results = []
    for case in cases:
        case_dir = folder / case.name
        case_dir.mkdir()
        commands = case_dir / "turn.lldb"
        commands.write_text(make_commands(case, contract, case_dir, args.native, args.deadline_ms), encoding="utf-8")
        if args.emit_only:
            continue
        try:
            run = subprocess.run([str(Path(lldb).resolve()), "--batch", "--source", str(commands), str(args.elf.resolve())],
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=args.timeout)
            (case_dir / "lldb.log").write_text(run.stdout, encoding="utf-8")
            require(run.returncode == 0, f"avm-lldb exited {run.returncode}; see {case_dir / 'lldb.log'}")
            measurement = validate_sample(case, contract, case_dir, run.stdout)
            meets_goal = measurement["ms"] <= args.goal_ms
            print(f"{case.name:22} {measurement['ms']:9.3f} ms  {'PASS' if meets_goal else 'OVER'}", flush=True)
            if args.html:
                profiler = Path(lldb).resolve().with_name("avm-prof" + suffix)
                subprocess.run([str(profiler), "report", str(case_dir / "turn.avmp"), "--html", str(case_dir / "profile.html")],
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=args.timeout)
        except (ValueError, subprocess.SubprocessError) as error:
            (case_dir / "error.txt").write_text(str(error) + "\n", encoding="utf-8")
            raise ValueError(f"{case.name}: {error}; artifacts in {case_dir}") from error
        results.append({"name": case.name, "description": case.description,
                        **measurement, "meets_goal": meets_goal})
        write_summary(folder, results, args.goal_ms, args.elf.resolve(), contract)
    if args.emit_only:
        print("Run any generated turn.lldb with avm-lldb --batch --source <script> <elf>.")
        return 0
    failed = sum(not case["meets_goal"] for case in results)
    print(f"{len(results) - failed}/{len(results)} benchmarks meet {args.goal_ms:g} ms; {failed} over. Summary: {folder / 'summary.md'}")
    return 2 if args.check and failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, StopIteration) as error:
        print(f"turn benchmark error: {error}", file=sys.stderr)
        sys.exit(1)
