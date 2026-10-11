"""Streaming explorer competence and same-policy lighting/offhand summaries."""
import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path

def rows(path):
    with path.open(newline='') as file:
        yield from csv.DictReader(file)

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()

def summarize(path):
    m=json.loads((path/'manifest.json').read_text())
    runs={}
    for r in rows(path/'runs.csv'):
        seed=int(r['effective_seed'])
        if seed in runs: raise ValueError('duplicate effective seed')
        if r['result'] not in ('escaped','death'): raise ValueError(f"simulator failure: {seed}: {r['reason']}")
        runs[seed]=r
    if len(runs)!=m['effective_seed_count']: raise ValueError('seed count mismatch')
    n=len(runs); totals=collections.Counter()
    for r in runs.values():
        for k in ('actions','turns','score','has_yendor','fuel_exhaustions','blind_turns','shield_turns','offhand_light_turns',*[f'light_{i}' for i in range(7)]):
            totals[k]+=int(r[k])
    escapes=sum(r['result']=='escaped' for r in runs.values())
    floors=collections.defaultdict(collections.Counter)
    for r in rows(path/'floors.csv'):
        f=floors[f"{r['direction']}:{r['floor']}"]; f['entered']+=1; f['exited']+=int(r['exited'])
        for k in ('turns','actions','items_picked_up'): f[k]+=int(r[k])
    e=collections.Counter(); cover=[]; surveyed=[]; discovery=[]; reached=[]; stretches=[]
    for r in rows(path/'exploration.csv'):
        e['visits']+=1
        for k in ('exploring_turns','travel','repeated_visits','frontier_changes','unreachable_attempts','missing_items','unseen_damage','items_observed','displacements',*[f'monsters_first_seen_{i}' for i in range(7)]): e[k]+=int(r[k])
        denom=int(r['reachable_floor_cells'])
        if denom:
            cover.append(int(r['floor_cells'])/denom)
            surveyed.append(int(r.get('surveyed_floor_cells',r['floor_cells']))/denom)
        d=int(r['stair_discovery_turn']); t=int(r['stair_reached_turn']); shortest=int(r['shortest_objective_route'])
        if d>=0: discovery.append(d)
        if t>=0: reached.append(t)
        if t>=0 and shortest>0: stretches.append(int(r['travel'])/shortest)
    mean=lambda values:sum(values)/len(values) if values else None
    seen=sum(e[f'monsters_first_seen_{i}'] for i in range(7))
    early=sum(e[f'monsters_first_seen_{i}'] for i in range(2,7))
    derived=dict(average_coverage=mean(cover),average_surveyed_coverage=mean(surveyed),average_stair_discovery_turn=mean(discovery),average_stair_reached_turn=mean(reached),
             average_movement_stretch=mean(stretches),monsters_seen=seen,monsters_seen_before_adjacent=early,
             monsters_seen_before_adjacent_percent=100*early/seen if seen else 0)
    for k,v in derived.items(): e[k]=v
    items=collections.defaultdict(collections.Counter)
    for r in rows(path/'items.csv'):
        for k in ('generated','picked_up','used','turns_equipped'): items[r['item']][k]+=int(r[k])
    out=dict(agent=m['agent'],agent_policy_hash=m['agent_policy_hash'],seeds=n,escapes=escapes,deaths=n-escapes,simulator_failures=0,
             escape_percent=100*escapes/n,totals=dict(totals),average_actions=totals['actions']/n,
             death_causes=dict(collections.Counter(r['death_cause'] for r in runs.values() if r['result']=='death')),
             deepest_floors=dict(collections.Counter(r['deepest_floor'] for r in runs.values())),
             floors=dict(floors),exploration=dict(e),items=dict(items),manifest=m,
             source_sha256={p.name:digest(p) for p in path.glob('*.csv')})
    return out,runs

def paired(a,b,ar,br):
    if a['agent']!=b['agent'] or a['agent_policy_hash']!=b['agent_policy_hash']: raise ValueError('paired policy mismatch')
    if ar.keys()!=br.keys(): raise ValueError('paired seed mismatch')
    if a['manifest']['options']['max_actions']!=b['manifest']['options']['max_actions']: raise ValueError('paired limits differ')
    gained=[s for s in ar if ar[s]['result']!='escaped' and br[s]['result']=='escaped']
    lost=[s for s in ar if ar[s]['result']=='escaped' and br[s]['result']!='escaped']
    n=len(ar); discordant=len(gained)+len(lost); delta=(len(gained)-len(lost))/n
    if discordant:
        tail=min(len(gained),len(lost))
        p=min(1.,2*sum(math.exp(math.lgamma(discordant+1)-math.lgamma(k+1)-math.lgamma(discordant-k+1)-discordant*math.log(2)) for k in range(tail+1)))
    else: p=1.
    se=math.sqrt(max(0,discordant/n-delta*delta)/n)
    return dict(gained=len(gained),lost=len(lost),gained_seeds=gained,lost_seeds=lost,
                difference_pp=100*delta,paired_95_normal_interval_pp=[100*(delta-1.96*se),100*(delta+1.96*se)],p_mcnemar_exact=p)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,help='root containing named run directories')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args(); out=args.output or args.directory/'report'; out.mkdir(parents=True,exist_ok=True)
    names=('omniscient-equipment','omniscient-full','explorer-equipment','explorer-full','explorer-no-shields')
    summaries={}; runs={}
    for name in names:
        if (args.directory/name/'runs.csv').exists(): summaries[name],runs[name]=summarize(args.directory/name)
    if not summaries: raise ValueError('no named run directories found')
    comparisons={}
    for name,a,b in [('oracle light','omniscient-equipment','omniscient-full'),('explorer light','explorer-equipment','explorer-full'),
                     ('explorer shields removed','explorer-equipment','explorer-no-shields')]:
        if a in summaries and b in summaries: comparisons[name]=paired(summaries[a],summaries[b],runs[a],runs[b])
    ranked=sorted((v['p_mcnemar_exact'],name) for name,v in comparisons.items()); q=1.
    for i in range(len(ranked)-1,-1,-1):
        p,name=ranked[i]; q=min(q,p*len(ranked)/(i+1)); comparisons[name]['q_bh']=q
    (out/'summary.json').write_text(json.dumps(dict(conditions=summaries,pairs=comparisons),indent=2)+'\n')
    lines=['# Explorer, lighting and offhand measurements','',
           '| Condition | Seeds | Escapes | Deaths | Failures | Total score | Mean actions | Known/surveyed coverage per visit | Exploration turns/visit | Early monster detection % |',
           '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
    for name,s in summaries.items():
        e=s['exploration']
        lines.append(f"| {name} | {s['seeds']} | {s['escapes']} | {s['deaths']} | 0 | {s['totals']['score']} | {s['average_actions']:.1f} | {e['average_coverage']:.3f}/{e['average_surveyed_coverage']:.3f} | {e['exploring_turns']/e['visits']:.1f} | {e['monsters_seen_before_adjacent_percent']:.1f} |")
    lines+=['','| Same-agent paired intervention | Escape difference pp | Gained | Lost | Exact McNemar p | BH q |',
            '| --- | ---: | ---: | ---: | ---: | ---: |']
    for name,s in comparisons.items(): lines.append(f"| {name} | {s['difference_pp']:+.3f} | {s['gained']} | {s['lost']} | {s['p_mcnemar_exact']:.4g} | {s['q_bh']:.4g} |")
    lines+=['','Full floor survival, death causes, radius distribution, fuel/offhand turns, stair times, movement stretch, observed/acquired items, failures and source hashes are in summary.json.',
            'Coverage and per-visit means are conditioned on survival. Early detection bins use Chebyshev distance; distances 2–6 count as before adjacent contact. Cross-agent rows are descriptive.',
            'items_observed counts ground placements whose cells enter terrain sight, including covered sprites; it is a discovery proxy. Actual pickups are separate. This reporting counter never feeds the policy.',
            'Normal paired intervals are approximate; zero discordant escapes do not establish absence of a prospective lighting effect. Use balance.py for paired bootstrap intervals and reconciled telemetry.']
    (out/'summary.md').write_text('\n'.join(lines)+'\n'); print('\n'.join(lines))

if __name__=='__main__': main()
