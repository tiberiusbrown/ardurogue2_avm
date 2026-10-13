#!/usr/bin/env python3
"""Describe magic, study tradeoffs and retained resources in validated run sets."""
import argparse
import json
from pathlib import Path
import pandas as pd
from balance import load_directory, floor_survival, write_json

SPELLS = ("Magic Dart", "Frostbind", "Repulsion", "Confound", "Sense Traps", "Blink",
          "Shockwave", "Piercing Bolt", "Banish", "Exchange", "Cleanse", "Fireburst", "Phase Step", "Chain Lightning", "Slumber", "Blood Bolt")
COUNTERS = ("generated", "collected", "studied", "study_turns", "learned", "interrupted",
            "message_interruptions", "hunger_interruptions", "canceled", "casts", "mana_spent",
            "damage", "kills", "displaced", "displacement_tiles", "multi_tile_pushes", "mana_regenerated",
            "direct_damage", "chained_damage", "disabled", "stun_turns", "hp_paid", "self_deaths", "status_targets", "status_turns", "traps_revealed", "statuses_cleared")


def report(directory, output):
    manifest, tables = load_directory(directory, visits=False)
    spells = pd.read_csv(directory / "spells.csv", keep_default_na=False)
    effect_fields = ("status_targets", "status_turns", "traps_revealed", "statuses_cleared")
    effect_counters_available = all(c in spells for c in effect_fields)
    for column in effect_fields:
        if column not in spells:
            spells[column] = 0  # Historical schemas did not record these effects.
    runs = tables["runs"]
    if (len(spells) != len(runs) * len(SPELLS) or
        spells.duplicated(["effective_seed", "spell"]).any() or
        set(spells.spell) != set(range(len(SPELLS))) or
        set(spells.effective_seed) != set(runs.effective_seed) or
        set(spells.agent) != {manifest["agent"]} or
        (spells[list(COUNTERS)] < 0).any().any()):
        raise ValueError("incomplete or invalid spell telemetry")
    learned = spells.groupby("effective_seed").learned.sum()
    descent_learned = pd.Series(0, index=learned.index, dtype="int64")
    consumed = descent_learned.copy()
    for chunk in pd.read_csv(directory / "visit_items.csv", keep_default_na=False,
                             usecols=["effective_seed", "direction", "item", "consumed"],
                             chunksize=200000):
        books = chunk[chunk.item == "SPELLBOOK"]
        consumed = consumed.add(books.groupby("effective_seed").consumed.sum(), fill_value=0)
        descending = books[books.direction == "descent"]
        descent_learned = descent_learned.add(descending.groupby("effective_seed").consumed.sum(), fill_value=0)
    magic_path = directory / "magic.csv"
    if magic_path.is_file():
        magic = pd.read_csv(magic_path, keep_default_na=False)
        if (magic.duplicated(["effective_seed", "visit", "spell"]).any() or
            set(magic.effective_seed) != set(runs.effective_seed) or
            not magic.groupby(["effective_seed", "visit"]).size().eq(len(SPELLS)).all() or
            not magic.groupby("effective_seed").learned.sum().reindex(learned.index).eq(learned).all()):
            raise ValueError("incomplete or inconsistent per-visit magic telemetry")
        completed = magic.groupby("effective_seed").books_completed.sum().reindex(learned.index, fill_value=0)
        destroyed = consumed.reindex(learned.index) - completed
        descent = magic[magic.direction == "descent"]
        descent_lower = descent.groupby("effective_seed").learned.sum().reindex(learned.index, fill_value=0)
        descent_upper = descent_lower.copy()  # Authoritative learning events, independent of book consumption.
    else:
        # Historical single-spell datasets lack per-visit learning events.
        destroyed = consumed.reindex(learned.index) - learned
        descent_lower = (descent_learned - destroyed).clip(lower=0)
        descent_upper = pd.concat([descent_learned, learned], axis=1).min(axis=1)
    if (destroyed < 0).any():
        raise ValueError("spellbook consumption is below completed books")

    cohorts = {}
    descent_cohorts = {}
    for name, selection in (("all", runs), ("reached_depth_10", runs[runs.deepest_floor >= 10]),
                            ("reached_depth_15", runs[runs.deepest_floor >= 15]),
                            ("escaped", runs[runs.result == "escaped"])):
        values = learned.reindex(selection.effective_seed)
        cohorts[name] = dict(runs=len(selection), mean=float(values.mean()) if len(values) else None,
                             median=float(values.median()) if len(values) else None,
                             quartiles=[float(x) for x in values.quantile([.25, .75])] if len(values) else [])
        lower = descent_lower.reindex(selection.effective_seed)
        upper = descent_upper.reindex(selection.effective_seed)
        descent_cohorts[name] = dict(runs=len(selection),
            mean_bounds=[float(lower.mean()), float(upper.mean())] if len(lower) else [],
            median_bounds=[float(lower.median()), float(upper.median())] if len(lower) else [],
            quartile_bounds=[[float(x) for x in v.quantile([.25, .75])] for v in (lower, upper)] if len(lower) else [])
    totals = spells.groupby("spell")[list(COUNTERS)].sum()
    totals.insert(0, "name", SPELLS)
    raw_items = tables["items"]
    lightning = raw_items[raw_items.item == "WAND_LIGHTNING"]
    lightning_diagnostics = {name: int(lightning[name].sum()) for name in
                             ("generated", "picked_up", "wands_activated", "charges_used",
                              "damage", "direct_damage", "chained_damage", "kills")}
    phase_users = spells[(spells.spell == 12) & (spells.casts > 0)].effective_seed
    phase_runs = runs[runs.effective_seed.isin(phase_users)]
    items = raw_items.groupby("item").sum(numeric_only=True)
    resource = {}
    for group, prefix in (("potions", "POTION_"), ("scrolls", "SCROLL_"), ("wands", "WAND_")):
        sample = items.loc[items.index.str.startswith(prefix)]
        resource[group] = {name: int(sample[name].sum()) for name in
                           ("generated", "picked_up", "used", "consumed", "charges_used", "carried")}
    # The legacy potion CSV labels omit POTION_, unlike scroll/wand labels.
    potion_names = ("HEALING", "CONFUSION", "POISON", "HARMING", "STRENGTH", "DEXTERITY",
                    "PARALYSIS", "SLOWING", "EXPERIENCE", "INVISIBILITY", "BLINDNESS",
                    "SEE_INVISIBLE", "EXTRA_HEALING", "RESTORE_VITALITY", "HASTE",
                    "LEVITATION", "FIRE_RESISTANCE", "FIRE", "MANA_RESTORATION")
    potions = items.loc[items.index.isin(potion_names)]
    resource["potions"] = {name: int(potions[name].sum()) for name in resource["potions"]}
    magic_totals = {}
    if magic_path.is_file():
        magic_totals = {field: int(magic[field].sum()) for field in (
            "books_generated", "books_collected", "books_completed", "mana_regenerated",
            "mana_restored", "meditation_mp", "amulet_hp", "study_deaths", "shortly_after_study_deaths")}
    summary = dict(schema=3, effect_counters_available=effect_counters_available, magic=magic_totals, agent=manifest["agent"], policy_hash=manifest["agent_policy_hash"],
                   runs=len(runs), escapes=int((runs.result == "escaped").sum()),
                   escape_percent=float((runs.result == "escaped").mean() * 100),
                   learned_additional=cohorts, learned_on_descent=descent_cohorts,
                   spellbooks_destroyed=int(destroyed.sum()),
                   totals={k: int(totals[k].sum()) for k in COUNTERS},
                   resources=resource, total_monster_kills=int(tables["monsters"].killed.sum()),
                   lightning_wand=lightning_diagnostics,
                   phase_step_users=dict(runs=len(phase_runs), escapes=int(phase_runs.result.eq("escaped").sum()),
                                         deaths=int(phase_runs.result.eq("death").sum()),
                                         note="Descriptive survival-selected cohort; not a causal spell benefit."),
                   direct_spell_kills=int(totals.kills.sum()),
                   arrows_fired=int(tables["ranged"].fired.sum()),
                   bow_turns=int(tables["ranged"].bow_turns.sum()),
                   death_causes={str(k): int(v) for k,v in runs[runs.result == "death"].death_cause.value_counts().items()})
    output.mkdir(parents=True, exist_ok=True)
    write_json(output / "magic.json", summary)
    totals.to_csv(output / "spells.csv", index_label="spell")
    floor_survival(tables).to_csv(output / "floor_survival.csv", index=False)
    if magic_path.is_file():
        fields = [c for c in COUNTERS if c in magic and c != "mana_regenerated"]
        by_floor = magic.groupby(["floor", "direction", "spell"])[fields].sum().reset_index()
        by_floor.insert(3, "name", by_floor.spell.map(dict(enumerate(SPELLS))))
        by_floor.to_csv(output / "spells_by_floor.csv", index=False)
        visits = magic[magic.spell.eq(0)].copy()
        for endpoint in ("entry", "exit"):
            visits[f"additional_known_{endpoint}"] = visits[f"{endpoint}_known"].map(lambda mask: (int(mask) & 0xfffe).bit_count())
        resources = visits.groupby(["floor", "direction"])[list(magic_totals)].sum()
        resources["visits"] = visits.groupby(["floor", "direction"]).size()
        for endpoint in ("entry", "exit"):
            resources[f"mean_additional_known_{endpoint}"] = visits.groupby(["floor", "direction"])[f"additional_known_{endpoint}"].mean()
        resources.to_csv(output / "magic_by_floor.csv")
    lines = [f"# Magic report: {manifest['agent']}", "",
             f"{len(runs):,} validated runs; {summary['escapes']:,} escapes ({summary['escape_percent']:.3f}%).", "",
             "Learned counts exclude starting Magic Dart. Depths use zero-based floor IDs.", "",
             "| Cohort | Runs | Whole-run mean | Descent mean | Descent median | Descent 25th–75th percentile |",
             "| --- | ---: | ---: | ---: | ---: | --- |"]
    for name, c in cohorts.items():
        d = descent_cohorts[name]
        lines.append(f"| {name} | {c['runs']} | {c['mean']} | {d['mean_bounds']} | {d['median_bounds']} | {d['quartile_bounds']} |")
    lines += ["", "| Spell | Generated | Collected | Studies | Learned | Interrupted | Casts | Damage | Kills | Pushed ≥2 tiles |",
              "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for row in totals.itertuples():
        lines.append(f"| {row.name} | {row.generated} | {row.collected} | {row.studied} | {row.learned} | {row.interrupted} | {row.casts} | {row.damage} | {row.kills} | {row.multi_tile_pushes} |")
    if effect_counters_available:
        lines += ["", "| Spell | Slow/confuse targets | Assigned status turns | Traps revealed | Conditions cleansed | Successful displacements |",
                  "| --- | ---: | ---: | ---: | ---: | ---: |"]
        for row in totals.itertuples():
            if row.status_targets or row.traps_revealed or row.statuses_cleared or row.displaced:
                lines.append(f"| {row.name} | {row.status_targets} | {row.status_turns} | {row.traps_revealed} | {row.statuses_cleared} | {row.displaced} |")
    lines += ["", "| Spell | Initial lightning damage | Chained damage | Disabled | Rolled stun actions | HP paid | Sacrifice deaths |",
              "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for row in totals.itertuples():
        if row.Index < 11:
            continue
        lines.append(f"| {row.name} | {row.direct_damage} | {row.chained_damage} | {row.disabled} | {row.stun_turns} | {row.hp_paid} | {row.self_deaths} |")
    lines += ["", "Lightning wand: " + ", ".join(f"{k}={v}" for k, v in lightning_diagnostics.items()) + ".",
              f"Phase Step users: {len(phase_runs)} runs, {int(phase_runs.result.eq('escaped').sum())} escaped. This cohort is selected by learning, use and survival, and does not estimate causal benefit."]
    lines += ["", "Spell damage/kills include its primary and chained hits; trap damage retains environmental attribution. The initial/chained split is specific to lightning effects.",
              "Displacement counts forced tiles before the enemy response, not proven counterfactual survival.",
              "Message interruptions include attack misses and condition/fuel expiration. Canceled also includes lifecycle/death cancellation.",
              "Mana regenerated is counted once per run in Magic Dart's row. Resource totals and floor survival accompany this report."]
    if magic_path.is_file():
        lines += ["Descent learning uses authoritative per-visit learning events. Completed books are counted separately from learned members; destruction/sacrifice is consumption minus completion."]
    else:
        lines += ["Historical descent learning bounds use consumed SPELLBOOK rows and total successful learning; destruction/sacrifice allocation is unknown."]
    lines += ["Starting Magic Dart is excluded from learned additional spells. Whole-run cohorts are descriptive and subject to survival selection."]
    (output / "magic.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(report(args.directory, args.output), indent=2))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(2, f"spell report: {error}\n")
