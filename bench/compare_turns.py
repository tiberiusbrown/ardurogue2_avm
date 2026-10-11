#!/usr/bin/env python3
"""Compare complete AVM suites and require saved-state/RNG/display equivalence."""
import argparse
import csv
import json
from pathlib import Path
from profile_turns import BENCHMARKS


def compare(reference, candidate):
    a=json.loads((reference/'summary.json').read_text())
    b=json.loads((candidate/'summary.json').read_text())
    if a['source_bench_sha256'] != b['source_bench_sha256']:
        raise ValueError('benchmark source differs')
    ar={r['name']:r for r in a['benchmarks']}
    br={r['name']:r for r in b['benchmarks']}
    expected={c.name for c in BENCHMARKS}
    if ar.keys()!=br.keys() or set(ar)!=expected: raise ValueError('case coverage differs or is incomplete')
    results=[]
    for name,x in ar.items():
        y=br[name]
        if not x['meets_goal'] or not y['meets_goal']: raise ValueError(f'{name}: timing failure')
        if x['endpoint']!=y['endpoint'] or x['display_hash']!=y['display_hash']:
            raise ValueError(f'{name}: endpoint/display differs')
        for file in ('before.json','after.json','game.bin','frame.pgm'):
            if (reference/name/file).read_bytes()!=(candidate/name/file).read_bytes():
                raise ValueError(f'{name}: {file} differs')
        af=sorted(p.name for p in (reference/name).glob('*frame.pgm'))
        bf=sorted(p.name for p in (candidate/name).glob('*frame.pgm'))
        if af!=bf: raise ValueError(f'{name}: status page count differs')
        for file in af:
            if (reference/name/file).read_bytes()!=(candidate/name/file).read_bytes():
                raise ValueError(f'{name}: status page {file} differs')
        # Status pagination has additional input boundaries and captures.
        ap=sorted(p.relative_to(reference/name) for p in (reference/name).rglob('game.bin'))
        bp=sorted(p.relative_to(candidate/name) for p in (candidate/name).rglob('game.bin'))
        if ap!=bp: raise ValueError(f'{name}: page coverage differs')
        for p in ap:
            for file in ('game.bin','after.json','frame.pgm'):
                rel=p.parent/file
                if (reference/name/rel).read_bytes()!=(candidate/name/rel).read_bytes():
                    raise ValueError(f'{name}: {rel} differs')
        results.append(dict(case=name,before_ms=x['ms'],after_ms=y['ms'],
                            improvement_ms=x['ms']-y['ms'],
                            improvement_percent=100*(x['ms']-y['ms'])/x['ms'],equivalent=True))
    return results


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('reference',type=Path); p.add_argument('candidate',type=Path)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args(); rows=compare(args.reference,args.candidate)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with args.output.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
    print(f'{len(rows)} equivalent cases; worst {max(r["before_ms"] for r in rows):.3f} -> {max(r["after_ms"] for r in rows):.3f} ms')


if __name__=='__main__': main()
