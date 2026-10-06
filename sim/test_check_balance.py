#!/usr/bin/env python3
"""Check band boundaries, scorecard denominators, and invalid/tampered data rejection."""
import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile

import check_balance as report


def rejected(operation, phrase):
    try:
        operation()
    except ValueError as error:
        assert phrase in str(error),str(error)
    else:
        raise AssertionError("Invalid balance data was accepted")


def main():
    for low,high in report.BANDS.values():
        assert report.band_status(low,(low,high))=="IN BAND"
        assert report.band_status(high,(low,high))=="IN BAND"
        assert report.band_status(low-.0001,(low,high))=="LOW"
        assert report.band_status(high+.0001,(low,high))=="HIGH"
    assert report.band_status(None,(1,3))=="NO DATA"
    assert report.band_status(5,None)=="REVIEW"
    assert report.percentage(0,0) is None
    assert report.percentage(2,4)==50
    with tempfile.TemporaryDirectory(prefix="balance scorecard spaces ") as scratch:
        device=Path(scratch)/"device build"
        device.mkdir()
        (device/"CMakeCache.txt").write_text("AVM_SDK_ROOT:PATH=/example/sdk\n")
        rejected(lambda: report.build_simulator(device,8),"AVM build directory")
        run=Path(scratch)/"valid"
        subprocess.run([sys.argv[1],"--seeds","1:8","--jobs","3","--output",str(run),"--entry-state"],check=True)
        with contextlib.redirect_stdout(io.StringIO()):
            summary=report.make_report(run,sys.argv[1],expected_runs=8)
        saved=json.loads((run/"report/summary.json").read_text())
        floors=report.pd.read_csv(run/"floors.csv")
        assert summary["runs"]==8 and len(saved["floors"])==31
        opening=floors[(floors.direction=="descent") & (floors.floor==0)]
        assert summary["simulator_failures"]==0
        assert saved["floors"][0]["mortality_pct"]==100*int((opening.exited==0).sum())/8
        assert all(r["band_status"] in ("REVIEW","NO DATA") for r in saved["floors"] if r["direction"]=="ascent")
        assert len(saved["provenance"]["output_csv_sha256"])==8
        assert len(saved["provenance"]["entry_state_sha256"])==64
        assert (run/"report/items_per_run.csv").exists() and (run/"report/monsters_per_run.csv").exists()
        rejected(lambda: report.make_report(run,expected_runs=10000),"exactly seeds")
        with (run/"entry_state.csv").open("a") as file:
            file.write("tamper\n")
        rejected(lambda: report.make_report(run,expected_runs=8),"hash mismatch")
        failed=Path(scratch)/"failed"
        subprocess.run([sys.argv[1],"--seed","1","--max-actions","1","--output",str(failed),"--entry-state"],check=True)
        rejected(lambda: report.make_report(failed,expected_runs=1),"simulator failure")
    print("balance scorecard checks passed")


if __name__=="__main__":
    main()
