#!/usr/bin/env python3
"""Small deterministic synthetic statistical and experimental compatibility tests."""
import copy
import tempfile
import unittest
from pathlib import Path
import numpy as np
import pandas as pd
from scipy import stats
import statsmodels.api as sm
import balance as b


def visits(n=4000, repeats=1):
    rng = np.random.default_rng(81)
    exposure = rng.integers(0, 2, n)
    outcome = rng.random(n) < (0.3 + .4 * exposure)
    frame = pd.DataFrame({"effective_seed": np.repeat(np.arange(1, n + 1), repeats),
                          "visit": np.tile(np.arange(1, repeats + 1), n),
                          "floor": np.tile(np.arange(repeats), n), "direction": "descent",
                          "entry_hp": 20, "entry_max_hp": 20, "archetype": "CHAMBERS",
                          "exited": np.repeat(outcome.astype(int), repeats)})
    for c in b.BASE_NUMERIC:
        if c not in frame:
            frame[c] = 1
    return b.prepare_visits(frame), np.repeat(exposure, repeats)


def runs(n=10):
    r = pd.DataFrame({"seed": np.arange(1, n + 1), "effective_seed": np.arange(1, n + 1),
                      "agent": "omniscient-v2", "result": ["escaped"] * (n // 2) + ["death"] * (n - n // 2),
                      "stuck": 0})
    manifest = dict(schema_version=1, telemetry_schema_version=b.SCHEMA, agent="omniscient-v2",
                    effective_seed_count=n, seed_selection=dict(first=1, last=n, all_seeds=False),
                    experiment="test", variant="control", options=dict(max_actions=20000, telemetry=True))
    return r, manifest


class Factors(unittest.TestCase):
    def test_helpful_harmful(self):
        f, x = visits()
        positive = b.fit_factor(f, x, "helpful", "item")
        negative = b.fit_factor(f, 1 - x, "harmful", "monster")
        self.assertEqual(positive["status"], "ok")
        self.assertGreater(positive["coefficient"], 0)
        self.assertLess(negative["coefficient"], 0)

    def test_bh_known_values(self):
        np.testing.assert_allclose(b.bh_qvalues([.01, .04, .03, .002]), [.02, .04, .04, .008])
        self.assertTrue(np.isnan(b.bh_qvalues([np.nan])[0]))

    def test_insufficient_and_separation(self):
        f, _ = visits()
        x = np.zeros(len(f)); x[:20] = 1
        self.assertEqual(b.fit_factor(f, x, "rare", "item")["status"], "insufficient_data")
        separated = f.exited.to_numpy()
        self.assertEqual(b.fit_factor(f, separated, "selected", "item")["status"], "unstable_model")

    def test_seed_cluster_covariance(self):
        f, x = visits(n=1000, repeats=4)
        result = b.fit_factor(f, x, "repeated", "item")
        self.assertEqual(result["clusters"], 1000)
        base, _ = b.design_matrix(f)
        independent = sm.GLM(f.exited.to_numpy(), np.column_stack([base, x]), family=sm.families.Binomial()).fit()
        clustered_width = result["coefficient_high"] - result["coefficient_low"]
        independent_width = np.diff(independent.conf_int()[-1])[0]
        self.assertGreater(clustered_width, independent_width * 1.8)

    def test_survivorship_reversal(self):
        # Only early survivors enter the late floor; the late monster is harmful
        # within that floor, yet appears helpful against whole-run escape.
        rng = np.random.default_rng(9)
        n = 6000
        early_exit = rng.random(n) < .25
        exposure = rng.integers(0, 2, n)
        late_exit = rng.random(n) < (.9 - .25 * exposure)
        escape = early_exit & late_exit
        naive_exposure = early_exit & exposure.astype(bool)
        self.assertGreater(stats.spearmanr(naive_exposure, escape).statistic, 0)
        early, _ = visits(n)
        early["exited"] = early_exit.astype(int)
        late = early[early_exit].copy()
        late["floor"] = 12; late["visit"] = 2; late["stratum"] = "12:descent"
        late["exited"] = late_exit[early_exit].astype(int)
        f = pd.concat([early, late], ignore_index=True)
        x = np.concatenate([np.zeros(n), exposure[early_exit]])
        result = b.fit_factor(f, x, "late_monster", "monster")
        self.assertEqual(result["status"], "ok")
        self.assertLess(result["coefficient"], 0)
        self.assertEqual(result["modeled_visits"], int(early_exit.sum()))


class Paired(unittest.TestCase):
    def test_identical(self):
        result = b.paired_binary([0, 1, 0, 1], [0, 1, 0, 1])
        self.assertEqual(result["delta_pp"], 0)
        self.assertEqual(result["baseline_loss_candidate_win"], 0)
        self.assertEqual(result["baseline_win_candidate_loss"], 0)
        self.assertEqual(result["ci_pp"], [0, 0])

    def test_known_discordance(self):
        result = b.paired_binary([1] * 3 + [0] * 9, [0] * 3 + [1] * 9)
        self.assertAlmostEqual(result["p_value"], .14599609375)
        self.assertEqual(result["delta_pp"], 50)

    def test_seed_order_pairing(self):
        r, m = runs()
        baseline, candidate = b.pair_runs(m, r, m, r.iloc[::-1])
        pd.testing.assert_frame_equal(baseline, candidate)

    def test_rejections(self):
        r, m = runs()
        with self.assertRaisesRegex(ValueError, "seed"):
            different, dm = runs(9)
            b.pair_runs(m, r, dm, different)
        with self.assertRaisesRegex(ValueError, "agent"):
            different = r.assign(agent="omniscient-v3")
            dm = {**m, "agent": "omniscient-v3"}
            b.pair_runs(m, r, dm, different)
        for result in ("SIM_STUCK", "SIM_ERROR", "abandoned"):
            different = r.copy(); different.loc[0, "result"] = result
            with self.assertRaisesRegex(ValueError, "failures"):
                b.pair_runs(m, r, m, different)
        different = r.copy(); different.loc[0, "effective_seed"] = 2
        with self.assertRaisesRegex(ValueError, "duplicate"):
            b.pair_runs(m, r, m, different)
        with self.assertRaisesRegex(ValueError, "schema"):
            b.pair_runs(m, r, {**m, "telemetry_schema_version": 999}, r)

    def test_bootstrap_stable(self):
        d = np.arange(-10, 20)
        self.assertEqual(b.paired_bootstrap(d), b.paired_bootstrap(d))
        self.assertEqual(b.paired_bootstrap(d, statistic="median"), b.paired_bootstrap(d, statistic="median"))

    def test_census(self):
        population = np.arange(1, 65536)
        self.assertTrue(b.is_census(population[::-1]))
        self.assertFalse(b.is_census(np.arange(0, 65535)))
        self.assertFalse(b.is_census(np.concatenate([population[:-1], [1]])))
        result = b.paired_binary(population % 2, population % 3 == 0, census=True)
        self.assertIsNone(result["p_value"])
        self.assertIsNone(result["ci_pp"])

    def test_hash_validation(self):
        _, manifest = runs()
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            (root / "runs.csv").write_text("original")
            manifest["output_csv_sha256"] = {"runs.csv": b.sha256(root / "runs.csv")}
            b.write_json(root / "manifest.json", manifest)
            b.read_manifest(root)
            (root / "runs.csv").write_text("modified")
            with self.assertRaisesRegex(ValueError, "hash"):
                b.read_manifest(root)


if __name__ == "__main__":
    unittest.main()
