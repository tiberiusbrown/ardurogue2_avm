"""Provenance, paired-policy identity and failure handling regressions."""
import csv
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import competence_experiments as c

EXE=Path(sys.argv.pop(1)).resolve()

class Reports(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.root=Path(self.temp.name)
        self.a=self.root/'a'
        subprocess.run([str(EXE),'--agent','explorer-v1','--seeds','1:3','--output',str(self.a)],check=True,capture_output=True)
        c.summary(self.a)

    def tearDown(self): self.temp.cleanup()

    def test_pair_and_zero_count_intervals(self):
        result=c.paired(self.a,self.a)
        self.assertEqual(result['difference_pp'],0)
        self.assertEqual(result['exact_mcnemar_p'],1)
        self.assertGreater(result['control_exact_95_percent'][1],0)
        self.assertLess(result['paired_conservative_exact_95_pp'][0],0)
        self.assertGreater(result['paired_conservative_exact_95_pp'][1],0)
        self.assertIn('not evidence',result['uncertainty_note'])

    def test_seal_and_failures(self):
        path=self.a/'runs.csv'
        with path.open(newline='') as file:
            reader=csv.DictReader(file); rows=list(reader); fields=reader.fieldnames
        rows[0]['result']='SIM_STUCK'
        with path.open('w',newline='') as file:
            writer=csv.DictWriter(file,fields); writer.writeheader(); writer.writerows(rows)
        with self.assertRaisesRegex(ValueError,'hash mismatch'): c.summary(self.a)
        (self.a/'raw-sha256.json').unlink()
        with self.assertRaisesRegex(ValueError,'invalid simulation'): c.summary(self.a)

    def test_duplicates_are_invalid(self):
        path=self.a/'runs.csv'
        lines=path.read_text().splitlines(); path.write_text('\n'.join(lines+[lines[1]])+'\n')
        (self.a/'raw-sha256.json').unlink()
        with self.assertRaisesRegex(ValueError,'duplicate seed'): c.summary(self.a)

    def test_policy_mismatch_requires_explicit_label(self):
        b=self.root/'b'; shutil.copytree(self.a,b)
        path=b/'manifest.json'; manifest=json.loads(path.read_text()); manifest['agent_policy_hash']='different'
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError,'identical policy'): c.paired(self.a,b)
        self.assertEqual(c.paired(self.a,b,True)['kind'],'policy comparison')

if __name__=='__main__': unittest.main()
