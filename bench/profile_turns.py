#!/usr/bin/env python3
"""Profile complete input-to-render turns in an unmodified AVM game ELF.

Uses avm-lldb's command interface, not its optional Python bindings. Every
sample launches a fresh emulator and installs the same saved-layout fixture.
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
import statistics
import subprocess
import sys
import tempfile


CLOCK_HZ = 16_000_000
DEFAULT_GOAL_MS = 150.0
GAME_SIZE = 821
GAME_ADDRESS = 0x01000100
# Saved-layout offsets, checked against the ELF's DWARF before using a result.
OFFSETS = {
    "walls": 0, "explored": 256, "rooms": 512, "doors": 560,
    "monsters": 582, "ground": 678, "inventory": 742,
    "run_seed": 774, "random_state": 776, "magic": 782, "version": 783,
    "valid": 784, "floor": 785, "player": 786, "up": 788, "down": 790,
    "hp": 792, "max_hp": 793, "dexterity": 797, "turns": 801,
    "door_count": 809, "weapon_slot": 810, "armor_slot": 811,
    "amulet_slot": 812, "ring_slots": 813, "identified_items": 815,
}


@dataclass(frozen=True)
class Benchmark:
    name: str
    description: str
    terrain: str = "room"
    setup: str = "play"
    button: str = "RIGHT"
    item: str = ""
    turns: int = 1


BENCHMARKS = (
    Benchmark("move_room", "Move in a lit room"),
    Benchmark("move_corridor", "Move along a branching corridor", "corridor"),
    Benchmark("move_map_edge", "Move with viewport clipping at map corner", "edge"),
    Benchmark("move_dense", "Move on an explored maze with 12 active enemies", "dense"),
    Benchmark("wait", "Wait one turn", setup="wait", button="A"),
    Benchmark("wait_dense", "Wait with 12 enemies, doors and ground items", "dense", "wait", "A"),
    Benchmark("attack_hit", "Bump attack that hits a surviving goblin"),
    Benchmark("attack_miss", "Bump attack that misses a goblin"),
    Benchmark("attack_kill", "Bump attack that kills and awards XP"),
    Benchmark("open_door", "Open a closed door without moving", "corridor"),
    Benchmark("eat_food", "Confirm eating food", setup="use", button="A", item="FOOD"),
    Benchmark("drink_healing", "Confirm drinking a healing potion", setup="use", button="A", item="HEALING"),
    Benchmark("equip_weapon", "Confirm equipping a long sword", setup="use", button="A", item="LONG_SWORD"),
    Benchmark("equip_armor", "Confirm equipping chain mail", setup="use", button="A", item="CHAIN_MAIL"),
    Benchmark("equip_ring", "Confirm equipping a dexterity ring", setup="use", button="A", item="RING_DEXTERITY"),
    Benchmark("equip_cursed_amulet", "Equip an unidentified amulet of speed and discover its curse", setup="use", button="A", item="AMULET_SPEED"),
    Benchmark("scroll_mapping", "Confirm a mapping scroll and reveal the map", setup="use", button="A", item="SCROLL_MAPPING"),
    Benchmark("scroll_teleport", "Confirm a teleport scroll", setup="use", button="A", item="SCROLL_TELEPORT"),
    Benchmark("drop_food", "Confirm dropping food", setup="drop", button="A", item="FOOD"),
    Benchmark("wand_digging", "Submit digging direction; carve blocked terrain", "corridor", "wand", "RIGHT", "WAND_DIGGING"),
    Benchmark("pickup_food", "Confirm pickup after stepping onto food", setup="pickup", button="A"),
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def enum_values(source, name):
    body = re.search(r"enum " + name + r"\s*:\s*uint8_t\s*\{(.*?)\}", source, re.S)
    require(body, f"cannot find {name} in source")
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", body[1], flags=re.S)
    names = [value.strip() for value in text.split(",") if value.strip()]
    require(all(re.fullmatch(r"[A-Z][A-Z_0-9]*", value) for value in names),
            f"{name} changed: update the benchmark enum reader")
    return {value: index for index, value in enumerate(names)}


def source_contract(source_dir):
    model = (source_dir / "model.hpp").read_text(encoding="utf-8")
    version = re.search(r"SAVE_VERSION\s*=\s*(\d+)", model)
    require(version, "cannot find save version")
    items = enum_values(model, "ItemType")
    monsters = enum_values(model, "MonsterType")
    main_lines = (source_dir / "main.cpp").read_text(encoding="utf-8").splitlines()
    ui_lines = (source_dir / "ui.cpp").read_text(encoding="utf-8").splitlines()

    def idle_after(lines, marker):
        start = next(i for i, line in enumerate(lines) if marker in line)
        return next(i + 1 for i in range(start, len(lines)) if "avm_idle();" in lines[i])

    return {
        "version": int(version[1]), "items": items, "monsters": monsters,
        "main_idle": idle_after(main_lines, "Sleep until an interrupt"),
        "item_idle": idle_after(ui_lines, "uint8_t selection = view.first_slot();"),
        "yesno_idle": idle_after(ui_lines, "static bool yesno_modal("),
        "model_sha256": hashlib.sha256((source_dir / "model.hpp").read_bytes()).hexdigest(),
    }


def next_random(state):
    state = state or 0xace1
    state ^= (state << 7) & 0xffff
    state ^= state >> 9
    state ^= (state << 8) & 0xffff
    return state


def amulet_knowledge_bit(item, contract):
    items = contract["items"]
    index = (items["INVISIBILITY"] - items["HEALING"] + 1 +
             items["SCROLL_MASS_POISON"] - items["SCROLL_IDENTIFY"] + 1 +
             items["RING_INVISIBILITY"] - items["RING_SEE_INVISIBLE"] + 1 +
             items[item] - items["AMULET_SPEED"])
    return OFFSETS["identified_items"] + index // 8, 1 << (index & 7)


def make_fixture(case, contract):
    data = bytearray(GAME_SIZE)
    data[:256] = b"\xff" * 256
    data[774:778] = b"\x12\x43\x12\x43"
    data[782:786] = bytes((0xa7, contract["version"], 1, 0))
    data[786:792] = bytes((32, 16, 2, 2, 61, 29))
    data[792:802] = bytes((100, 100, 1, 0, 5, 4, 4, 2, 200, 0))
    data[810:815] = b"\xff" * 5
    # Known appearances avoid mixing identification messages into equip tests.
    data[815:821] = b"\xff" * 6

    def carve(x, y):
        bit = y * 64 + x
        data[bit // 8] &= ~(1 << (bit & 7))

    if case.terrain == "room":
        data[512:516] = bytes((26, 10, 13, 13))
        for y in range(11, 22):
            for x in range(27, 38):
                carve(x, y)
    elif case.terrain == "edge":
        data[786:788] = b"\0\0"
        data[512:516] = bytes((0, 0, 9, 9))
        for y in range(9):
            for x in range(9):
                carve(x, y)
    elif case.terrain == "corridor":
        for x in range(24, 43):
            carve(x, 16)
        for y in range(10, 23):
            carve(32, y)
    elif case.terrain == "dense":
        data[256:512] = b"\xff" * 256
        for y in range(10, 24):
            for x in range(26, 40):
                if x % 4 != 1 or y % 4 == 0:
                    carve(x, y)
    else:
        raise ValueError(f"unknown terrain {case.terrain}")

    def monster(index, x, y, name="GOBLIN", hp=100):
        carve(x, y)
        start = 582 + index * 8
        data[start:start + 8] = bytes((x, y, contract["monsters"][name], hp, 0, 0, 0, 1))

    def ground(index, x, y, name="FOOD"):
        start = 678 + index * 4
        data[start:start + 4] = bytes((x, y, contract["items"][name], 1))

    if case.terrain == "dense":
        positions = ((30, 16), (34, 16), (32, 14), (32, 18), (28, 12), (36, 12),
                     (28, 20), (36, 20), (30, 10), (34, 10), (30, 22), (34, 22))
        for index, (x, y) in enumerate(positions):
            monster(index, x, y, ("GOBLIN", "TROLL", "RATTLESNAKE")[index % 3])
        data[560:564] = bytes((31, 16, 35, 16))
        data[809] = 2
        for index, (x, y) in enumerate(((28, 16), (32, 12), (36, 16), (32, 20))):
            ground(index, x, y)

    if case.name.startswith("attack_"):
        data[797] = 0 if case.name == "attack_miss" else 84
        hp = 1 if case.name == "attack_kill" else 100
        monster(0, 33, 16, hp=hp)
        # Goblin DEX is 4; choose a seed guaranteeing the requested hit/miss.
        limit = data[797] * 2 + 4 + 1
        seed = next(seed for seed in range(1, 65536)
                    if (next_random(seed) % limit >= 4) == (case.name != "attack_miss"))
        data[776:778] = seed.to_bytes(2, "little")
    if case.name == "open_door":
        data[560:562] = bytes((33, 16))
        data[809] = 1
    if case.item:
        info = 0x85 if case.item in ("LONG_SWORD", "CHAIN_MAIL") else 0x81
        if case.item.startswith("WAND_"):
            info = 0x83  # Identified, normal modifier, three charges.
        data[742:744] = bytes((contract["items"][case.item], info))
    if case.name == "equip_cursed_amulet":
        data[743] = 0x41  # Cursed +1 magnitude; instance/curse not yet identified.
        offset, mask = amulet_knowledge_bit(case.item, contract)
        data[offset] &= ~mask  # Its type must also be discovered by equipping it.
    if case.name == "drink_healing":
        data[792] = 50
    if case.name == "wand_digging":
        data[(16 * 64 + 35) // 8] |= 1 << (35 & 7)
    if case.setup == "pickup":
        ground(0, 33, 16)
    return bytes(data)


def quote(path):
    # LLDB command parsing, never a shell command.
    return '"' + str(path).replace("\\", "/").replace('"', '\\"') + '"'


def dump_game(path):
    return (f"memory read --binary --outfile {quote(path)} --size 1 "
            f"--count {GAME_SIZE} 0x{GAME_ADDRESS:x}")


def layout_checks():
    expressions = [("sizeof(rogue::game)", GAME_SIZE), ("&rogue::game", 0x100)]
    expressions += [(f"(char*)&rogue::game.{field} - (char*)&rogue::game", offset)
                    for field, offset in OFFSETS.items()]
    return expressions


def make_commands(case, contract, folder, native=False, deadline_ms=10000):
    run = f"avm run-for {deadline_ms}ms"
    def press(button):
        return ["avm button set " + button, run, "avm button set", run]

    commands = [
        f"breakpoint set --file main.cpp --line {contract['main_idle']}", "run",
        *[f"expr -- (unsigned int)({expr})" for expr, _ in layout_checks()],
        *press("A"),  # Start a real game; initialize Ui/Session via production code.
        f"memory write --infile {quote(folder / 'fixture.bin')} 0x{GAME_ADDRESS:x}",
        *press("B"), *press("B"),  # Menu/cancel renders the fixture without taking a turn.
        "avm time",  # Known main-loop boundary, before modal preparation.
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
            "breakpoint disable 1", f"breakpoint set --file ui.cpp --line {line}",
            "avm button set " + ("RIGHT" if case.setup == "pickup" else "A"), run,
            "avm button set", run,  # Poll release, ready for the final A edge.
            "breakpoint disable 2", "breakpoint enable 1",
        ]
        if case.setup == "wand":
            commands += press("A")  # Confirm slot, stop after direction prompt is ready.
    commands += [
        dump_game(folder / "before.bin"), "avm time",
        "avm profile start" + (" --native" if native else ""),
        "avm button set " + case.button, run, "avm time", "avm profile stop",
        f"avm profile save {quote(folder / 'turn.avmp')}", "avm profile report --top 12",
        dump_game(folder / "after.bin"),
        "thread backtrace",
        f"avm display save {quote(folder / 'frame.pgm')} --mode controller",
    ]
    return "\n".join(commands) + "\n"


def records_from(output):
    return [json.loads(line) for line in output.splitlines()
            if line.startswith("{") and line.endswith("}")]


def validate_outcome(case, before, after, contract):
    require(len(before) == len(after) == GAME_SIZE, "incomplete state capture")
    require(after[801] == (before[801] + case.turns) % 256,
            f"{case.name}: expected {case.turns} completed game turn(s)")
    require(after[784] and after[792], "turn unexpectedly ended the run")
    if case.name.startswith("move_"):
        require(after[786:788] == bytes((before[786] + 1, before[787])), "player did not move right")
    if case.name == "attack_hit":
        require(0 < after[585] < before[585], "attack did not hit a surviving enemy")
    if case.name == "attack_miss":
        require(after[585] == before[585], "attack was not a miss")
    if case.name == "attack_kill":
        require(after[584] == 0 and after[778:780] != before[778:780], "enemy was not killed/scored")
    if case.name == "open_door":
        require(after[561] & 0x80 and after[786:788] == before[786:788], "door was not opened in place")
    if case.name in ("eat_food", "drink_healing", "scroll_mapping", "scroll_teleport", "drop_food"):
        require(after[742] == 0, "consumable/drop was not used")
    if case.name == "drink_healing":
        require(after[792] > before[792], "potion did not heal")
    if case.name == "equip_weapon":
        require(after[810] == 0, "weapon was not equipped")
    if case.name == "equip_armor":
        require(after[811] == 0, "armor was not equipped")
    if case.name == "equip_ring":
        require(0 in after[813:815], "ring was not equipped")
    if case.name == "equip_cursed_amulet":
        offset, mask = amulet_knowledge_bit(case.item, contract)
        require(before[812] == 0xff and before[742] == contract["items"][case.item] and
                before[743] == 0x41 and not before[offset] & mask,
                "cursed amulet must start unequipped with its type and curse unknown")
        require(after[812] == 0 and after[742] == before[742] and
                after[743] == 0xc1 and after[offset] & mask,
                "amulet was not equipped with its type and curse discovered")
    if case.name == "scroll_mapping":
        require(after[256:512] == b"\xff" * 256, "mapping did not reveal the map")
    if case.name == "scroll_teleport":
        require(after[786:788] != before[786:788], "teleport did not move the player")
    if case.name == "drop_food":
        require(after[680] == contract["items"]["FOOD"], "dropped food is missing")
    if case.name.startswith("wand_"):
        require(after[743] & 0x0f == 2, "wand charge was not spent")
    if case.name == "wand_digging":
        require(not after[(16 * 64 + 35) // 8] & (1 << (35 & 7)), "digging did not carve the wall")
    if case.name == "pickup_food":
        require(after[680] == 0 and after[742] == contract["items"]["FOOD"], "food was not picked up")


def validate_sample(case, contract, folder, output):
    values = [int(value) for value in re.findall(r"\(unsigned int\) \$\d+ = (\d+)", output)]
    expected_layout = [expected for _, expected in layout_checks()]
    require(values == expected_layout, "ELF saved layout does not match")
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
                    row["linkage"] == "_ZN5rogue10make_floorEv" for row in profile["pcs"]),
            "generation or animation occurred inside the measured turn")
    buttons = [record for record in records if "pressed_mask" in record and
               start["cycles"] <= record.get("cycle", -1) <= end["cycles"]]
    require(len(buttons) == 1 and buttons[0]["cycle"] == start["cycles"] and
            buttons[0]["pressed_mask"] != 0,
            "a measured turn must contain exactly one submitted input")
    validate_outcome(case, (folder / "before.bin").read_bytes(), (folder / "after.bin").read_bytes(), contract)
    return {"cycles": cycles, "ms": cycles * 1000 / CLOCK_HZ,
            "start_cycle": start["cycles"], "end_cycle": end["cycles"],
            "start_pc": start["pc"], "end_pc": end["pc"],
            "profile": str(folder / "turn.avmp"), "identity": profile["identity"],
            "fixture_sha256": hashlib.sha256((folder / "fixture.bin").read_bytes()).hexdigest(),
            "start_game_sha256": hashlib.sha256((folder / "before.bin").read_bytes()).hexdigest(),
            "display_hash": profile["end_display_hash"]}


def write_summary(folder, cases, goal_ms, elf, contract):
    summary = {"schema": 1, "metric": "input-to-ready elapsed emulated cycles",
               "clock_hz": CLOCK_HZ, "goal_ms": goal_ms, "elf": str(elf),
               "source_model_sha256": contract["model_sha256"], "benchmarks": cases}
    (folder / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    with (folder / "summary.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(("benchmark", "samples", "median_ms", "worst_ms", "goal_ms", "meets_goal"))
        for case in cases:
            writer.writerow((case["name"], len(case["samples"]), case["median_ms"], case["worst_ms"], goal_ms, case["meets_goal"]))
    lines = [f"# Turn benchmarks: {goal_ms:g} ms initial goal", "",
             "Elapsed emulated time at 16 MHz, from submitted button to the main input wait after final render/display.", "",
             "| Benchmark | Median ms | Worst ms | Goal |", "| --- | ---: | ---: | --- |"]
    for case in cases:
        lines.append(f"| {case['name']} | {case['median_ms']:.3f} | {case['worst_ms']:.3f} | {'PASS' if case['meets_goal'] else 'OVER'} |")
    (folder / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, help="current -g -O2 -flto game ELF")
    parser.add_argument("--lldb", type=Path, help="installed avm-lldb executable")
    parser.add_argument("--sdk-root", type=Path, help="SDK containing bin/avm-lldb")
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1] / "src")
    parser.add_argument("--output", type=Path, default=Path("turn-benchmarks"), help="parent of a new, unique run directory")
    parser.add_argument("--benchmark", action="append", choices=[case.name for case in BENCHMARKS], help="repeat to select cases")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--goal-ms", type=float, default=DEFAULT_GOAL_MS)
    parser.add_argument("--deadline-ms", type=int, default=10000, help="emulated safety deadline, not the performance goal")
    parser.add_argument("--timeout", type=float, default=120, help="host seconds allowed for each debugger session")
    parser.add_argument("--native", action="store_true", help="also collect AVR interpreter hotspots")
    parser.add_argument("--html", action="store_true", help="render each profile with the SDK's avm-prof")
    parser.add_argument("--emit-only", action="store_true", help="write fixtures/LLDB scripts without running")
    parser.add_argument("--check", action="store_true", help="exit 2 if any completed sample exceeds the goal")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    if args.list:
        for case in BENCHMARKS:
            print(f"{case.name:22} {case.description}")
        return 0
    require(args.elf and args.elf.is_file(), "--elf must name an existing game ELF")
    require(args.repeat > 0 and args.deadline_ms > 0 and args.timeout > 0 and
            math.isfinite(args.goal_ms) and args.goal_ms > 0, "budgets and repeat count must be positive")
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
        samples = []
        for index in range(1, args.repeat + 1):
            sample_dir = folder / case.name / str(index)
            sample_dir.mkdir(parents=True)
            (sample_dir / "fixture.bin").write_bytes(make_fixture(case, contract))
            commands = sample_dir / "turn.lldb"
            commands.write_text(make_commands(case, contract, sample_dir, args.native, args.deadline_ms), encoding="utf-8")
            if args.emit_only:
                continue
            try:
                run = subprocess.run([str(Path(lldb).resolve()), "--batch", "--source", str(commands), str(args.elf.resolve())],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=args.timeout)
                (sample_dir / "lldb.log").write_text(run.stdout, encoding="utf-8")
                require(run.returncode == 0, f"avm-lldb exited {run.returncode}; see {sample_dir / 'lldb.log'}")
                sample = validate_sample(case, contract, sample_dir, run.stdout)
                samples.append(sample)
                print(f"{case.name:22} [{index}/{args.repeat}] {sample['ms']:9.3f} ms  {'PASS' if sample['ms'] <= args.goal_ms else 'OVER'}", flush=True)
                if args.html:
                    profiler = Path(lldb).resolve().with_name("avm-prof" + suffix)
                    subprocess.run([str(profiler), "report", str(sample_dir / "turn.avmp"), "--html", str(sample_dir / "profile.html")],
                                   check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=args.timeout)
            except (ValueError, subprocess.SubprocessError) as error:
                (sample_dir / "error.txt").write_text(str(error) + "\n", encoding="utf-8")
                raise ValueError(f"{case.name} sample {index}: {error}; artifacts in {sample_dir}") from error
        if samples:
            worst = max(sample["ms"] for sample in samples)
            results.append({"name": case.name, "description": case.description, "samples": samples,
                            "median_ms": statistics.median(sample["ms"] for sample in samples),
                            "worst_ms": worst, "meets_goal": worst <= args.goal_ms})
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
