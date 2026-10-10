#!/usr/bin/env python3
"""Stream schema-5 paired monster-trap telemetry without loading large CSVs."""
import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

TRAPS = dict(enumerate(("Spike", "Poison dart", "Flame", "Frost", "Snare", "Teleport", "Confusion", "Magic drain", "Summoning", "Rust"), 1))
MONSTER_FIELDS = ("monster_activated", "monster_damage", "monster_killed", "monster_teleported", "monster_status", "monster_spawned")


def rows(folder, name):
    with (folder / (name + ".csv")).open(newline="", encoding="utf-8") as stream:
        yield from csv.DictReader(stream)


def summarize(folder):
    totals, deaths, results = Counter(), Counter(), Counter()
    count = 0
    for row in rows(folder, "runs"):
        count += 1
        assert row["result"] in ("escaped", "death") and int(row["stuck"]) == 0, row
        results[row["result"]] += 1
        if row["death_cause"]: deaths[row["death_cause"]] += 1
        for key in ("actions", "turns", "score", "level", "xp", "final_hunger", "has_yendor", "floors_entered"):
            totals[key] += int(row[key])
    assert not any(results[key] for key in ("SIM_ERROR", "SIM_STUCK", "sim_error", "sim_stuck")), results
    features = defaultdict(Counter)
    for row in rows(folder, "features"):
        type_id = int(row["feature_type"])
        for key in MONSTER_FIELDS + ("activated", "damage", "deaths", "spawned", "displacement"):
            features[type_id][key] += int(row[key])
    for row in rows(folder, "monsters"):
        for key in ("generated", "encountered", "engaged", "killed", "player_attacks", "damage_to_monster", "monster_attacks", "damage_to_player"):
            totals["monsters_" + key] += int(row[key])
        if row["monster"] == "LORD": totals["lords_killed"] += int(row["killed"])
    for row in rows(folder, "items"):
        if row["item"] == "FOOD": totals["food_rations_consumed"] += int(row["consumed"])
    for row in rows(folder, "floors"):
        for key in ("damage_dealt", "damage_taken", "exit_hunger"):
            totals[key] += int(row[key])
    assert count > 0, "empty run population"
    return dict(seeds=count, results=dict(results), deaths=dict(deaths), totals=dict(totals),
                means={key: value/count for key,value in totals.items()},
                features={key:dict(value) for key,value in sorted(features.items())})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paired", type=Path, help="directory containing control/ and treatment/")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = {name:summarize(args.paired/name) for name in ("control", "treatment")}
    assert report["control"]["seeds"] == report["treatment"]["seeds"]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output/"monster-traps.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    lines = ["| Metric (mean/run) | Control | Monster traps |", "| --- | ---: | ---: |"]
    for key in report["control"]["means"]:
        lines.append(f"| {key} | {report['control']['means'][key]:.3f} | {report['treatment']['means'][key]:.3f} |")
    lines += ["", "| Trap | Activations | HP damage | Kills | Teleports | Status extensions | Summons |", "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for index,name in TRAPS.items():
        values=report["treatment"]["features"].get(index*2,{})
        lines.append("| " + name + " | " + " | ".join(str(values.get(key,0)) for key in MONSTER_FIELDS) + " |")
    (args.output/"monster-traps.md").write_text("\n".join(lines)+"\n",encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__": main()
