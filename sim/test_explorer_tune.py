"""Deterministic fitness, configuration forwarding and checkpoint validation."""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile

def main():
    exe=Path(sys.argv[1]); simulator=Path(sys.argv[2])
    with tempfile.TemporaryDirectory(prefix='explorer-tune-') as directory:
        root=Path(directory)
        def search(name,jobs,*extra,valid=True):
            out=root/name
            p=subprocess.run([str(exe),'--seeds','1:8','--population','3','--generations','2',
                              '--jobs',str(jobs),'--random-seed','41','--simulator',str(simulator),
                              '--output',str(out),*extra],capture_output=True,text=True)
            assert (p.returncode==0)==valid,p.stderr
            return out
        a=search('a',1); b=search('b',3)
        rows=list(csv.DictReader((a/'history.csv').open()))
        other=list(csv.DictReader((b/'history.csv').open()))
        assert len(rows)==6
        for x,y in zip(rows,other):
            for k in x:
                if k!='seconds': assert x[k]==y[k],(k,x,y)
        valid=[r for r in rows if r['simulator_failures']=='0']
        winner=max(valid,key=lambda r:(int(r['escapes']),int(r['total_score']),-int(r['candidate'])))
        assert (a/'best.json').read_bytes()==(a/'candidates'/f"{winner['candidate']}.json").read_bytes()
        assert (a/'best.json').read_bytes()==(b/'best.json').read_bytes()
        manifest=json.loads((a/'manifest.json').read_text())
        assert manifest['objective']==['escapes descending','total score descending']
        staged=search('staged',3,'--initial-seeds','2','--plateau-generations','1')
        staged_rows=list(csv.DictReader((staged/'history.csv').open()))
        assert {int(r['seed_count']) for r in staged_rows}=={2,8}
        full=[r for r in staged_rows if r['seed_count']=='8' and r['simulator_failures']=='0']
        winner=max(full,key=lambda r:(int(r['escapes']),int(r['total_score']),-int(r['candidate'])))
        assert (staged/'best.json').read_bytes()==(staged/'candidates'/f"{winner['candidate']}.json").read_bytes()
        assert json.loads((staged/'best-result.json').read_text())['last_seed']==8
        failed=search('fail',1,'--max-actions','1',valid=False)
        assert not (failed/'best.json').exists()
        assert int(next(csv.DictReader((failed/'history.csv').open()))['simulator_failures'])==8
    print('Explorer evolutionary tuning checks passed')

if __name__=='__main__': main()
