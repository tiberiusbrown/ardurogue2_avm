#!/usr/bin/env python3
"""Frozen-policy, same-executable gameplay screening and progression reports.

Raw data and specifications are retained in a fresh ignored build directory.
No agent parameters are tuned here. All variants use equipment-dependent light.
"""
import argparse
import csv
import hashlib
import json
import statistics
import subprocess
import time
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

BASELINE = {
    "ground": 16, "inventory": 16, "monsters": 12, "phantom-count": 12,
    "phantom-strength": 6, "phantom-dexterity": 4, "phantom-speed": 4,
    "phantom-armor": 1, "phantom-health": 12, "sight-duration": 40,
    "sight-percent": 0, "sight-early-percent": 0, "natural-regen-interval": 0,
    "healing-min-quarters": 1, "healing-random-quarters": 2,
    "early-equipment-percent": 0, "late-strength-percent": 100,
    "ascent-strength-percent": 100, "late-dexterity-percent": 100, "late-health-percent": 100,
}


def screening():
    variants = {"baseline": {}}
    for strength in (5, 4, 3, 2):
        variants[f"phantom-strength-{strength}"] = {"phantom-strength": strength}
    for key, values in (("phantom-dexterity", (2, 3)), ("phantom-health", (8, 10)),
                        ("phantom-armor", (0,)), ("phantom-speed", (3,)),
                        ("phantom-count", (8, 10)), ("monsters", (8, 10))):
        for value in values:
            variants[f"{key}-{value}"] = {key: value}
    for duration in (20, 40, 80):
        variants[f"sight-duration-{duration}"] = {
            "sight-duration": duration, "sight-percent": 12, "sight-early-percent": 12}
    for percent in (6, 20, 30):
        variants[f"sight-percent-{percent}"] = {
            "sight-percent": percent, "sight-early-percent": percent}
    variants["sight-early-24"] = {"sight-percent": 6, "sight-early-percent": 24}
    for ground, inventory in ((20, 16), (16, 20), (20, 20), (24, 20)):
        variants[f"capacity-{ground}-{inventory}"] = {"ground": ground, "inventory": inventory}
    for value in (64, 128):
        variants[f"natural-regeneration-{value}"] = {"natural-regen-interval": value}
    for minimum, spread in ((2, 2), (3, 1), (4, 0)):
        variants[f"healing-potion-{minimum}-{spread}"] = {
            "healing-min-quarters": minimum, "healing-random-quarters": spread}
    variants["early-equipment-50"] = {"early-equipment-percent": 50}
    for value in (75, 85):
        variants[f"late-strength-{value}"] = {"late-strength-percent": value}
    variants["ascent-strength-75"] = {"ascent-strength-percent": 75}
    return {name: BASELINE | values for name, values in variants.items()}


def read_rows(path):
    with path.open(newline="", encoding="utf-8") as f:
        yield from csv.DictReader(f)


def seal(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def summarize(directory):
    directory = Path(directory)
    runs = list(read_rows(directory / "runs.csv"))
    if not runs or len({r["effective_seed"] for r in runs}) != len(runs):
        raise ValueError(f"missing/duplicate seeds: {directory}")
    if any(r["result"] not in ("escaped", "death") or r["stuck"] != "0" for r in runs):
        raise ValueError(f"invalid simulator outcomes: {directory}")
    n = len(runs)
    depths = [int(r["deepest_floor"]) + 1 for r in runs]
    result = {"runs": n, "escaped": sum(r["result"] == "escaped" for r in runs),
              "mean_depth": statistics.mean(depths), "median_depth": statistics.median(depths),
              "lord": sum(int(r["killed"]) for r in read_rows(directory / "monsters.csv") if r["monster"] == "LORD"),
              "yendor": sum(int(r["has_yendor"]) for r in runs),
              "starvation": sum(r["death_cause"] == "starvation" for r in runs),
              "death_causes": dict(Counter(r["death_cause"] for r in runs if r["result"] == "death"))}
    result["escape_percent"] = 100 * result["escaped"] / n
    result["ascent_survival_percent"] = 100 * result["escaped"] / result["yendor"] if result["yendor"] else None
    floors = defaultdict(lambda: Counter())
    for f in read_rows(directory / "floors.csv"):
        key = (int(f["floor"]), f["direction"])
        a = floors[key]
        a["entered"] += 1
        a["exited"] += int(f["exited"])
        for name in ("actions", "turns", "damage_taken", "consumables", "pickups"):
            a[name] += int(f[{"consumables":"consumables_used", "pickups":"items_picked_up"}.get(name,name)])
    result["floors"] = [{"floor": k[0], "direction": k[1], **v,
                         "survival_percent": 100 * v["exited"] / v["entered"]}
                        for k, v in sorted(floors.items())]
    items = defaultdict(Counter)
    for row in read_rows(directory / "items.csv"):
        for field in ("generated", "picked_up", "used", "potions_drunk", "identified", "expired", "active_turns", "pack_full", "discarded"):
            items[row["item"]][field] += int(row.get(field, 0))
    result["items"] = dict(items)
    first, encounters = Counter(), Counter()
    lengths = []
    for e in read_rows(directory / "encounters.csv"):
        if e["phantom"] != "1":
            continue
        encounters["encounters"] += 1
        encounters["survived"] += int(e["survived"])
        encounters["visible_first"] += int(e["phantom_visible_first"])
        encounters["potion_first"] += int(e.get("potion_sight_first", 0))
        encounters["potion_any"] += int(int(e.get("phantom_sight_attack_events", 0)) > 0)
        for field in ("phantom_kills", "phantom_damage", "phantom_healing_consumed", "phantom_escape_consumed", "phantom_attack_events", "phantom_sight_attack_events"):
            encounters[field] += int(e.get(field, 0))
        lengths.append(int(e["end_turn"]) - int(e["phantom_contact_turn"]))
        if e["first_phantom"] == "1":
            first["contacts"] += 1
            first["survived"] += int(e["survived"])
            first["survived_100"] += int(e["survived_100"])
    result["phantom"] = dict(encounters)
    result["first_phantom"] = dict(first)
    result["phantom_encounter_turns_mean"] = statistics.mean(lengths) if lengths else None
    result["phantom_encounter_turns_median"] = statistics.median(lengths) if lengths else None
    phantom_floor = [r for r in read_rows(directory / "floors.csv")
                     if r["floor"] == "4" and r["direction"] == "descent"]
    result["phantom_floor"] = {"visits": len(phantom_floor), **{
        f"mean_{field}": statistics.mean(int(r[field]) for r in phantom_floor) if phantom_floor else None
        for field in ("turns", "entry_hp", "entry_max_hp", "entry_level", "entry_armor_rating",
                      "entry_healing_units", "entry_food_units", "damage_taken", "consumables_used")}}
    manifest = json.loads((directory / "manifest.json").read_text())
    result["policy_hash"] = manifest["agent_policy_hash"]
    result["seals"] = {p.name: seal(p)
                       for p in directory.glob("*.csv")}
    (directory / "game_balance_summary.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def run_variant(executable, root, name, parameters, seeds, jobs, agent, parameter_file=None, max_actions=100000, interventions=()):
    directory = root / name
    args = [str(executable), "--agent", agent, "--seeds", seeds, "--jobs", str(jobs),
            "--entry-state", "--max-actions", str(max_actions), "--experiment", "game-balance", "--variant", name,
            "--output", str(directory)]
    if parameter_file:
        args += ["--agent-params", str(parameter_file)]
    for key, value in parameters.items():
        args += ["--intervention", f"balance:{key}={value}"]
    for intervention in interventions:
        args += ["--intervention", intervention]
    specification = {"command": args, "parameters": parameters, "seeds": seeds,
                     "executable_sha256": seal(executable)}
    (root / (name + "-spec.json")).write_text(json.dumps(specification, indent=2) + "\n")
    start = time.monotonic()
    with (root / (name + ".log")).open("w") as log:
        subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, check=True)
    try:
        result = summarize(directory)
    except ValueError as error:
        runs = list(read_rows(directory / "runs.csv"))
        failures = [r for r in runs if r["result"] not in ("escaped", "death") or r["stuck"] != "0"]
        result = {"invalid": str(error), "runs": len(runs), "failures": failures,
                  "policy_hash": json.loads((directory / "manifest.json").read_text())["agent_policy_hash"]}
        (directory / "invalid.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"{name}: INVALID ({len(failures)} simulator failures)", flush=True)
        return name, result
    result["seconds"] = time.monotonic() - start
    print(f"{name}: {result['escaped']}/{result['runs']} ({result['escape_percent']:.3f}%), "
          f"depth {result['mean_depth']:.3f}, Yendor {result['yendor']}, "
          f"{result['seconds']:.1f}s", flush=True)
    return name, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--simulator", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--spec", type=Path, help="JSON mapping variant names to full balance parameters")
    parser.add_argument("--seeds", default="1:2000")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--concurrent", type=int, default=1)
    parser.add_argument("--agent", default="explorer-v1", choices=("explorer-v1", "omniscient-v2"))
    parser.add_argument("--agent-params", type=Path)
    parser.add_argument("--max-actions", type=int, default=100000)
    parser.add_argument("--intervention", action="append", default=[], help="additional controlled intervention in every specified arm")
    parser.add_argument("--summarize-only", action="store_true")
    args = parser.parse_args()
    if args.summarize_only:
        print(json.dumps(summarize(args.output), indent=2))
        return
    if not args.simulator or args.output.exists():
        parser.error("a simulator and a fresh output directory are required")
    args.output.mkdir(parents=True)
    variants = json.loads(args.spec.read_text()) if args.spec else screening()
    (args.output / "specifications.json").write_text(json.dumps(variants, indent=2) + "\n")
    results = {}
    with ThreadPoolExecutor(max_workers=args.concurrent) as pool:
        pending = [pool.submit(run_variant, args.simulator.resolve(), args.output.resolve(),
                               name, params, args.seeds, args.jobs, args.agent, args.agent_params, args.max_actions, args.intervention)
                   for name, params in variants.items()]
        for completed in as_completed(pending):
            name, result = completed.result()
            results[name] = result
            hashes = {r["policy_hash"] for r in results.values()}
            if len(hashes) != 1:
                raise ValueError("policy changed during experiment")
            (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
