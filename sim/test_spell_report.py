"""Partial book consumption and per-visit learning must remain independent."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import pandas as pd
import balance
from spell_report import report

def main():
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temp:
        for agent in ("explorer-v1", "omniscient-v2"):
            directory = Path(temp) / agent
            subprocess.run([str(exe), "--agent", agent, "--seeds", "1:32", "--jobs", "2",
                            "--max-actions", "60000", "--output", str(directory)], check=True)
            balance.seal_manifest(directory, exe)
            spells = pd.read_csv(directory / "spells.csv")
            magic = pd.read_csv(directory / "magic.csv")
            assert spells.learned.sum() > magic.books_completed.sum()
            report(directory, directory / "report")
            summary = json.loads((directory / "report/magic.json").read_text())
            assert summary["effect_counters_available"]
            assert summary["totals"]["learned"] == int(spells.learned.sum())
            assert summary["magic"]["books_completed"] == int(magic.books_completed.sum())
            bounds = summary["learned_on_descent"]["all"]["mean_bounds"]
            assert bounds[0] == bounds[1] == spells.learned.sum() / 32
            by_floor = pd.read_csv(directory / "report/spells_by_floor.csv")
            assert by_floor.learned.sum() == spells.learned.sum()
            assert by_floor.casts.sum() == spells.casts.sum()
            assert by_floor.traps_revealed.sum() == spells.traps_revealed.sum()
    print("Multi-spell report checks passed")

if __name__ == "__main__":
    main()
