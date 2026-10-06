#!/usr/bin/env python3
"""End-to-end tiny same-binary experiment, reports and tamper rejection."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import pandas as pd


def main():
    exe = sys.argv[1]
    tool = str(Path(__file__).with_name("balance.py"))
    with tempfile.TemporaryDirectory(prefix="balance CLI spaces ") as scratch:
        root = Path(scratch)
        subprocess.run([sys.executable, tool, "ab", "--control-exe", exe, "--treatment-exe", exe,
                        "--seeds", "1:32", "--jobs", "3", "--experiment", "same-binary-test",
                        "--treatment-intervention", "replace-item:HEALING:FOOD:floor=0:direction=descent:max=1",
                        "--bootstrap", "100", "--output", str(root)], check=True)
        for variant in ("control", "treatment"):
            m = json.loads((root / variant / "manifest.json").read_text())
            assert m["variant"] == variant and m["experiment"] == "same-binary-test"
            assert len(m["simulator_executable_sha256"]) == 64 and len(m["output_csv_sha256"]) == 8
            assert m["experiment_specification_sha256"]
        result = json.loads((root / "comparison" / "comparison.json").read_text())
        assert result["effective_seed_count"] == 32 and not result["census"]
        assert (root / "comparison" / "floor_survival.csv").exists()
        subprocess.run([sys.executable, tool, "compare", str(root / "control"), str(root / "control"),
                        "--bootstrap", "100", "--output", str(root / "identity")], check=True)
        identity = json.loads((root / "identity" / "comparison.json").read_text())
        assert identity["primary"]["delta_pp"] == 0 and identity["primary"]["ci_pp"] == [0, 0]
        # No-data factor report still contains explicit insufficient-data status.
        subprocess.run([sys.executable, tool, "factors", str(root / "control"), "--output", str(root / "factors")], check=True)
        f = pd.read_csv(root / "factors" / "factors.csv")
        assert (f.status != "ok").any()
        subprocess.run([sys.executable, tool, "summarize", str(root / "control")], check=True)
        with (root / "treatment" / "runs.csv").open("a") as file:
            file.write("tamper\n")
        rejected = subprocess.run([sys.executable, tool, "compare", str(root / "control"), str(root / "treatment")], capture_output=True, text=True)
        assert rejected.returncode and "hash mismatch" in rejected.stderr
    print("balance CLI checks passed")


if __name__ == "__main__":
    main()
