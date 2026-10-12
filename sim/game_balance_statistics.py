#!/usr/bin/env python3
"""Paired run-level screening statistics; invalid batches receive no estimates.

Use balance.py for full telemetry reconciliation and detailed pair reports.
This bounded-memory family report also works while retaining census sidecars.
"""
import argparse
import csv
import json
from pathlib import Path

import numpy as np
from scipy import stats
from statsmodels.stats.multitest import multipletests
from balance import is_census, paired_binary, paired_bootstrap


def read(directory):
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    with (directory / "runs.csv").open(newline="", encoding="utf-8") as stream:
        rows = {int(r["effective_seed"]): r for r in csv.DictReader(stream)}
    if len(rows) != manifest["effective_seed_count"]:
        raise ValueError("missing or duplicate effective seeds")
    if any(r["result"] not in ("death", "escaped") or r["stuck"] != "0" for r in rows.values()):
        raise ValueError("simulator failures invalidate the batch")
    return manifest, rows


def compare(control, candidate, repetitions=2000):
    bm, b = read(control)
    cm, c = read(candidate)
    for key in ("agent", "agent_policy_hash", "telemetry_schema_version"):
        if bm[key] != cm[key]:
            raise ValueError(f"incompatible {key}")
    if bm["options"]["max_actions"] != cm["options"]["max_actions"] or b.keys() != c.keys():
        raise ValueError("action limits or effective seeds differ")
    seeds = sorted(b)
    census = bool(is_census(seeds))
    escaped = paired_binary([b[s]["result"] == "escaped" for s in seeds],
                            [c[s]["result"] == "escaped" for s in seeds], census, repetitions)
    difference = np.array([int(c[s]["deepest_floor"]) - int(b[s]["deepest_floor"]) for s in seeds])
    positive, negative = int(sum(difference > 0)), int(sum(difference < 0))
    depth = {"delta_mean": float(difference.mean()), "deeper": positive, "shallower": negative,
             "mean_ci": None if census else paired_bootstrap(difference, repetitions),
             "sign_test_p": None if census else stats.binomtest(positive, positive + negative).pvalue if positive + negative else 1.0}
    return {"runs": len(seeds), "census": census, "escape": escaped, "depth": depth}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--control", default="baseline")
    parser.add_argument("--baseline", type=Path, help="control directory outside the candidate family")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bootstrap", type=int, default=2000)
    args = parser.parse_args()
    family = {}
    for candidate in sorted(args.root.iterdir()):
        if candidate.name == args.control or not (candidate / "manifest.json").exists():
            continue
        try:
            family[candidate.name] = compare(args.baseline or args.root / args.control, candidate, args.bootstrap)
        except ValueError as error:
            family[candidate.name] = {"invalid": str(error)}
    # Keep invalid planned comparisons in the denominator conservatively; they
    # receive no effect estimate, and p=1 cannot produce a discovery.
    if family and not any(r.get("census") for r in family.values()):
        for metric, field in (("escape", "p_value"), ("depth", "sign_test_p")):
            values = [r.get(metric, {}).get(field, 1.0) for r in family.values()]
            q = multipletests(values, method="fdr_bh")[1]
            for result, value in zip(family.values(), q):
                if metric in result:
                    result[metric]["q_value"] = float(value)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"control": str(args.baseline or args.root / args.control), "comparisons": family,
        "fdr_families": "escape McNemar and paired depth sign tests, separately; invalid planned tests count as p=1",
        "bootstrap_repetitions": args.bootstrap}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
