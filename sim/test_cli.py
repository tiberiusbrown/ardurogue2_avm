#!/usr/bin/env python3
"""Exercise the public executable, seed selection and all CSV streams."""
import csv
import io
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

    quiet = invoke("--seed", "4")
    traced = invoke("--seed", "0x4", "--trace")
    assert quiet.stdout == traced.stdout
    assert "killed LORD" in traced.stderr and "picked up YENDOR_AMULET" in traced.stderr
    assert "result=escaped" in traced.stderr
    for args in (("--seed", "65536"), ("--count", "0"), ("--seeds", "5:1"),
                 ("--count", "2", "--trace"), ("--seed", "1", "--count", "2"),
                 ("--start-seed", "65535", "--count", "2"), ("--jobs", "0"), ("--jobs", "65"),
                 ("--jobs",), ("--seed",), ("--unknown",)):
        invoke(*args, valid=False)
    stuck = list(csv.DictReader(io.StringIO(invoke("--seed", "4", "--max-actions", "1").stdout)))[0]
    assert stuck["result"] == "SIM_STUCK" and stuck["stuck"] == "1"
    zero = list(csv.DictReader(io.StringIO(invoke("--seed", "0").stdout)))[0]
    assert zero["seed"] == "0" and zero["effective_seed"] == str(0xACE1)
    serial=invoke("--seeds","0:8")
    parallel=invoke("--seeds","0:8","--jobs","3")
    assert (serial.stdout,serial.stderr)==(parallel.stdout,parallel.stderr)
    assert invoke("--seed","4","--jobs","8","--trace").stdout==quiet.stdout
    limited=invoke("--seeds","1:5","--max-actions","1","--no-telemetry")
    assert limited.stdout==invoke("--seeds","1:5","--jobs","3","--max-actions","1","--no-telemetry").stdout
    with tempfile.TemporaryDirectory(prefix="ardurogue2-sim-cli-") as scratch:
        first, second, concurrent = Path(scratch) / "range", Path(scratch) / "count", Path(scratch) / "parallel output"
        invoke("--seeds", "1:8", "--output", str(first))
        invoke("--count", "8", "--start-seed", "1", "--output", str(second))
        invoke("--seeds", "1:8", "--jobs", "3", "--output", str(concurrent))
        rows = {}
        for name in ("runs", "floors", "items", "monsters"):
            path = first / f"{name}.csv"
            assert path.read_bytes() == (second / path.name).read_bytes(), name
            assert path.read_bytes() == (concurrent / path.name).read_bytes(), f"parallel {name}"
            with path.open(newline="") as f:
                rows[name] = list(csv.DictReader(f))
            assert rows[name] and all(r["agent"] == "omniscient-v2" for r in rows[name])
            assert set(r["seed"] for r in rows[name]) == set(map(str, range(1, 9)))
        assert len(rows["runs"]) == 8
        for run in rows["runs"]:
            visits = [f for f in rows["floors"] if f["seed"] == run["seed"]]
            assert sum(int(f["actions"]) for f in visits) == int(run["actions"])
            assert sum(int(f["turns"]) for f in visits) == int(run["turns"])
            assert len(visits) == int(run["floors_entered"])
    print("simulator CLI checks passed")


if __name__ == "__main__":
    main()
