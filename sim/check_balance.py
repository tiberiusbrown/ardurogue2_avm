#!/usr/bin/env python3
"""Build current mechanics, run seeds 1..10000, and write a human balance scorecard."""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

# Use the documented local analysis environment when invoked with ordinary Python.
try:
    import balance
except ModuleNotFoundError as error:
    venv = ROOT / "build/balance-venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if __name__=="__main__" and venv.is_file() and Path(sys.executable).resolve() != venv.resolve():
        raise SystemExit(subprocess.call([str(venv), str(Path(__file__).resolve()), *sys.argv[1:]]))
    raise SystemExit(f"Missing analysis dependency: {error.name}.\n"
                     "Install once: python -m pip install -r sim/requirements-balance.txt") from error

np, pd = balance.np, balance.pd
BANDS = {0: (8, 15), **{f: (1, 3) for f in range(1, 5)},
         **{f: (1, 2.5) for f in range(5, 9)}, **{f: (1.5, 3) for f in range(9, 12)},
         12: (2, 4), 13: (3, 6), 14: (4, 7), 15: (5, 9)}
STATE_FIELDS = ("entry_control_units", "entry_wand_charges", "entry_offensive_charges",
                "entry_emergency_charges", "entry_ring_invisibility", "entry_speed_amulet", "entry_invisible")
ENTRY_FIELDS = ("entry_hp", "entry_max_hp", "entry_level", "entry_strength", "entry_dexterity",
                "entry_armor_rating", "entry_weapon_enchant", "entry_armor_enchant",
                "entry_food_units", "entry_healing_units", "entry_hunger", "entry_speed", *STATE_FIELDS)


def band_status(value, band):
    if value is None:
        return "NO DATA"
    if band is None:
        return "REVIEW"
    return "LOW" if value < band[0] else "HIGH" if value > band[1] else "IN BAND"


def percentage(numerator, denominator):
    return 100 * numerator / denominator if denominator else None


def fmt(value):
    return "n/a" if value is None else f"{value:.2f}"


def table(headers, rows):
    return ["| " + " | ".join(headers) + " |", "| " + " | ".join("---" for _ in headers) + " |",
            *("| " + " | ".join(str(v) for v in row) + " |" for row in rows)]


def windows_build_environment():
    env = os.environ.copy()
    if os.name != "nt":
        return env, None
    env = {name.upper(): value for name, value in env.items()}
    vswhere = Path(env.get("PROGRAMFILES(X86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.is_file():
        return env, None
    installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*",
                                            "-property", "installationPath"], text=True).strip()
    if not installation:
        return env, None
    install = Path(installation)
    vcvars = install / "VC/Auxiliary/Build/vcvars64.bat"
    if vcvars.is_file():
        # Read the developer environment into this process; do not print its contents.
        result = subprocess.run(f'call "{vcvars}" >nul && set', shell=True,
                                capture_output=True, text=True, check=True)
        for line in result.stdout.splitlines():
            name, separator, value = line.partition("=")
            if separator and name:
                env[name.upper()] = value
    compiler = install / "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    return env, compiler if compiler.is_file() else None


def build_simulator(build_dir, jobs):
    cache = build_dir/"CMakeCache.txt"
    if cache.is_file() and any(line.startswith("AVM_SDK_ROOT:") and line.partition("=")[2]
                               for line in cache.read_text(encoding="utf-8").splitlines()):
        raise ValueError("This is an AVM build directory; choose a separate native --build-dir.")
    if not shutil.which("cmake"):
        raise ValueError("CMake is required to build the simulator; install it or use --simulator EXE.")
    # A dedicated native directory avoids reconfiguring the AVM build or its SDK.
    env, compiler = windows_build_environment()
    configure = ["cmake", "-S", str(ROOT), "-B", str(build_dir),
                 "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DAVM_SDK_ROOT=",
                 f"-DBALANCE_PYTHON={Path(sys.executable).as_posix()}"]
    if os.name == "nt" and env.get("WINDOWSSDKDIR") and env.get("WINDOWSSDKVERSION"):
        rc = Path(env["WINDOWSSDKDIR"]) / "bin" / env["WINDOWSSDKVERSION"].rstrip("\\/") / "x64/rc.exe"
        if rc.is_file():
            configure += [f"-DCMAKE_RC_COMPILER={rc.as_posix()}"]
    # Prefer the bundled Windows Clang with Ninja when installed. Existing caches
    # retain their compiler/generator; other platforms use CMake's defaults.
    ninja = shutil.which("ninja", path=env.get("PATH"))
    if not cache.exists() and compiler and ninja:
        configure += ["-G", "Ninja", f"-DCMAKE_CXX_COMPILER={compiler.as_posix()}",
                      f"-DCMAKE_MAKE_PROGRAM={Path(ninja).as_posix()}"]
    subprocess.run(configure, check=True, env=env)
    subprocess.run(["cmake", "--build", str(build_dir), "--config", "RelWithDebInfo",
                    "--target", "ardurogue2_sim", "--parallel", str(jobs)], check=True, env=env)
    suffix = ".exe" if os.name == "nt" else ""
    for relative in (f"sim/RelWithDebInfo/ardurogue2_sim{suffix}", f"sim/ardurogue2_sim{suffix}"):
        executable = build_dir / relative
        if executable.is_file():
            return executable.resolve()
    raise ValueError(f"Simulator executable not found in {build_dir}")


def load_entry_state(directory, manifest, floors):
    path = directory / "entry_state.csv"
    if manifest.get("entry_state_schema_version") != 1 or not path.is_file():
        raise ValueError("Entry reserves/occupancy are missing. Rerun with this script or simulator --entry-state.")
    if manifest.get("entry_state_csv_sha256") and balance.sha256(path)!=manifest["entry_state_csv_sha256"]:
        raise ValueError("Entry-state CSV hash mismatch")
    state = pd.read_csv(path, keep_default_na=False)
    keys = ["seed", "effective_seed", "agent", "visit", "floor", "direction"]
    if any(c not in state for c in (*keys, *STATE_FIELDS)) or len(state) != len(floors):
        raise ValueError("Entry-state schema/count does not match floor visits")
    joined = floors.merge(state, on=keys, how="left", validate="one_to_one")
    if joined[list(STATE_FIELDS)].isna().any().any():
        raise ValueError("Missing or mismatched entry-state visit keys")
    if any(not state[c].isin((0, 1)).all() for c in STATE_FIELDS[-3:]):
        raise ValueError("Invalid entry-state occupancy flag")
    if (state[list(STATE_FIELDS[:4])] < 0).any().any() or not (
            state.entry_wand_charges == state.entry_offensive_charges + state.entry_emergency_charges).all():
        raise ValueError("Invalid entry-state reserve counters")
    return joined


def make_report(directory, executable=None, expected_runs=10000, baseline=None):
    manifest, data = balance.load_directory(directory)
    runs = data["runs"]
    if manifest["agent"] != "omniscient-v2" or set(runs.effective_seed) != set(range(1, expected_runs + 1)):
        raise ValueError(f"Expected maintained omniscient-v2 and exactly seeds 1..{expected_runs}")
    if manifest.get("interventions"):
        raise ValueError("This scorecard requires production mechanics without interventions")
    balance.reconcile_telemetry(data)  # Also rejects simulator failures via load_directory.
    floors = load_entry_state(directory, manifest, data["floors"])
    manifest = balance.seal_manifest(directory, executable)
    manifest["entry_state_csv_sha256"] = balance.sha256(directory/"entry_state.csv")
    balance.write_json(directory/"manifest.json",manifest)
    # Release sparse visit telemetry after the framework's reconciliation.
    del data["visit_items"], data["visit_monsters"]
    n = len(runs)
    census = balance.is_census(runs.effective_seed.to_numpy())
    deaths = runs[runs.result.eq("death")]
    escaped = int(runs.result.eq("escaped").sum())
    yendor = int(runs.has_yendor.sum())
    monsters = data["monsters"].drop(columns=["seed","effective_seed","monster_type"]).groupby("monster").sum(numeric_only=True)
    items = data["items"].drop(columns=["seed","effective_seed","item_type"]).groupby("item").sum(numeric_only=True)
    item_types = data["items"][["item","item_type"]].drop_duplicates().set_index("item").item_type
    rows = []
    for direction in ("descent", "ascent"):
        # There are 16 descent visits and 15 ascent visits; ascent 15 never occurs.
        order = range(16) if direction == "descent" else range(14, -1, -1)
        for floor in order:
            visits = floors[(floors.direction == direction) & (floors.floor == floor)]
            entered = len(visits)
            failed = int((visits.exited == 0).sum())
            mortality = percentage(failed, entered)
            band = BANDS[floor] if direction == "descent" else None
            row = dict(direction=direction, floor=floor, entered=entered, deaths=failed,
                       reach_pct=percentage(entered, n), mortality_pct=mortality,
                       survival_pct=100 - mortality if mortality is not None else None,
                       target_low_pct=band[0] if band else None, target_high_pct=band[1] if band else None,
                       band_status=band_status(mortality, band))
            if entered:
                if not census:
                    low, high = balance.stats.binomtest(failed, entered).proportion_ci(method="wilson")
                    row.update(mortality_ci_low_pct=100 * low, mortality_ci_high_pct=100 * high)
                row["entry_hp_fraction_mean"] = float((visits.entry_hp / visits.entry_max_hp).mean())
                row["top_weapon_pct"] = percentage(int(visits.entry_weapon_type.eq(item_types["TWO_HANDED_SWORD"]).sum()), entered)
                row["plate_pct"] = percentage(int(visits.entry_armor_type.eq(item_types["PLATE_MAIL"]).sum()), entered)
                for field in (*ENTRY_FIELDS, "damage_taken", "damage_dealt", "consumables_used", "actions", "turns"):
                    row[field + "_mean"] = float(visits[field].mean())
                    row[field + "_median"] = float(visits[field].median())
                for field in STATE_FIELDS[-3:]:
                    row[field + "_pct"] = 100 * float(visits[field].mean())
            rows.append(row)
    violations = [r for r in rows if r["direction"] == "descent" and r["band_status"] != "IN BAND"]
    opening_deaths = rows[0]["deaths"]
    summary = dict(runs=n,escaped=escaped,escape_pct=percentage(escaped,n),deaths=len(deaths),
                   reach12_pct=percentage(int(runs.deepest_floor.ge(12).sum()),n),
                   reach15_pct=percentage(int(runs.deepest_floor.ge(15).sum()),n),
                   lord_kills=int(monsters.loc["LORD","killed"]),yendor_acquisitions=yendor,
                   post_yendor_survival_pct=percentage(int((runs.result.eq("escaped") & runs.has_yendor.eq(1)).sum()),yendor),
                   floor0_share_of_deaths_pct=percentage(opening_deaths,len(deaths)),
                   short_opening_deaths=int((deaths.deepest_floor.eq(0) & deaths.actions.le(12)).sum()),
                   zero_progression_deaths=int((deaths.deepest_floor.eq(0) & deaths.level.eq(1) & deaths.score.eq(0)).sum()),
                   starvation_deaths=int(deaths.death_cause.eq("starvation").sum()),simulator_failures=0,
                   consumables_per_run=float(floors.consumables_used.sum()/n),
                   healing_drunk_per_run=float(items.loc["HEALING","potions_drunk"]/n),
                   control_used_per_run=float(items.loc[["CONFUSION","PARALYSIS","SLOWING","STRENGTH",
                       "INVISIBILITY","SCROLL_FEAR","SCROLL_TELEPORT","SCROLL_MASS_CONFUSE"],"used"].sum()/n),
                   wand_charges_used_per_run=float(items.charges_used.sum()/n),
                   outside_bands=len(violations),floor0_death_concentration_review=(
                       opening_deaths >= .5 * len(deaths) if len(deaths) else False))
    for field in ("actions","turns","score","deepest_floor","level"):
        summary[field+"_mean"] = float(runs[field].mean())
        summary[field+"_median"] = float(runs[field].median())
    causes = deaths.death_cause.value_counts().rename_axis("cause").reset_index(name="deaths")
    causes["per_1000_runs"] = causes.deaths * 1000 / n
    causes["share_of_deaths_pct"] = causes.deaths * 100 / len(deaths) if len(deaths) else 0
    out = directory / "report"
    out.mkdir(exist_ok=True)
    pd.DataFrame(rows).to_csv(out / "floors.csv",index=False)
    causes.to_csv(out / "death_causes.csv",index=False)
    items.div(n).to_csv(out / "items_per_run.csv")
    burden = monsters.div(n)
    burden["damage_per_engagement"] = monsters.damage_to_player / monsters.engaged.replace(0,np.nan)
    burden["hit_pct"] = 100 * monsters.monster_hits / monsters.monster_attacks.replace(0,np.nan)
    burden.to_csv(out / "monsters_per_run.csv")
    paired = None
    if baseline:
        comparison = balance.compare(baseline,directory,output=out / "comparison")
        paired = comparison["primary"]
    provenance = dict(agent=manifest["agent"],agent_policy_hash=manifest["agent_policy_hash"],seed_range=f"1..{expected_runs}",
                      git_sha=manifest["git_sha"],git_dirty=manifest["git_dirty"],
                      workspace_source_sha256={str(p.relative_to(ROOT)):balance.sha256(p)
                          for folder in (ROOT/"src",ROOT/"sim") for p in sorted(folder.glob("*"))
                          if p.suffix in (".cpp",".hpp")},
                      executable_sha256=manifest.get("simulator_executable_sha256"),
                      output_csv_sha256=manifest["output_csv_sha256"],
                      entry_state_schema_version=1,entry_state_sha256=balance.sha256(directory/"entry_state.csv"),
                      visit_counters_reconciled=True)
    balance.write_json(out/"summary.json",dict(summary=summary,floors=rows,violations=violations,
                                              paired_comparison=paired,provenance=provenance))
    text = ["# Balance check: 10,000 seeds" if n==10000 else f"# Balance check: {n:,} seeds", "",
            f"Maintained omniscient-v2; seeds 1..{expected_runs}; validated and reconciled telemetry.", "",
            f"**{len(violations)} descent floors outside their design bands.**", "",
            "Bands are broad design goals. LOW/HIGH uses the unrounded point estimate. "
            + ("This is the complete deterministic seed population; sampling intervals are omitted. " if census else
               "The 95% Wilson interval shows finite-sample uncertainty. ")
            + "Overall escape and ascent have no numerical targets.", "",
            "## Outcomes", ""]
    text += table(["Metric","Result"],[(k.replace("_"," "),fmt(v)) for k,v in summary.items()])
    if paired:
        text += ["", "## Paired change from baseline", "",
                 f"Escape delta: {paired['delta_pp']:+.2f} percentage points; "
                 f"95% paired bootstrap CI: {paired['ci_pp']}; "
                 f"gains {paired['baseline_loss_candidate_win']}, losses {paired['baseline_win_candidate_loss']}.",
                 "See comparison/compare.md for floor, outcome, item and monster deltas."]
    for direction in ("descent","ascent"):
        phase = [r for r in rows if r["direction"]==direction]
        text += ["",f"## {direction.capitalize()} progression", ""]
        text += table(["Floor","Entered","Deaths","Reach %","Mortality %","95% CI","Survival %","Band %","Status"],
            [(r["floor"],r["entered"],r["deaths"],fmt(r["reach_pct"]),fmt(r["mortality_pct"]),
              f"{fmt(r.get('mortality_ci_low_pct'))}–{fmt(r.get('mortality_ci_high_pct'))}",fmt(r["survival_pct"]),
              f"{r['target_low_pct']}–{r['target_high_pct']}" if r["target_low_pct"] is not None else "unspecified",r["band_status"])
             for r in phase])
        text += ["", "Entry means among runs reaching each floor (survivor selection applies).", ""]
        text += table(["Floor","HP/max HP","Level","STR/DEX","Armor","Food","Healing","Control","Wand charges","Offensive/Emergency"],
            [(r["floor"],f"{fmt(r.get('entry_hp_mean'))}/{fmt(r.get('entry_max_hp_mean'))}",fmt(r.get("entry_level_mean")),
              f"{fmt(r.get('entry_strength_mean'))}/{fmt(r.get('entry_dexterity_mean'))}",fmt(r.get("entry_armor_rating_mean")),
              fmt(r.get("entry_food_units_mean")),fmt(r.get("entry_healing_units_mean")),fmt(r.get("entry_control_units_mean")),
              fmt(r.get("entry_wand_charges_mean")),f"{fmt(r.get('entry_offensive_charges_mean'))}/{fmt(r.get('entry_emergency_charges_mean'))}") for r in phase])
        text += ["", "Equipment and accessory entry occupancy; worn jewelry includes cursed instances.", ""]
        text += table(["Floor","Heavy weapon %","Plate %","Invisibility ring %","Speed amulet %","Actually invisible %","Speed budget"],
            [(r["floor"],*(fmt(r.get(k)) for k in ("top_weapon_pct","plate_pct","entry_ring_invisibility_pct",
              "entry_speed_amulet_pct","entry_invisible_pct","entry_speed_mean"))) for r in phase])
    text += ["", "## Death causes", ""] + table(list(causes.columns),[
        (r.cause,r.deaths,fmt(r.per_1000_runs),fmt(r.share_of_deaths_pct)) for r in causes.itertuples()])
    text += ["", "## Item activity per starting run", ""] + table(
        ["Item","Generated","Picked up","Used","Consumed","Equipped","Equipped turns","Charges used"],
        [(name,*(fmt(float(row[k]/n)) for k in ("generated","picked_up","used","consumed","equipped","turns_equipped","charges_used")))
         for name,row in items.iterrows()])
    text += ["", "## Monster burden per starting run", ""] + table(
        ["Monster","Generated","Encountered","Engaged","Killed","Damage/run","Damage/engagement","Hit %","Deaths/run"],
        [(name,*(fmt(float(row[k]/n)) for k in ("generated","encountered","engaged","killed","damage_to_player")),
          fmt(float(burden.loc[name,"damage_per_engagement"])) if row.engaged else "n/a",
          fmt(float(burden.loc[name,"hit_pct"])) if row.monster_attacks else "n/a",fmt(float(row.deaths_caused/n)))
         for name,row in monsters.iterrows()])
    import ranged_report
    text += ["", ranged_report.report(data, out)]
    text += ["", "## Definitions and provenance", "",
             "Mortality = failed visits / entered visits; reach uses all starting runs. "
             "Simulator failures invalidate the report and are never counted as deaths. "
             "Post-Yendor survival uses acquisitions as its denominator.", "",
             "Control reserves count Confusion, Paralysis, Slowing, Strength and Invisibility potions, "
             "plus Fear, Teleport and Mass Confuse scrolls. Usable wand charges exclude cursed/unreliable "
             "wands and Digging; offensive = Fire/Striking/Ice, emergency = Force/Teleport/Polymorph. "
             "Heavy weapon occupancy means Two-handed sword; top armor means Plate mail. "
             "Lower effective speed budgets mean faster player turns.", "",
             "Floor 0's share of all deaths is flagged for review at 50%; starvation deaths are listed. "
             "No numerical bands were specified for these guardrails, resources, equipment, or ascent. "
             "Item activity and monster burden are descriptive, not causal power rankings. "
             "Hungry-turn counts, level-up healing, Griffin-attributed healing and enchantment allocation "
             "are not measured by this telemetry and are not reported as zero.", "",
             "CSV exports include entry means and medians, per-floor action/turn/damage/consumable statistics, "
             "all typed item/monster counters, and death causes. Raw streams and manifest remain in the parent directory.", "",
             f"Build revision: {manifest['git_sha']}; dirty: {manifest['git_dirty']}. "
             "summary.json records current workspace source hashes, the executable hash when available, "
             "and all nine CSV hashes. Workspace hashes alone do not establish the source of an externally supplied executable.", ""]
    report = "\n".join(text)
    (out/"summary.md").write_text(report,encoding="utf-8")
    (out/"summary.txt").write_text(report,encoding="utf-8")
    print(report,flush=True)
    print(f"\nReport saved: {out/'summary.md'}",flush=True)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir",type=Path,default=ROOT/"build/balance-check-native",help="native CMake directory; rebuilt by default")
    parser.add_argument("--simulator",type=Path,help="use an existing executable instead of building (caller ensures it is current)")
    parser.add_argument("--jobs",type=int,default=8,help="isolated simulator workers, 1..64 (default: 8)")
    parser.add_argument("--output",type=Path,help="fresh output directory (default: timestamp under build/balance-check-runs)")
    parser.add_argument("--summarize-only",type=Path,metavar="RUN",help="regenerate reports from a completed 10k run with entry-state telemetry")
    parser.add_argument("--baseline",type=Path,help="previous 10k run for paired comparison")
    parser.add_argument("--strict-bands",action="store_true",help="exit 2 if any descent floor is outside its band; default reports without failing")
    args = parser.parse_args()
    if not 1<=args.jobs<=64:
        parser.error("--jobs must be in 1..64")
    if args.summarize_only and (args.output or args.simulator):
        parser.error("--summarize-only cannot be combined with --output or --simulator")
    try:
        if args.summarize_only:
            directory=args.summarize_only.resolve()
            executable=None
        else:
            stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
            directory=(args.output or ROOT/"build/balance-check-runs"/stamp).resolve()
            if directory.exists() and any(directory.iterdir()):
                raise ValueError(f"Output directory is not empty: {directory}; choose a fresh directory")
            executable=args.simulator.resolve() if args.simulator else build_simulator(args.build_dir.resolve(),args.jobs)
            if not executable.is_file():
                raise ValueError(f"Simulator not found: {executable}")
            print(f"Running seeds 1..10000 with {args.jobs} workers. Raw telemetry: {directory}",flush=True)
            subprocess.run([str(executable),"--seeds","1:10000","--jobs",str(args.jobs),
                            "--output",str(directory),"--entry-state"],check=True)
        print("Validating telemetry and preparing the scorecard...",flush=True)
        summary=make_report(directory,executable,baseline=args.baseline)
        return 2 if args.strict_bands and summary["outside_bands"] else 0
    except (ValueError,OSError,subprocess.CalledProcessError) as error:
        print(f"Balance check failed: {error}",file=sys.stderr)
        return 1


if __name__=="__main__":
    raise SystemExit(main())
