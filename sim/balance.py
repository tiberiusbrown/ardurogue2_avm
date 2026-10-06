#!/usr/bin/env python3
"""Reproducible visit-factor analysis and effective-seed paired experiments.

All stochastic analysis uses a separate fixed NumPy PRNG. Gameplay never imports
this module. Requires sim/requirements-balance.txt only for host analysis.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import warnings

# Avoid oversubscribing BLAS for many small, separate adjusted models.
for _name in ("OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS", "OMP_NUM_THREADS"):
    os.environ.setdefault(_name, "1")
import numpy as np
import pandas as pd
from scipy import linalg, special, stats
import statsmodels.api as sm
from statsmodels.stats.multitest import multipletests

SCHEMA = 3
STREAMS = ("runs", "floors", "items", "monsters", "visit_items", "visit_monsters", "interventions", "ranged")
NOTICE = ("Adjusted observational association is not proof of causation. "
          "Controlled A/B interventions are preferred for causal balance conclusions.")
BASE_NUMERIC = ("entry_hp_fraction", "entry_level", "entry_strength", "entry_dexterity",
                "entry_speed", "entry_hunger", "entry_armor_rating", "entry_armor_enchant",
                "entry_food_units", "entry_healing_units", "floor_tiles", "major_features",
                "corridors", "loops", "open_connections")
KEY = ["effective_seed", "visit"]


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def specification_hash(spec):
    return hashlib.sha256(json.dumps(spec, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")


def read_manifest(directory):
    path = Path(directory) / "manifest.json"
    if not path.exists():
        raise ValueError(f"missing manifest: {path}; legacy datasets must be rerun")
    m = json.loads(path.read_text(encoding="utf-8"))
    required = ("schema_version", "telemetry_schema_version", "agent", "agent_policy_hash", "effective_seed_count",
                "seed_selection", "experiment", "variant", "options")
    if any(k not in m for k in required) or m["schema_version"] != 1:
        raise ValueError(f"unsupported/incomplete manifest: {path}")
    if m["telemetry_schema_version"] != SCHEMA:
        raise ValueError(f"incompatible telemetry schema: {path}")
    for name, digest in m.get("output_csv_sha256", {}).items():
        if name not in {s + ".csv" for s in STREAMS} or sha256(Path(directory) / name) != digest:
            raise ValueError(f"output hash mismatch: {name}")
    return m


def seal_manifest(directory, executable=None, spec=None):
    """Attach hashes without including paths or wall time in result identity."""
    directory = Path(directory)
    m = read_manifest(directory)
    if any(not (directory / (s + ".csv")).is_file() for s in STREAMS):
        raise ValueError("incomplete telemetry stream set")
    if executable:
        m["simulator_executable_path"] = str(Path(executable).resolve())
        m["simulator_executable_sha256"] = sha256(executable)
    m["output_csv_sha256"] = {s + ".csv": sha256(directory / (s + ".csv")) for s in STREAMS}
    if spec is not None:
        m["experiment_specification"] = spec
        m["experiment_specification_sha256"] = specification_hash(spec)
    # Paths, timestamps, command spelling and job count are provenance, not identity.
    identity = {k: m[k] for k in ("telemetry_schema_version", "agent",
                                "effective_seed_count", "experiment", "variant", "interventions",
                                "output_csv_sha256")}
    identity["max_actions"] = m["options"]["max_actions"]
    selection = m["seed_selection"]
    identity["effective_seeds"] = sorted(s if s else 0xACE1 for s in range(selection["first"], selection["last"] + 1))
    if "simulator_executable_sha256" in m:
        identity["executable_sha256"] = m["simulator_executable_sha256"]
    m["deterministic_result_sha256"] = specification_hash(identity)
    m["analysis_dependencies"] = {"numpy": np.__version__, "pandas": pd.__version__,
                                  "scipy": __import__("scipy").__version__,
                                  "statsmodels": __import__("statsmodels").__version__}
    write_json(directory / "manifest.json", m)
    return m


def validate_runs(runs, manifest):
    if runs.empty or runs.effective_seed.duplicated().any():
        raise ValueError("empty data or duplicate effective seeds")
    seeds = runs.effective_seed.to_numpy()
    if not np.isfinite(seeds).all() or not np.equal(seeds, seeds.astype(int)).all() or np.any((seeds < 1) | (seeds > 65535)):
        raise ValueError("invalid effective seeds")
    if set(runs.agent) != {manifest["agent"]}:
        raise ValueError("agent disagrees with manifest")
    if not runs.result.isin(["escaped", "death"]).all() or runs.stuck.astype(bool).any():
        raise ValueError("simulator failures/unfinished runs invalidate experimental data")
    if len(runs) != manifest["effective_seed_count"]:
        raise ValueError("manifest seed count disagrees with runs")
    first, last = manifest["seed_selection"]["first"], manifest["seed_selection"]["last"]
    expected = {s if s else 0xACE1 for s in range(first, last + 1)}
    if set(seeds) != expected or len(expected) != last - first + 1:
        raise ValueError("manifest seed population disagrees with runs")
    if not manifest["options"].get("telemetry"):
        raise ValueError("full telemetry is required for balance analysis")


def load_directory(directory, visits=True):
    directory = Path(directory)
    m = read_manifest(directory)
    if any(not (directory / (s + ".csv")).is_file() for s in STREAMS):
        raise ValueError("incomplete telemetry stream set")
    # Paired reports only use whole-run content and floor outcomes. Keep census
    # comparisons practical by not materializing millions of unused typed visits.
    streams = STREAMS if visits else tuple(s for s in STREAMS if not s.startswith("visit_"))
    tables = {s: pd.read_csv(directory / (s + ".csv"), keep_default_na=False) for s in streams}
    validate_runs(tables["runs"], m)
    seeds = set(tables["runs"].effective_seed)
    floors = tables["floors"]
    if floors.empty or floors.duplicated(KEY).any() or set(floors.effective_seed) != seeds:
        raise ValueError("missing or duplicate floor visits")
    for s, t in tables.items():
        if s == "runs":
            continue
        if not set(t.effective_seed).issubset(seeds) or (len(t) and set(t.agent) != {m["agent"]}):
            raise ValueError(f"invalid run keys in {s}")
        type_key = "item_type" if "item_type" in t else "monster_type" if "monster_type" in t else None
        keys = KEY if "visit" in t else ["effective_seed"]
        if type_key and t.duplicated(keys + [type_key]).any():
            raise ValueError(f"duplicate content keys in {s}")
        if "visit" in t and len(t):
            joined = t.merge(floors[KEY + ["floor", "direction"]], on=KEY, how="left", suffixes=("", "_floor"), validate="many_to_one")
            if joined.floor_floor.isna().any() or not (joined.floor == joined.floor_floor).all() or not (joined.direction == joined.direction_floor).all():
                raise ValueError(f"invalid visit keys in {s}")
    return m, tables


def reconcile_telemetry(tables):
    """Sparse visit rows must sum to every event-based whole-run counter."""
    runs = tables["runs"].set_index("effective_seed")
    floors = tables["floors"].groupby("effective_seed")
    for field in ("actions", "turns"):
        if not np.array_equal(floors[field].sum().reindex(runs.index).to_numpy(), runs[field].to_numpy()):
            raise ValueError(f"floor/run {field} does not reconcile")
    if not np.array_equal(floors.size().reindex(runs.index), runs.floors_entered):
        raise ValueError("floor entry counts do not reconcile")
    if not np.array_equal(floors.exited.sum().reindex(runs.index), runs.floors_exited):
        raise ValueError("floor exit counts do not reconcile")
    for category, type_key, label in (("items", "item_type", "item"), ("monsters", "monster_type", "monster")):
        whole = tables[category].set_index(["effective_seed", type_key])
        fields = [c for c in whole if c not in ("seed", "agent", label, "carried")]
        totals = tables["visit_" + category].groupby(["effective_seed", type_key])[fields].sum().reindex(whole.index, fill_value=0)
        if not np.array_equal(whole[fields].to_numpy(), totals.to_numpy()):
            raise ValueError(f"visit/run {category} counters do not reconcile")
    if 'ranged' in tables:
        import ranged_report
        ranged_report.reconcile(tables)
    return True


def bh_qvalues(values):
    a = np.asarray(values, dtype=float)
    out = np.full(len(a), np.nan)
    finite = np.isfinite(a)
    if finite.any():
        out[finite] = multipletests(a[finite], method="fdr_bh")[1]
    return out


def prepare_visits(floors):
    f = floors.copy()
    if (f.entry_max_hp <= 0).any():
        raise ValueError("invalid entry maximum HP")
    f["entry_hp_fraction"] = f.entry_hp / f.entry_max_hp
    f["stratum"] = f.floor.astype(str) + ":" + f.direction
    return f


def design_matrix(f, exclude=None):
    columns = ["intercept"]
    matrices = [np.ones((len(f), 1))]
    for name in BASE_NUMERIC:
        if name == exclude:
            continue
        x = f[name].to_numpy(dtype=float)
        if np.std(x) > 1e-10:
            matrices.append(((x - x.mean()) / x.std())[:, None])
            columns.append(name)
    for name in ("stratum", "archetype"):
        if name == exclude:
            continue
        d = pd.get_dummies(f[name], prefix=name, drop_first=True, dtype=float)
        matrices.append(d.to_numpy())
        columns.extend(d.columns)
    x = np.concatenate(matrices, axis=1)
    # Rank-revealing QR removes redundant controls, retaining their column space.
    _, r, piv = linalg.qr(x, mode="economic", pivoting=True, check_finite=False)
    rank = np.sum(np.abs(np.diag(r)) > 1e-8)
    keep = sorted(piv[:rank])
    return x[:, keep], [columns[i] for i in keep]


def fit_factor(f, exposure, name, category, min_exposure=100, exclude=None, binary=True):
    """Separate binomial GLM; sandwich covariance clustered by effective seed."""
    f = f.copy()
    f["factor_value"] = np.asarray(exposure, dtype=float)
    eligible = len(f)
    exposed = int((f.factor_value > 0).sum()) if binary else eligible
    record = dict(factor=name, category=category, eligible_visits=eligible, exposed_visits=exposed,
                  status="insufficient_data", effect_direction="unknown", clusters=f.effective_seed.nunique(),
                  coefficient=np.nan, coefficient_low=np.nan, coefficient_high=np.nan,
                  odds_ratio=np.nan, or_low=np.nan, or_high=np.nan, adjusted_difference_pp=np.nan,
                  difference_low_pp=np.nan, difference_high_pp=np.nan, p_value=np.nan,
                  modeled_visits=0, modeled_exposed_visits=0, note="", unit="presence" if binary else "one SD")
    if eligible < 2 * min_exposure or (binary and min(exposed, eligible - exposed) < min_exposure) or f.exited.nunique() < 2:
        record["note"] = "minimum exposed/unexposed visits or outcome variation not met"
        return record
    # Structural exposure absence outside the content's support is not a comparator.
    groups = f.groupby("stratum").agg(variation=("factor_value", "nunique"), outcomes=("exited", "nunique"))
    support = groups.index[(groups.variation > 1) & (groups.outcomes > 1)]
    f = f[f.stratum.isin(support)].copy()
    record["modeled_visits"] = len(f)
    record["modeled_exposed_visits"] = int((f.factor_value > 0).sum()) if binary else len(f)
    record["clusters"] = f.effective_seed.nunique()
    if len(f) < 2 * min_exposure or f.effective_seed.nunique() < 30:
        record["note"] = "insufficient within-stratum support or seed clusters"
        return record
    z = f.factor_value.to_numpy()
    if binary and min(np.count_nonzero(z), np.count_nonzero(z == 0)) < min_exposure:
        record["note"] = "insufficient within-stratum exposed/unexposed visits"
        return record
    if not binary:
        z = (z - z.mean()) / z.std()
    # Require at least five exits and failures in each exposure arm.
    if binary and min(pd.crosstab(z, f.exited).reindex(index=[0, 1], columns=[0, 1], fill_value=0).to_numpy().ravel()) < 5:
        record["status"] = "unstable_model"
        record["note"] = "sparse outcome/exposure cell; possible separation"
        return record
    base, controls = design_matrix(f, exclude)
    residual = z - base @ linalg.lstsq(base, z, check_finite=False)[0]
    if np.linalg.norm(residual) < 1e-7:
        record["note"] = "factor is fully confounded with controls"
        return record
    x = np.column_stack([base, z])
    try:
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")
            model = sm.GLM(f.exited.to_numpy(dtype=float), x, family=sm.families.Binomial())
            fit = model.fit(maxiter=80, tol=1e-8, cov_type="cluster",
                            cov_kwds={"groups": f.effective_seed.to_numpy(), "use_correction": True})
        beta = float(fit.params[-1])
        low, high = map(float, fit.conf_int()[-1])
        if not fit.converged or not np.isfinite([beta, low, high, fit.pvalues[-1]]).all() or abs(beta) > 10 or high - low > 20 or any("separation" in str(w.message).lower() for w in caught):
            raise ValueError("separation, nonconvergence or unstable confidence interval")
        x0, x1 = x.copy(), x.copy()
        x0[:, -1] = 0
        x1[:, -1] = 1  # binary counterfactuals, or mean to mean + 1 SD
        p0, p1 = special.expit(x0 @ fit.params), special.expit(x1 @ fit.params)
        difference = float((p1 - p0).mean())
        gradient = ((p1 * (1 - p1)) @ x1 - (p0 * (1 - p0)) @ x0) / len(f)
        se = float(np.sqrt(max(0, gradient @ fit.cov_params() @ gradient)))
        record.update(status="ok", effect_direction="positive" if beta > 0 else "negative",
                      coefficient=beta, coefficient_low=low, coefficient_high=high,
                      odds_ratio=float(np.exp(beta)), or_low=float(np.exp(low)), or_high=float(np.exp(high)),
                      adjusted_difference_pp=100 * difference, difference_low_pp=100 * (difference - 1.96 * se),
                      difference_high_pp=100 * (difference + 1.96 * se), p_value=float(fit.pvalues[-1]),
                      note="cluster-robust by effective seed; supported strata only; controls=" + ";".join(controls))
    except (ValueError, np.linalg.LinAlgError) as error:
        record.update(status="unstable_model", note=str(error))
    return record


def monster_burden(monsters):
    fields = ["generated", "encountered", "engaged", "damage_to_player", "monster_attacks", "monster_hits",
              "deaths_caused", "poison", "confusion", "paralysis", "fire"]
    b = monsters.groupby("monster")[fields].sum()
    den = b.engaged.replace(0, np.nan)
    b["damage_per_engagement"] = b.damage_to_player / den
    b["hits_per_attack"] = b.monster_hits / b.monster_attacks.replace(0, np.nan)
    b["deaths_per_engagement"] = b.deaths_caused / den
    b["statuses_per_engagement"] = b[["poison", "confusion", "paralysis", "fire"]].sum(axis=1) / den
    return b.reset_index().sort_values("damage_to_player", ascending=False)


def selection_associations(tables):
    rows = []
    win = tables["runs"].set_index("effective_seed").result.eq("escaped").astype(float)
    for item, group in tables["items"].groupby("item"):
        group = group.set_index("effective_seed").reindex(win.index, fill_value=0)
        for field in ("generated", "picked_up", "used", "equipped", "turns_equipped"):
            x = group[field].to_numpy()
            correlation, p = stats.spearmanr(x, win) if np.std(x) > 0 and win.nunique() > 1 else (np.nan, np.nan)
            rows.append(dict(item=item, activity=field, spearman_r=correlation, p_value=p,
                             exposed_runs=int((x > 0).sum()), interpretation="selection/survivorship biased; not causal"))
    out = pd.DataFrame(rows)
    out["q_value"] = bh_qvalues(out.p_value)
    return out


def markdown_table(frame, columns, limit=12):
    if frame.empty:
        return "No estimable factors in this section.\n"
    def fmt(value):
        if isinstance(value, (list, tuple, np.ndarray)):
            return str(value)
        if pd.isna(value):
            return "—"
        if isinstance(value, (float, np.floating)):
            return f"{value:.4g}"
        return str(value).replace("|", "/").replace("\n", " ")
    lines = ["| " + " | ".join(columns) + " |", "| " + " | ".join("---" for _ in columns) + " |"]
    lines += ["| " + " | ".join(fmt(row[c]) for c in columns) + " |" for _, row in frame.head(limit).iterrows()]
    return "\n".join(lines) + "\n"


def factors(directory, output=None, min_exposure=100, q_threshold=.05):
    m, tables = load_directory(directory)
    output = Path(output or Path(directory) / "factors")
    output.mkdir(parents=True, exist_ok=True)
    f = prepare_visits(tables["floors"])
    rows = []
    for stream, category, label in (("visit_items", "item", "item"), ("visit_monsters", "monster", "monster")):
        visits = tables[stream]
        for name in sorted(tables["items" if category == "item" else "monsters"][label].unique()):
            eligible = f[f.direction == "descent"] if category == "item" else f
            generated = visits[visits[label] == name].set_index(KEY).generated
            exposure = generated.reindex(pd.MultiIndex.from_frame(eligible[KEY]), fill_value=0).to_numpy() > 0
            if name == "YENDOR_AMULET":
                record = fit_factor(eligible, np.zeros(len(eligible)), name, category, min_exposure)
                record["note"] = "Lord drop is outcome-selected generation; excluded from availability inference"
            else:
                record = fit_factor(eligible, exposure, name, category, min_exposure)
            rows.append(record)
            print(f"{category} {name}: {record['status']}", flush=True)
    for name in ("floor_tiles", "major_features", "corridors", "loops", "open_connections"):
        rows.append(fit_factor(f, f[name], name, "floor", min_exposure, exclude=name, binary=False))
    archetypes = sorted(f.archetype.unique())
    for name in archetypes[1:]:
        subset = f[f.archetype.isin([archetypes[0], name])]
        rows.append(fit_factor(subset, subset.archetype.eq(name), f"archetype:{name} vs {archetypes[0]}",
                               "floor", min_exposure, exclude="archetype"))
    results = pd.DataFrame(rows)
    results["q_value"] = bh_qvalues(results.p_value)
    results["statistically_notable"] = results.q_value <= q_threshold
    results["magnitude_pp"] = results.adjusted_difference_pp.abs()
    results = results.sort_values("magnitude_pp", ascending=False, na_position="last")
    results.to_csv(output / "factors.csv", index=False)
    burden = monster_burden(tables["monsters"])
    burden.to_csv(output / "monster_burden.csv", index=False)
    selected = selection_associations(tables)
    selected.to_csv(output / "selection_associations.csv", index=False)
    cols = ["factor", "eligible_visits", "modeled_visits", "modeled_exposed_visits", "adjusted_difference_pp",
            "difference_low_pp", "difference_high_pp", "odds_ratio", "or_low", "or_high", "p_value", "q_value"]
    text = ["# Exploratory visit survival factors", "", "**" + NOTICE + "**", "",
            f"Agent: {m['agent']}; {len(tables['runs']):,} unique effective seeds; {len(f):,} entered visits.", "",
            "Separate binomial logistic models: visit_exited ~ floor/direction categorical stratum + "
            "entry HP fraction, level, effective strength/dexterity/speed, hunger, armor rating/enchantment, "
            "food/healing units + archetype + floor tiles, major features, corridors, loops, open connections + factor. "
            "Rank-redundant controls are removed. Each analyzed floor covariate is omitted from its own adjustment set.", "",
            "Item presence means generated > 0 on eligible descent visits; Yendor drops are excluded. "
            "Monster presence uses entered visits with within-stratum exposure variation. "
            "All-success/all-failure strata provide no estimable finite intercept and are omitted. "
            "Eligible and actually modeled counts are both reported. Sparse outcome cells, separation and nonconvergence are flagged.", "",
            f"95% Wald intervals use sandwich covariance clustered by effective seed with finite-cluster correction. "
            f"BH FDR spans all successfully estimated item, monster and floor hypotheses (q <= {q_threshold:g}). "
            "Probability differences average model predictions toggling binary exposure in the modeled population; "
            "floor counts compare their mean to mean + 1 SD. Their intervals use the same clustered covariance and delta method.", "",
            "Ranking is by absolute adjusted probability difference, with exposure counts and uncertainty alongside. "
            "Statistical significance does not establish practical significance. Small effects remain small.", ""]
    ok = results[results.status == "ok"]
    for title, subset in (
        ("Strongest positive item availability associations", ok[(ok.category == "item") & (ok.adjusted_difference_pp > 0)]),
        ("Strongest negative item availability associations", ok[(ok.category == "item") & (ok.adjusted_difference_pp < 0)]),
        ("Strongest harmful monster associations", ok[(ok.category == "monster") & (ok.adjusted_difference_pp < 0)]),
        ("Floor/generation factors", ok[ok.category == "floor"])):
        text += ["## " + title, "", markdown_table(subset, cols), ""]
    text += ["## Monster direct burden", "", "Damage and deaths are totals; rates use engagements/attacks. "
             "These are behavior-selected descriptive burdens, not adjusted causal estimates.", "",
             markdown_table(burden, ["monster", "encountered", "engaged", "damage_to_player", "damage_per_engagement",
                                     "hits_per_attack", "deaths_caused", "deaths_per_engagement", "statuses_per_engagement"], 16), "",
             "## Selection-biased pickup/use associations", "", "Whole-run Spearman associations with escape. "
             "Long-lived runs generate/collect more content; struggling players consume healing. "
             "Pickup/use/equip direction must not be interpreted as content benefit or harm. "
             "These descriptive q-values form a separate BH family.", "",
             markdown_table(selected.assign(magnitude=selected.spearman_r.abs()).sort_values("magnitude", ascending=False),
                            ["item", "activity", "exposed_runs", "spearman_r", "p_value", "q_value"], 20), "",
             "## Insufficient-data factors", "",
             markdown_table(results[results.status != "ok"], ["factor", "category", "exposed_visits", "status", "note"], len(results)), "",
             "## Limits", "", "Residual confounding, entry-survivor selection, correlated availability and policy-specific decisions remain. "
             "These models measure conditional visit survival under the recorded policy hash, not human win rates. "
             "Presence ignores dose, curse, enchantment, positions and accessibility; geometry measures can be correlated. "
             "Fixed content slots induce composition confounding: presence of one type can displace another. "
             "A positive monster coefficient is not evidence that adding that monster helps. "
             "No generated factor report alone justifies a production balance change.", ""]
    (output / "factors.md").write_text("\n".join(text), encoding="utf-8")
    write_json(output / "analysis.json", {"source_manifest_sha256": sha256(Path(directory) / "manifest.json"),
               "min_exposure": min_exposure, "q_threshold": q_threshold, "cluster_key": "effective_seed",
               "fdr_family": "all estimable adjusted factors", "telemetry_schema_version": SCHEMA})
    return results


def is_census(seeds):
    a = np.asarray(seeds)
    return len(a) == 65535 and np.array_equal(np.sort(a), np.arange(1, 65536))


def paired_bootstrap(difference, repetitions=2000, seed=20261005, statistic="mean"):
    d = np.asarray(difference, dtype=float)
    if not len(d) or repetitions < 100:
        raise ValueError("bootstrap requires observations and at least 100 repetitions")
    if np.all(d == d[0]):
        return [float(d[0]), float(d[0])]
    rng = np.random.default_rng(seed)
    values = np.empty(repetitions)
    # Bounded memory even for a full census; resample paired seed differences.
    for first in range(0, repetitions, 32):
        indices = rng.integers(0, len(d), size=(min(32, repetitions - first), len(d)))
        sample = d[indices]
        values[first:first + len(sample)] = np.mean(sample, axis=1) if statistic == "mean" else np.median(sample, axis=1)
    return np.quantile(values, [.025, .975]).tolist()


def paired_binary(baseline, candidate, census=False, repetitions=2000):
    b, c = np.asarray(baseline, dtype=bool), np.asarray(candidate, dtype=bool)
    if b.shape != c.shape or not len(b):
        raise ValueError("invalid binary pairs")
    losses, gains = int(np.sum(b & ~c)), int(np.sum(~b & c))
    difference = c.astype(float) - b.astype(float)
    baseline_rate, candidate_rate = float(b.mean()), float(c.mean())
    return dict(baseline_rate=baseline_rate, candidate_rate=candidate_rate,
                delta_pp=100 * float(difference.mean()),
                relative_delta=(candidate_rate / baseline_rate - 1) if baseline_rate else None,
                baseline_win_candidate_loss=losses, baseline_loss_candidate_win=gains,
                both_escaped=int(np.sum(b & c)), both_died=int(np.sum(~b & ~c)),
                p_value=None if census else float(stats.binomtest(gains, gains + losses, .5).pvalue) if gains + losses else 1.0,
                ci_pp=None if census else [100 * x for x in paired_bootstrap(difference, repetitions)],
                inference="exact deterministic seed-population effect; sampling inference unnecessary" if census else
                "two-sided exact McNemar/binomial test; deterministic paired percentile bootstrap 95% CI")


def pair_runs(bm, bt, cm, ct):
    validate_runs(bt, bm)
    validate_runs(ct, cm)
    if not bm.get("agent_policy_hash") or bm["agent_policy_hash"] != cm.get("agent_policy_hash"):
        raise ValueError("agent policy hashes differ or are missing")
    if bm["agent"] != cm["agent"]:
        raise ValueError("agent versions differ")
    if bm["telemetry_schema_version"] != cm["telemetry_schema_version"]:
        raise ValueError("telemetry schemas differ")
    if bm["options"]["max_actions"] != cm["options"]["max_actions"]:
        raise ValueError("simulator action limits differ")
    if set(bt.effective_seed) != set(ct.effective_seed):
        raise ValueError("effective seed sets differ")
    return bt.sort_values("effective_seed").set_index("effective_seed"), ct.sort_values("effective_seed").set_index("effective_seed")


def floor_survival(tables):
    f = tables["floors"]
    n = len(tables["runs"])
    rows = []
    for direction in ("descent", "ascent"):
        for floor in range(16):
            visits = f[(f.direction == direction) & (f.floor == floor)]
            entered = visits.effective_seed.nunique()
            exited = visits[visits.exited.astype(bool)].effective_seed.nunique()
            rows.append(dict(floor=floor, direction=direction, entered_runs=entered, exited_runs=exited,
                             unconditional_reach=entered / n, unconditional_exit=exited / n,
                             conditional_visit_exit=float(visits.exited.mean()) if len(visits) else np.nan,
                             entered_visits=len(visits)))
    return pd.DataFrame(rows)


def run_outcomes(tables):
    r = tables["runs"].set_index("effective_seed")
    out = pd.DataFrame(index=r.index)
    out["escaped"] = r.result.eq("escaped")
    out["reached_floor_12"] = r.deepest_floor >= 12
    out["reached_floor_15"] = r.deepest_floor >= 15
    killed = tables["monsters"][tables["monsters"].monster == "LORD"].set_index("effective_seed").killed
    out["lord_killed"] = killed.reindex(r.index, fill_value=0) > 0
    out["yendor_acquired"] = r.has_yendor.astype(bool)
    out["post_yendor_death"] = r.has_yendor.astype(bool) & r.result.eq("death")
    for name in ("deepest_floor", "score", "actions", "turns"):
        out[name] = r[name]
    for name in ("damage_taken", "damage_dealt", "consumables_used"):
        out[name] = tables["floors"].groupby("effective_seed")[name].sum().reindex(r.index, fill_value=0)
    return out.sort_index()


def content_shifts(bt, ct, stream, label):
    fields = ("generated", "picked_up", "used") if stream == "items" else (
        "generated", "encountered", "engaged", "damage_to_player", "deaths_caused")
    b = bt[stream].groupby(label)[list(fields)].sum() / len(bt["runs"])
    c = ct[stream].groupby(label)[list(fields)].sum() / len(ct["runs"])
    rows = []
    for name in b.index.union(c.index):
        for field in fields:
            bv = float(b.loc[name, field]) if name in b.index else 0.0
            cv = float(c.loc[name, field]) if name in c.index else 0.0
            rows.append(dict(content=name, metric=field, baseline_per_run=bv, candidate_per_run=cv, delta_per_run=cv - bv))
    if stream == "monsters":
        b, c = monster_burden(bt[stream]).set_index(label), monster_burden(ct[stream]).set_index(label)
        for name in b.index.union(c.index):
            for field in ("damage_per_engagement", "deaths_per_engagement"):
                bv, cv = (float(t.loc[name, field]) if name in t.index else np.nan for t in (b, c))
                rows.append(dict(content=name, metric=field, baseline_per_run=bv, candidate_per_run=cv, delta_per_run=cv - bv))
    return pd.DataFrame(rows).assign(magnitude=lambda f: f.delta_per_run.abs()).sort_values("magnitude", ascending=False)


def compare(baseline, candidate, output=None, repetitions=2000):
    bm, bt = load_directory(baseline, visits=False)
    cm, ct = load_directory(candidate, visits=False)
    br, cr = pair_runs(bm, bt["runs"], cm, ct["runs"])
    output = Path(output or Path(candidate) / "comparison")
    output.mkdir(parents=True, exist_ok=True)
    census = is_census(br.index)
    bo, co = run_outcomes(bt), run_outcomes(ct)
    primary = paired_binary(bo.escaped, co.escaped, census, repetitions)
    secondary = []
    for name in bo.columns[1:]:
        b, c = bo[name].to_numpy(), co[name].to_numpy()
        if bo[name].dtype == bool:
            row = paired_binary(b, c, census, repetitions)
            row.update(metric=name)
            secondary.append(row)
        else:
            d = c.astype(float) - b.astype(float)
            secondary.append(dict(metric=name, baseline_mean=float(b.mean()), candidate_mean=float(c.mean()),
                                  baseline_median=float(np.median(b)), candidate_median=float(np.median(c)),
                                  paired_mean_difference=float(d.mean()), paired_median_difference=float(np.median(d)),
                                  mean_ci=None if census else paired_bootstrap(d, repetitions),
                                  median_ci=None if census else paired_bootstrap(d, repetitions, statistic="median")))
    pd.DataFrame(secondary).to_csv(output / "paired_outcomes.csv", index=False)
    curves = floor_survival(bt).merge(floor_survival(ct), on=["floor", "direction"], suffixes=("_baseline", "_candidate"))
    curves.to_csv(output / "floor_survival.csv", index=False)
    deaths = pd.concat([r[r.result == "death"].death_cause.value_counts().rename(tag)
                        for r, tag in ((br, "baseline"), (cr, "candidate"))], axis=1).fillna(0)
    deaths["delta_per_1000_runs"] = 1000 * (deaths.candidate - deaths.baseline) / len(br)
    deaths = deaths.rename_axis("death_cause").reset_index().assign(magnitude=lambda t: t.delta_per_1000_runs.abs()).sort_values("magnitude", ascending=False)
    deaths.to_csv(output / "death_causes.csv", index=False)
    shifts = {}
    for stream, label in (("items", "item"), ("monsters", "monster")):
        shifts[stream] = content_shifts(bt, ct, stream, label)
        shifts[stream].to_csv(output / (stream + "_shifts.csv"), index=False)
    discordant = pd.DataFrame(index=br.index)
    for prefix, r in (("baseline", br), ("candidate", cr)):
        for field, target in (("result", "result"), ("deepest_floor", "deepest"), ("death_cause", "death_cause")):
            discordant[prefix + "_" + target] = r[field]
    discordant = discordant[bo.escaped != co.escaped].copy()
    discordant["classification"] = np.where(discordant.baseline_result == "escaped", "baseline_win_candidate_loss", "baseline_loss_candidate_win")
    discordant.reset_index().to_csv(output / "discordant_seeds.csv", index=False)
    result = dict(effective_seed_count=len(br), census=bool(census), agent=bm["agent"], agent_policy_hash=bm["agent_policy_hash"], primary=primary,
                  secondary=secondary, bootstrap_seed=20261005, bootstrap_repetitions=repetitions,
                  baseline_manifest_sha256=sha256(Path(baseline) / "manifest.json"),
                  candidate_manifest_sha256=sha256(Path(candidate) / "manifest.json"))
    write_json(output / "comparison.json", result)
    text = ["# Paired balance comparison", "", f"{len(br):,} identical unique effective seeds; agent {bm['agent']}. "
            "Pairs are joined by effective_seed, never row order. Failures and incompatible data are rejected.", "",
            "**" + ("Census of all 65,535 deterministic nonzero effective seeds: exact population effects. "
                       "Sampling confidence intervals and p-values are unnecessary and omitted." if census else
                       "Sampled seed population: effects take priority over statistical significance.") + "**", "",
            f"Escape: baseline **{100 * primary['baseline_rate']:.3f}%**, candidate **{100 * primary['candidate_rate']:.3f}%**; "
            f"absolute delta **{primary['delta_pp']:+.3f} percentage points**; "
            f"relative delta {primary['relative_delta'] if primary['relative_delta'] is not None else 'undefined (zero baseline)' }.", "",
            f"Baseline escape / candidate death: {primary['baseline_win_candidate_loss']}; "
            f"baseline death / candidate escape: {primary['baseline_loss_candidate_win']}; "
            f"both escaped: {primary['both_escaped']}; both died: {primary['both_died']}.", "",
            f"95% paired difference CI (pp): {primary['ci_pp']}; exact two-sided McNemar/binomial p: {primary['p_value']}.", "",
            "For samples, fixed-PRNG percentile bootstrap resamples seed pairs (2,000 draws by default), "
            "preserving within-pair dependence. Finite sampled ranges are not necessarily random draws from all seeds; "
            "sampling inference assumes representativeness. Secondary outcomes are descriptive and not multiplicity-adjusted.", "",
            "## Secondary paired outcomes", "",
            markdown_table(pd.DataFrame([r for r in secondary if "baseline_rate" in r]), ["metric", "baseline_rate", "candidate_rate", "delta_pp", "ci_pp", "p_value"], len(secondary)), "",
            markdown_table(pd.DataFrame([r for r in secondary if "baseline_mean" in r]), ["metric", "baseline_mean", "candidate_mean", "baseline_median", "candidate_median",
                                                    "paired_mean_difference", "paired_median_difference", "mean_ci", "median_ci"], len(secondary)), "",
            "## Floor survival", "", "floor_survival.csv separates unconditional reach/exit among all seeds from conditional "
            "exit among entered visits. Ascent has a different entry population. These quantities must not be conflated.", "",
            "## Death cause shifts", "", "Immediate cause is not the full causal chain. Largest absolute shifts per 1,000 runs:", "",
            markdown_table(deaths, ["death_cause", "baseline", "candidate", "delta_per_1000_runs"], 20), ""]
    for stream in shifts:
        text += ["## " + stream.title() + " shifts", "", "Totals are normalized per run; rows named per_engagement use that denominator.", "",
                 markdown_table(shifts[stream], ["content", "metric", "baseline_per_run", "candidate_per_run", "delta_per_run"], 20), ""]
    text += ["## Representative discordant seeds", ""]
    for classification in ("baseline_win_candidate_loss", "baseline_loss_candidate_win"):
        text += [classification + ": " + ", ".join(map(str, discordant[discordant.classification == classification].index[:10])) + ".", ""]
    text += ["Trace these pairs with their original binary, experiment and variant before retuning constants. "
             "Matched seeds supply initial conditions. Actions/mechanics may subsequently diverge and consume different "
             "gameplay random draws; do not resynchronize RNG. Controlled interventions can support causal conclusions "
             "for the specified substitution/removal under this maintained policy. Ordinary two-build comparisons include every build difference.", ""]
    (output / "compare.md").write_text("\n".join(text), encoding="utf-8")
    return result


def summarize(directory, output=None, executable=None):
    m, t = load_directory(directory)
    reconcile_telemetry(t)
    if executable or "output_csv_sha256" not in m:
        m = seal_manifest(directory, executable)
    output = Path(output or Path(directory) / "summary")
    output.mkdir(parents=True, exist_ok=True)
    o = run_outcomes(t)
    result = {"agent": m["agent"], "runs": len(o), "census": bool(is_census(o.index)),
              "escape_rate": float(o.escaped.mean()), "escaped": int(o.escaped.sum()),
              "simulator_failures": 0, "visit_counters_reconciled": True,
              "means": {c: float(o[c].mean()) for c in o.columns}}
    write_json(output / "summary.json", result)
    floor_survival(t).to_csv(output / "floor_survival.csv", index=False)
    (output / "summary.md").write_text(f"# Maintained-policy balance summary\n\n{len(o):,} runs; {int(o.escaped.sum()):,} escaped "
                                      f"({100 * o.escaped.mean():.2f}%); no simulator failures.\n\n{NOTICE}\n", encoding="utf-8")
    return result


def ab(args):
    if not 1 <= args.jobs <= 64:
        raise ValueError("jobs must be in 1..64")
    output = Path(args.output)
    if any((output / name / "runs.csv").exists() for name in ("control", "treatment")):
        raise ValueError("experiment output already exists; use a fresh directory")
    seeds = ["--all-seeds"] if args.all_seeds else ["--seeds", args.seeds]
    spec = {"experiment": args.experiment, "seed_selection": "all-seeds" if args.all_seeds else args.seeds,
            "agent": "omniscient-v2", "max_actions": args.max_actions,
            "control_interventions": args.control_intervention, "treatment_interventions": args.treatment_intervention}
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "experiment.json", spec)
    for variant, exe, rules in (("control", args.control_exe, args.control_intervention),
                                ("treatment", args.treatment_exe, args.treatment_intervention)):
        executable = Path(exe).resolve()
        directory = output / variant
        command = [str(executable), *seeds, "--jobs", str(args.jobs), "--output", str(directory),
                   "--max-actions", str(args.max_actions), "--experiment", args.experiment, "--variant", variant]
        for rule in rules:
            command.extend(["--intervention", rule])
        subprocess.run(command, check=True)  # argv arrays support Windows paths with spaces
        seal_manifest(directory, executable, spec)
    return compare(output / "control", output / "treatment", output / "comparison", args.bootstrap)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    s = sub.add_parser("summarize")
    s.add_argument("directory", type=Path)
    s.add_argument("--simulator", type=Path)
    f = sub.add_parser("factors")
    f.add_argument("directory", type=Path)
    f.add_argument("--min-exposure", type=int, default=100)
    f.add_argument("--q-threshold", type=float, default=.05)
    c = sub.add_parser("compare")
    c.add_argument("baseline", type=Path)
    c.add_argument("candidate", type=Path)
    c.add_argument("--bootstrap", type=int, default=2000)
    a = sub.add_parser("ab")
    a.add_argument("--control-exe", type=Path, required=True)
    a.add_argument("--treatment-exe", type=Path, required=True)
    selection = a.add_mutually_exclusive_group(required=True)
    selection.add_argument("--seeds")
    selection.add_argument("--all-seeds", action="store_true")
    a.add_argument("--jobs", type=int, default=1)
    a.add_argument("--max-actions", type=int, default=20000)
    a.add_argument("--experiment", default="paired-ab")
    a.add_argument("--control-intervention", action="append", default=[])
    a.add_argument("--treatment-intervention", action="append", default=[])
    a.add_argument("--bootstrap", type=int, default=2000)
    for p in (s, f, c, a):
        p.add_argument("--output", type=Path, required=p is a)
    args = parser.parse_args()
    try:
        if args.command == "summarize":
            summarize(args.directory, args.output, args.simulator)
        elif args.command == "factors":
            if args.min_exposure < 10 or not 0 < args.q_threshold < 1:
                raise ValueError("minimum exposure must be >=10 and q threshold in (0,1)")
            factors(args.directory, args.output, args.min_exposure, args.q_threshold)
        elif args.command == "compare":
            compare(args.baseline, args.candidate, args.output, args.bootstrap)
        else:
            ab(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"balance: {error}\n")
    print("balance analysis complete", flush=True)


if __name__ == "__main__":
    main()
