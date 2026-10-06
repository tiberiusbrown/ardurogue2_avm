#!/usr/bin/env python3
"""Exercise the public executable, seed selection and all CSV streams."""
import csv
from collections import defaultdict
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    exe = sys.argv[1]
    def invoke(*args, valid=True):
        p = subprocess.run([exe, *args], capture_output=True, text=True, check=False)
        assert (p.returncode == 0) == valid, (args, p.stderr)
        return p

    quiet = invoke("--seed", "1")
    traced = invoke("--seed", "0x1", "--trace")
    assert quiet.stdout == traced.stdout
    assert "killed LORD" in traced.stderr and "picked up YENDOR_AMULET" in traced.stderr
    assert "result=escaped" in traced.stderr
    for args in (("--seed", "65536"), ("--count", "0"), ("--seeds", "5:1"),
                 ("--count", "2", "--trace"), ("--seed", "1", "--count", "2"),
                 ("--start-seed", "65535", "--count", "2"), ("--jobs", "0"), ("--jobs", "65"),
                 ("--jobs",), ("--seed",), ("--unknown",)):
        invoke(*args, valid=False)
    invoke("--entry-state",valid=False)
    for args in (("--all-seeds", "--seed", "4"), ("--seeds", "0:65535"),
                 ("--intervention", "replace-item:DAGGER:FOOD:info=preserve"),
                 ("--intervention", "replace-monster:BAT:INVALID")):
        invoke(*args, valid=False)
    stuck = list(csv.DictReader(io.StringIO(invoke("--seed", "4", "--max-actions", "1").stdout)))[0]
    assert stuck["result"] == "SIM_STUCK" and stuck["stuck"] == "1"
    zero = list(csv.DictReader(io.StringIO(invoke("--seed", "0").stdout)))[0]
    assert zero["seed"] == "0" and zero["effective_seed"] == str(0xACE1)
    serial=invoke("--seeds","0:8")
    parallel=invoke("--seeds","0:8","--jobs","3")
    assert (serial.stdout,serial.stderr)==(parallel.stdout,parallel.stderr)
    assert invoke("--seed","1","--jobs","8","--trace").stdout==quiet.stdout
    limited=invoke("--seeds","1:5","--max-actions","1","--no-telemetry")
    assert limited.stdout==invoke("--seeds","1:5","--jobs","3","--max-actions","1","--no-telemetry").stdout
    with tempfile.TemporaryDirectory(prefix="ardurogue2-sim-cli-") as scratch:
        first, second, concurrent = Path(scratch) / "range", Path(scratch) / "count", Path(scratch) / "parallel output"
        invoke("--seeds", "1:8", "--output", str(first))
        invoke("--count", "8", "--start-seed", "1", "--output", str(second))
        invoke("--seeds", "1:8", "--jobs", "3", "--output", str(concurrent))
        entry_serial, entry_parallel = Path(scratch)/"entry serial", Path(scratch)/"entry parallel"
        invoke("--seeds","1:8","--output",str(entry_serial),"--entry-state")
        invoke("--seeds","1:8","--jobs","3","--output",str(entry_parallel),"--entry-state")
        invoke("--seed","1","--output",str(Path(scratch)/"invalid entry"),"--entry-state","--no-telemetry",valid=False)
        rows = {}
        streams = ("runs", "floors", "items", "monsters", "visit_items", "visit_monsters", "interventions")
        for name in streams:
            path = first / f"{name}.csv"
            assert path.read_bytes() == (second / path.name).read_bytes(), name
            assert path.read_bytes() == (concurrent / path.name).read_bytes(), f"parallel {name}"
            assert path.read_bytes() == (entry_serial/path.name).read_bytes(), f"entry collection perturbed {name}"
            assert path.read_bytes() == (entry_parallel/path.name).read_bytes(), f"parallel entry collection perturbed {name}"
            with path.open(newline="") as f:
                rows[name] = list(csv.DictReader(f))
            assert all(r["agent"] == "omniscient-v2" for r in rows[name])
            if name != "interventions":
                assert rows[name] and set(r["seed"] for r in rows[name]) == set(map(str, range(1, 9)))
        assert len(rows["runs"]) == 8
        assert (entry_serial/"entry_state.csv").read_bytes()==(entry_parallel/"entry_state.csv").read_bytes()
        with (entry_serial/"entry_state.csv").open(newline="") as file:
            entry_rows=list(csv.DictReader(file))
        assert len(entry_rows)==len(rows["floors"])
        assert json.loads((entry_serial/"manifest.json").read_text())["entry_state_schema_version"]==1
        assert all(int(r["entry_wand_charges"])==int(r["entry_offensive_charges"])+int(r["entry_emergency_charges"]) for r in entry_rows)
        for run in rows["runs"]:
            visits = [f for f in rows["floors"] if f["seed"] == run["seed"]]
            assert sum(int(f["actions"]) for f in visits) == int(run["actions"])
            assert sum(int(f["turns"]) for f in visits) == int(run["turns"])
            assert len(visits) == int(run["floors_entered"])
        for category, type_key in (("items", "item_type"), ("monsters", "monster_type")):
            counters = [k for k in rows[category][0] if k not in ("seed", "effective_seed", "agent", type_key, "item", "monster", "carried")]
            totals = defaultdict(lambda: defaultdict(int))
            for row in rows["visit_" + category]:
                for k in counters:
                    totals[(row["effective_seed"], row[type_key])][k] += int(row[k])
            for row in rows[category]:
                for k in counters:
                    assert totals[(row["effective_seed"], row[type_key])][k] == int(row[k]), (category, k, row)
        manifest = json.loads((first / "manifest.json").read_text())
        assert manifest["agent"] == "omniscient-v2" and manifest["telemetry_schema_version"] == 2
        assert manifest["effective_seed_count"] == 8 and manifest["variant"] == "control"
        for row in rows["floors"]:
            assert int(row["entry_max_hp"]) >= int(row["entry_hp"]) and int(row["floor_tiles"]) > 0
            assert row["archetype"] in ("CHAMBERS", "WARREN", "FORTRESS", "RUINS")
        rule = "replace-item:HEALING:FOOD:floor=0:direction=descent:max=2"
        treatment = Path(scratch) / "treatment serial"
        parallel_treatment = Path(scratch) / "treatment parallel"
        common = ("--seeds", "1:8", "--experiment", "proof", "--variant", "treatment", "--intervention", rule)
        invoke(*common, "--output", str(treatment))
        invoke(*common, "--jobs", "3", "--output", str(parallel_treatment))
        for name in streams:
            assert (treatment / (name + ".csv")).read_bytes() == (parallel_treatment / (name + ".csv")).read_bytes(), name
        with (treatment / "interventions.csv").open(newline="") as file:
            ledger = list(csv.DictReader(file))
        assert ledger and all(r["experiment"] == "proof" and r["variant"] == "treatment" and 0 < int(r["count"]) <= 2 for r in ledger)
        # Population cap and pre-scan ordering reconcile from ledger to visits.
        with (treatment / "visit_items.csv").open(newline="") as file:
            changed = list(csv.DictReader(file))
        for record in ledger:
            seed = record["seed"]
            before = {r["item"]: int(r["generated"]) for r in rows["visit_items"] if r["seed"] == seed and r["visit"] == "1"}
            after = {r["item"]: int(r["generated"]) for r in changed if r["seed"] == seed and r["visit"] == "1"}
            assert before.get("HEALING", 0) - after.get("HEALING", 0) == int(record["count"])
            assert after.get("FOOD", 0) - before.get("FOOD", 0) == int(record["count"])
        invoke("--seeds", "1:8", "--output", str(first), valid=False)
    census = invoke("--all-seeds", "--jobs", "8", "--max-actions", "1", "--no-telemetry")
    seeds = [int(r["effective_seed"]) for r in csv.DictReader(io.StringIO(census.stdout))]
    assert seeds == list(range(1, 65536))
    print("simulator CLI checks passed")


if __name__ == "__main__":
    main()
