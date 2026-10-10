"""Controlled map-feature interventions and a streaming paired scorecard.

Run with a frozen simulator; all variants use that executable and policy. Raw
data and manifests stay in ignored build/. No gameplay RNG resynchronization.
"""
import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess

VARIANTS = {
    "original": ["original-teleport-placement", "original-early-traps", "original-holy-generation"],
    "placement": ["original-early-traps", "original-holy-generation"],
    "holy": ["original-early-traps"],
    "final": [],
    "no-teleport": ["no-teleport-traps"],
    "no-early-damage": ["no-early-damaging-traps"],
    "interactive": ["interactive-features-only"],
    "traps": ["traps-only"],
    "no-features": ["no-map-features"],
    "original-density": ["original-early-density"],
    "original-damage": ["original-early-damage"],
    "original-roster": ["original-early-roster"],
}
PAIRS = {
    "A no teleport": ("final", "no-teleport"),
    "B no early damage": ("final", "no-early-damage"),
    "C interactive only vs no features": ("no-features", "interactive"),
    "D traps only vs no features": ("no-features", "traps"),
    "E safe placement": ("original", "placement"),
    "F early tuning": ("holy", "final"),
    "G floor holy probability": ("placement", "holy"),
    "F density component": ("original-density", "final"),
    "F damage component": ("original-damage", "final"),
    "F roster component": ("original-roster", "final"),
    "complete vs no features": ("no-features", "final"),
}


def rows(path):
    with path.open(newline="") as f:
        yield from csv.DictReader(f)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def summarize(path):
    manifest = json.loads((path / "manifest.json").read_text())
    runs = {int(r["effective_seed"]): r for r in rows(path / "runs.csv")}
    if len(runs) != manifest["effective_seed_count"]:
        raise ValueError("missing/duplicate effective seeds")
    failures = [s for s, r in runs.items() if r["result"] not in ("escaped", "death")]
    if failures:
        raise ValueError(f"invalid simulator outcomes: {failures[:10]}")
    n = len(runs)
    out = dict(seeds=n, escaped=sum(r["result"] == "escaped" for r in runs.values()),
               turns=sum(int(r["turns"]) for r in runs.values()),
               actions=sum(int(r["actions"]) for r in runs.values()),
               deaths=dict(collections.Counter(r["death_cause"] for r in runs.values() if r["result"] == "death")),
               policy_hash=manifest["agent_policy_hash"], simulator_failures=0)
    out["escape_percent"] = 100 * out["escaped"] / n
    floors = collections.defaultdict(collections.Counter)
    holy = collections.defaultdict(collections.Counter)
    last_visit = {}
    for r in rows(path / "floors.csv"):
        key = f'{r["direction"]}:{r["floor"]}'
        f = floors[key]
        f["entered"] += 1
        exited = int(r["exited"])
        f["exited"] += exited
        for name in ("actions", "turns", "move_actions"):
            f[name] += int(r.get(name, 0))
        if exited:
            for name in ("entry_hp", "exit_hp", "entry_hunger", "exit_hunger", "entry_level", "exit_level"):
                f[name] += int(r[name])
        seed, visit = int(r["effective_seed"]), int(r["visit"])
        last_visit[seed] = visit
        # Visited-floor distributions are descriptive; use the independent
        # generation census to avoid survivor selection in spawn probabilities.
        hkey = f'{r["floor"]}:{r["archetype"]}:{r.get("unholy_count", "?")}:{int(r.get("holy_candidates", 0)) // 128}:{int(r.get("holy_preferred", 0)) // 128}'
        holy[hkey]["visits"] += 1
    features = collections.defaultdict(collections.Counter)
    teleport_outliers, exposed_death_visits = [], set()
    tele_visits = active_tele_visits = 0
    for r in rows(path / "features.csv"):
        t = int(r["feature_type"])
        f = features[str(t)]
        for name in ("generated", "discovered", "activated", "declined", "damage", "deaths", "spawned", "holy_damage", "repeated", "displacement"):
            f[name] += int(r.get(name, 0))
        f["max_entries"] = max(f["max_entries"], int(r.get("max_entries", 0)))
        seed, visit = int(r["effective_seed"]), int(r["visit"])
        if t <= 20 and int(r["activated"]) and visit == last_visit[seed] and runs[seed]["result"] == "death":
            exposed_death_visits.add(seed)
        if t == 12:
            tele_visits += int(r["generated"]) > 0
            active_tele_visits += int(r["activated"]) > 0
            if int(r.get("max_entries", 0)) > 10:
                teleport_outliers.append(dict(seed=seed, visit=visit, floor=int(r["floor"]),
                                              max_entries=int(r["max_entries"]), activations=int(r["activated"])))
    out["features"] = dict(features)
    out["direct_trap_deaths"] = sum(v["deaths"] for t, v in features.items() if int(t) <= 20)
    out["effective_trap_damage"] = sum(v["damage"] for t, v in features.items() if int(t) <= 20)
    out["holy_visit_conditions"] = dict(holy)
    out["floors"] = dict(floors)
    out["moves"] = sum(f["move_actions"] for f in floors.values())
    out["teleport_generated_visits"] = tele_visits
    out["teleport_active_visits"] = active_tele_visits
    activations = features["12"]["activated"]
    out["teleports_per_generated_visit"] = activations / tele_visits if tele_visits else 0
    out["teleports_per_active_visit"] = activations / active_tele_visits if active_tele_visits else 0
    out["teleport_outliers_over_10"] = sorted(teleport_outliers, key=lambda r: -r["max_entries"])
    out["deaths_after_trap_exposure_without_direct_trap_cause"] = sum(
        not runs[s]["death_cause"].startswith("trap:") for s in exposed_death_visits)
    # This preceding-exposure count is explicitly observational, not a claim
    # that a trap caused those deaths. Discordant action traces resolve examples.
    out["source_hashes"] = {p.name: digest(p) for p in path.glob("*.csv")}
    return out, runs


def report(root):
    summaries, populations = {}, {}
    for name in VARIANTS:
        if (root / name / "manifest.json").exists():
            summaries[name], populations[name] = summarize(root / name)
    paired = {}
    for name, (a, b) in PAIRS.items():
        if a not in summaries or b not in summaries:
            continue
        if summaries[a]["policy_hash"] != summaries[b]["policy_hash"]:
            raise ValueError("paired policies differ")
        ar, br = populations[a], populations[b]
        if ar.keys() != br.keys():
            raise ValueError("paired effective seed populations differ")
        gained = [s for s in ar if ar[s]["result"] != "escaped" and br[s]["result"] == "escaped"]
        lost = [s for s in ar if ar[s]["result"] == "escaped" and br[s]["result"] != "escaped"]
        paired[name] = dict(control=a, treatment=b, gained=len(gained), lost=len(lost),
                            difference_pp=100 * (len(gained) - len(lost)) / len(ar),
                            gained_examples=gained[:5], lost_examples=lost[:5])
        # Paired normal interval and McNemar large-discordance approximation.
        # A full deterministic seed census needs neither sampling intervals nor p.
        if len(ar) < 65535:
            delta = (len(gained) - len(lost)) / len(ar)
            se = math.sqrt((len(gained) + len(lost)) / len(ar) - delta * delta) / math.sqrt(len(ar))
            paired[name]["paired_95_interval_pp"] = [100 * (delta - 1.96 * se), 100 * (delta + 1.96 * se)]
            discordant = len(gained) + len(lost)
            paired[name]["p_mcnemar_approx"] = math.erfc(abs(len(gained) - len(lost)) / math.sqrt(2 * discordant)) if discordant else 1
    ranked = sorted((p["p_mcnemar_approx"], name) for name, p in paired.items() if "p_mcnemar_approx" in p)
    previous = 1
    for rank in range(len(ranked), 0, -1):
        value, name = ranked[rank - 1]
        previous = min(previous, value * len(ranked) / rank)
        paired[name]["q_bh"] = previous
    output = root / "report"
    output.mkdir(exist_ok=True)
    (output / "summary.json").write_text(json.dumps(dict(variants=summaries, pairs=paired), indent=2))
    lines = ["# Map-feature controlled experiments", "", "Each row labels its exact seed population. Raw data, policy identity and SHA-256 seals are retained beside this report.", "",
             "| Variant | Seeds | Escapes | Escape % | Starvation | Turns | Teleport activations | Same-trap maximum/visit |", "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for name, s in summaries.items():
        t = s["features"].get("12", {})
        lines.append(f'| {name} | {s["seeds"]} | {s["escaped"]} | {s["escape_percent"]:.3f} | {s["deaths"].get("starvation", 0)} | {s["turns"]} | {t.get("activated", 0)} | {t.get("max_entries", 0)} |')
    lines += ["", "| Paired experiment | Difference pp | Gained escapes | Lost escapes |", "| --- | --- | --- | --- |"]
    for name, p in paired.items():
        lines.append(f'| {name} | {p["difference_pp"]:+.3f} | {p["gained"]} | {p["lost"]} |')
    lines += ["", "Detailed floor survival, typed activations/uses, effective damage, HP/hunger transition sums, moves/actions, repeated-teleport outliers and immediate death causes are in summary.json. Transition means divide sums by exited visits. Secondary deaths after exposure are descriptive; only paired interventions and audited traces support causal interpretation."]
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("mode", choices=("run", "report"))
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--simulator", type=Path)
    p.add_argument("--seeds", default="1:10000")
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--variants", nargs="+", choices=list(VARIANTS), default=list(VARIANTS))
    args = p.parse_args()
    if args.mode == "run":
        if args.simulator is None:
            p.error("run requires --simulator")
        args.output.mkdir(parents=True, exist_ok=True)
        if any((args.output / name).exists() for name in args.variants):
            p.error("variant output already exists; use fresh directories")
        exe = args.simulator.resolve()
        spec = dict(seeds=args.seeds, variants={n: VARIANTS[n] for n in args.variants}, executable_sha256=digest(exe))
        spec_path = args.output / ("spec-" + "-".join(args.variants) + ".json")
        if spec_path.exists():
            p.error("experiment specification already exists")
        spec_path.write_text(json.dumps(spec, indent=2))
        for name in args.variants:
            if digest(exe) != spec["executable_sha256"]:
                raise ValueError("frozen simulator changed during experiment")
            command = [str(exe), "--seeds", args.seeds, "--jobs", str(args.jobs), "--entry-state",
                       "--experiment", "feature-tuning-20261010", "--variant", name, "--output", str(args.output / name)]
            for rule in VARIANTS[name]:
                command += ["--intervention", rule]
            with (args.output / (name + ".log")).open("w") as log:
                subprocess.run(command, check=True, stdout=log, stderr=log)
            print(name, (args.output / (name + ".log")).read_text().strip(), flush=True)
    else:
        report(args.output)


if __name__ == "__main__":
    main()
