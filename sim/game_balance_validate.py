#!/usr/bin/env python3
"""Stream ordered simulator sidecars, reconcile counters, and summarize exposure.

Memory is bounded by one seed's content rather than a complete census. The
ordinary balance.py validator remains the detailed dataframe-based reference.
"""
import argparse
import csv
import itertools
import json
from collections import Counter, defaultdict
from pathlib import Path


def groups(path):
    previous = 0
    with path.open(newline="", encoding="utf-8") as stream:
        for seed, rows in itertools.groupby(csv.DictReader(stream), lambda r: int(r["effective_seed"])):
            if seed <= previous:
                raise ValueError(f"unordered or repeated seed in {path}")
            previous = seed
            yield seed, list(rows)


class Cursor:
    def __init__(self, path):
        self.path = path
        self.groups = iter(groups(path))
        self.current = next(self.groups, None)

    def take(self, seed, agent):
        if self.current is None or self.current[0] > seed:
            return []
        if self.current[0] != seed:
            raise ValueError(f"unexpected seed in {self.path}")
        rows = self.current[1]
        self.current = next(self.groups, None)
        if any(any(v is None for v in r.values()) or r["agent"] != agent for r in rows):
            raise ValueError(f"incomplete row or wrong agent in {self.path}")
        return rows


def typed(rows, key):
    values = {r[key]: r for r in rows}
    if len(values) != len(rows):
        raise ValueError(f"duplicate {key}")
    return values


def validate(directory):
    directory = Path(directory)
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    streams = ("floors", "items", "monsters", "visit_items", "visit_monsters", "interventions", "ranged", "features")
    cursors = {name: Cursor(directory / (name + ".csv")) for name in streams}
    exposure, means, row_counts = defaultdict(Counter), Counter(), Counter()
    first, last = manifest["seed_selection"]["first"], manifest["seed_selection"]["last"]
    next_seed, runs = first, 0
    with (directory / "runs.csv").open(newline="", encoding="utf-8") as stream:
        for run in csv.DictReader(stream):
            seed = int(run["effective_seed"])
            if seed != (next_seed or 0xACE1) or run["result"] not in ("escaped", "death") or run["stuck"] != "0":
                raise ValueError("missing/duplicate seed or invalid run outcome")
            next_seed += 1
            if run["agent"] != manifest["agent"] or any(v is None for v in run.values()):
                raise ValueError("wrong agent or incomplete run")
            data = {name: cursor.take(seed, manifest["agent"]) for name, cursor in cursors.items()}
            row_counts.update({name: len(rows) for name, rows in data.items()})
            floors = typed(data["floors"], "visit")
            if set(floors) != {str(v) for v in range(1, int(run["floors_entered"]) + 1)}:
                raise ValueError("missing or duplicate floor visit")
            for field in ("actions", "turns"):
                if sum(int(r[field]) for r in floors.values()) != int(run[field]):
                    raise ValueError(f"floor/run {field} mismatch")
            if sum(int(r["exited"]) for r in floors.values()) != int(run["floors_exited"]):
                raise ValueError("floor exits mismatch")
            for row in floors.values():
                key = (int(row["floor"]), row["direction"], "below_half" if 2 * int(row["entry_hp"]) < int(row["entry_max_hp"]) else "at_least_half")
                if not 0 <= key[0] < 16 or key[1] not in ("descent", "ascent"):
                    raise ValueError("invalid floor/direction")
                exposure[key]["visits"] += 1
                for field in ("entry_hp", "entry_armor_rating", "entry_healing_units", "entry_food_units", "damage_taken", "consumables_used", "items_picked_up"):
                    exposure[key][field] += int(row[field])
            for category, type_key, label in (("items", "item_type", "item"), ("monsters", "monster_type", "monster")):
                whole = typed(data[category], type_key)
                totals, visits = defaultdict(Counter), set()
                fields = set(next(iter(whole.values()), {})) - {"seed", "effective_seed", "agent", type_key, label, "carried"}
                for row in data["visit_" + category]:
                    visit = floors.get(row["visit"])
                    identity = (row["visit"], row[type_key])
                    if identity in visits or visit is None or any(row[k] != visit[k] for k in ("floor", "direction")):
                        raise ValueError("duplicate or invalid typed visit key")
                    visits.add(identity)
                    if row[type_key] not in whole:
                        raise ValueError("typed visit missing from whole-run content")
                    for field in fields:
                        totals[row[type_key]][field] += int(row[field])
                    if category == "items" and row[label] in ("HEALING", "SEE_INVISIBLE", "FOOD", "TORCH", "LANTERN"):
                        key = (int(visit["floor"]), visit["direction"], "below_half" if 2 * int(visit["entry_hp"]) < int(visit["entry_max_hp"]) else "at_least_half")
                        for field in ("generated", "picked_up", "used", "identified", "pack_full"):
                            exposure[key][row[label] + "_" + field] += int(row[field])
                for kind, row in whole.items():
                    if any(totals[kind][field] != int(row[field]) for field in fields):
                        raise ValueError(f"typed visit/run {category} counter mismatch")
            if sum(int(r["killed"]) for r in data["monsters"] if r["monster"] == "LORD") != int(run["lord_killed"]):
                raise ValueError("Lord counter mismatch")
            for field in ("actions", "turns", "fuel_exhaustions", "blind_turns", "light_0", "light_1", "final_hunger"):
                means[field] += int(run[field])
            runs += 1
    if next_seed != last + 1 or runs != manifest["effective_seed_count"] or any(c.current is not None for c in cursors.values()):
        raise ValueError("manifest population or sidecar seed mismatch")
    result = {"runs": runs, "counter_reconciliation": True, "row_counts": dict(row_counts),
              "exposure": [{"floor": k[0], "direction": k[1], "entry_hp_stratum": k[2], **v} for k, v in sorted(exposure.items())]}
    result.update({"mean_" + key: value / runs for key, value in means.items()})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = validate(args.directory)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{result['runs']} complete runs; floor and typed-visit counters reconcile")


if __name__ == "__main__":
    main()
