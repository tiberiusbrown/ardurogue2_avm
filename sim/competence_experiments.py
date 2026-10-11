"""Reproducible competence ablations and isolated gameplay experiments.

Each command freezes the binary, parameters, seed range and raw manifests.
Policy comparisons are explicitly descriptive; gameplay pairs reject unequal
policy hashes. No failed run is converted to a death or excluded from a pair.
"""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def read_rows(path):
    with path.open(newline='') as file:
        yield from csv.DictReader(file)


def summary(path):
    manifest = json.loads((path / 'manifest.json').read_text())
    seal=path/'raw-sha256.json'
    hashes=manifest.get('output_csv_sha256')
    if hashes is None:
        if seal.exists(): hashes=json.loads(seal.read_text())
        else:
            hashes={p.name:digest(p) for p in path.glob('*.csv')}
            seal.write_text(json.dumps(hashes,indent=2)+'\n')
    for name, expected in hashes.items():
        if digest(path / name) != expected:
            raise ValueError(f'raw file hash mismatch: {path / name}')
    runs = {}
    for row in read_rows(path / 'runs.csv'):
        seed = int(row['effective_seed'])
        if seed in runs:
            raise ValueError('duplicate seed')
        if row['result'] not in ('death', 'escaped'):
            raise ValueError(f"invalid simulation {seed}: {row['result']}: {row['reason']}")
        runs[seed] = row
    if len(runs) != manifest['effective_seed_count']:
        raise ValueError('incomplete batch')
    n = len(runs)
    totals = collections.Counter()
    for row in runs.values():
        for key in ('turns', 'actions', 'score', 'has_yendor', 'lord_killed',
                    'shield_turns', 'offhand_light_turns', 'fuel_exhaustions',
                    *[f'light_{i}' for i in range(7)]):
            totals[key] += int(row.get(key, 0))
    floors = collections.defaultdict(collections.Counter)
    terminal = {}
    for row in read_rows(path / 'floors.csv'):
        seed = int(row['effective_seed'])
        terminal[seed] = int(row['visit'])
        f = floors[f"{row['direction']}:{int(row['floor'])+1}"]
        f['entered'] += 1
        f['exited'] += int(row['exited'])
        for key in ('actions', 'turns', 'damage_taken', 'entry_level', 'entry_max_hp',
                    'entry_hp', 'entry_food_units', 'entry_healing_units', 'entry_armor_rating'):
            f[key] += int(row[key])
    exploration = collections.Counter()
    stair_times, stretch, coverage, contacts = [], [], [], []
    flags = collections.Counter()
    for row in read_rows(path / 'exploration.csv'):
        exploration['visits'] += 1
        for key in ('exploring_turns', 'combat_turns', 'travel', 'repeated_visits',
                    'frontier_changes', 'unseen_damage', 'items_observed', 'healing_then_death',
                    'food_waste', 'identified_potions', 'caution_actions', 'serious_actions', 'lethal_actions',
                    *[f'monsters_first_seen_{i}' for i in range(7)]):
            exploration[key] += int(row.get(key, 0))
        for key in (*[f'initial_alert_distance_{i}' for i in range(9)], *[f'initial_alert_light_{i}' for i in range(7)]):
            exploration[key] += int(row.get(key, 0))
        if int(row.get('first_hostile_contact_turn',-1))>=0:
            contacts.append(int(row['first_hostile_contact_turn']))
        if int(row['stair_discovery_turn']) >= 0:
            stair_times.append(int(row['stair_discovery_turn']))
        if int(row['stair_reached_turn']) >= 0 and int(row['shortest_objective_route']) > 0:
            stretch.append(int(row['travel']) / int(row['shortest_objective_route']))
        if int(row['reachable_floor_cells']):
            coverage.append(int(row['surveyed_floor_cells']) / int(row['reachable_floor_cells']))
        seed = int(row['effective_seed'])
        if int(row['visit']) != terminal[seed] or runs[seed]['result'] != 'death':
            continue
        cause = runs[seed]['death_cause']
        # These are co-occurring diagnostic flags, not identified causes.
        flags['insufficient_known_healing'] += int(row.get('remaining_healing', 0)) == 0
        flags['healing_retained'] += int(row.get('remaining_healing', 0)) > 0
        flags['escape_retained'] += int(row.get('remaining_escape', 0)) > 0
        flags['control_retained'] += int(row.get('remaining_control', 0)) > 0
        flags['known_retreat_before_death'] += row.get('safe_retreat') == '1'
        flags['late_recognition_candidate'] += int(row.get('last_one', 0)) >= int(runs[seed]['max_hp']) / 2
        flags['early_contact_low_counterplay'] += row['floor'] == '0' and int(runs[seed]['turns']) < 30
        flags['invisible_attrition'] += cause == 'PHANTOM'
        flags['starvation'] += cause == 'starvation'
        flags['boss_failure'] += cause == 'LORD'
        flags['post_yendor_death'] += runs[seed]['has_yendor'] == '1'
    items = collections.defaultdict(collections.Counter)
    for row in read_rows(path / 'items.csv'):
        for key in ('generated', 'picked_up', 'used', 'consumed', 'carried', 'discarded', 'potions_drunk', 'potions_thrown'):
            items[row['item']][key] += int(row[key])
    seen = sum(exploration[f'monsters_first_seen_{i}'] for i in range(7))
    mean = lambda values: sum(values) / len(values) if values else None
    escapes = sum(r['result'] == 'escaped' for r in runs.values())
    out = dict(seeds=n, escapes=escapes, escape_percent=100*escapes/n, failures=0,
               mean_floor=mean([int(r['deepest_floor'])+1 for r in runs.values()]),
               deaths=dict(collections.Counter(r['death_cause'] for r in runs.values() if r['result']=='death')),
               totals=dict(totals), floors=dict(floors), exploration=dict(exploration), items=dict(items),
               diagnostic_flags=dict(flags), mean_stair_time=mean(stair_times), mean_stretch=mean(stretch),
               mean_first_contact=mean(contacts),
               surveyed_coverage=mean(coverage), early_detection_percent=100*sum(exploration[f'monsters_first_seen_{i}'] for i in range(2,7))/seen if seen else None,
               policy_hash=manifest['agent_policy_hash'], manifest=manifest)
    import statistics
    from survival_report import diagnostics
    out['median_floor']=statistics.median(int(r['deepest_floor'])+1 for r in runs.values())
    out['survival']=diagnostics(path,runs)
    return out, runs


def paired(control, treatment, policy_comparison=False):
    import numpy as np
    from scipy.stats import beta, binomtest
    a, ar = summary(control)
    b, br = summary(treatment)
    if ar.keys() != br.keys():
        raise ValueError('seed mismatch')
    if a['manifest']['agent'] != b['manifest']['agent']:
        raise ValueError('agent mismatch')
    if not policy_comparison and a['policy_hash'] != b['policy_hash']:
        raise ValueError('gameplay comparisons require identical policy hashes')
    if a['manifest']['options']['max_actions'] != b['manifest']['options']['max_actions']:
        raise ValueError('action limit mismatch')
    gain = [s for s in ar if ar[s]['result'] != 'escaped' and br[s]['result'] == 'escaped']
    loss = [s for s in ar if ar[s]['result'] == 'escaped' and br[s]['result'] != 'escaped']
    n, k = len(ar), len(gain)+len(loss)
    delta = 100*(len(gain)-len(loss))/n
    rng = np.random.default_rng(20261011)
    # Resample the three paired difference categories, avoiding a huge matrix.
    draws = rng.multinomial(n, [len(gain)/n, len(loss)/n, 1-k/n], size=10000)
    intervals = np.quantile(100*(draws[:,0]-draws[:,1])/n, [.025,.975]).tolist()
    proportion = lambda wins: [100*(beta.ppf(.025,wins,n-wins+1) if wins else 0),
                                100*(beta.ppf(.975,wins+1,n-wins) if wins<n else 1)]
    # Each gain/loss marginal is binomial. Simultaneous 97.5% exact intervals
    # give conservative >=95% bounds for their difference by the union bound,
    # without assuming the two mutually exclusive categories are independent.
    # Unlike a bootstrap with no discordances, these remain non-degenerate.
    marginal = lambda wins: [100*(beta.ppf(.0125,wins,n-wins+1) if wins else 0),
                              100*(beta.ppf(.9875,wins+1,n-wins) if wins<n else 1)]
    gain_ci, loss_ci = marginal(len(gain)), marginal(len(loss))
    census=n==65535 and set(ar)==set(range(1,65536))
    depths=np.array([int(br[s]['deepest_floor'])-int(ar[s]['deepest_floor']) for s in ar])
    values,counts=np.unique(depths,return_counts=True)
    depth_draws=rng.multinomial(n,counts/n,size=10000)@values/n
    floor_pairs=[]
    for floor in range(1,16):
        gained=sum(int(ar[s]['deepest_floor'])<floor<=int(br[s]['deepest_floor']) for s in ar)
        lost=sum(int(br[s]['deepest_floor'])<floor<=int(ar[s]['deepest_floor']) for s in ar)
        discord=gained+lost
        floor_pairs.append(dict(floor=floor+1,gains=gained,losses=lost,difference_pp=100*(gained-lost)/n,
                                exact_mcnemar_p=None if census else binomtest(gained,discord,.5).pvalue if discord else 1.))
    if not census:
        from statsmodels.stats.multitest import multipletests
        qs=multipletests([r['exact_mcnemar_p'] for r in floor_pairs],method='fdr_bh')[1]
        for row,q in zip(floor_pairs,qs): row['bh_q']=float(q)
    from survival_report import paired_diagnostics
    return dict(survival_differences=paired_diagnostics(control,treatment,ar,br),kind='policy comparison' if policy_comparison else 'controlled gameplay intervention',
                control=a, treatment=b, difference_pp=delta,
                mean_depth_difference=float(depths.mean()), mean_depth_paired_95=None if census else np.quantile(depth_draws,[.025,.975]).tolist(),
                descent_floor_pairs=floor_pairs,
                relative_percent=100*(b['escapes']-a['escapes'])/a['escapes'] if a['escapes'] else None,
                gains=gain, losses=loss, paired_bootstrap_95_pp=None if census else intervals,
                paired_conservative_exact_95_pp=None if census else
                    [gain_ci[0]-loss_ci[1],gain_ci[1]-loss_ci[0]],
                exact_mcnemar_p=None if census else binomtest(len(gain),k,.5).pvalue if k else 1.,
                control_exact_95_percent=None if census else proportion(a['escapes']), treatment_exact_95_percent=None if census else proportion(b['escapes']),
                uncertainty_note='Complete finite seed population; no sampling interval.' if census else
                    'Conditional on this fixed policy and seed sample; zero discordances give a degenerate paired bootstrap, not evidence of zero effect.')


def suite(name):
    if name in ('baseline', 'validation'):
        return [(f'{agent}-{light}', agent, {}, ['full-light'] if light=='full' else [])
                for agent in ('explorer-v1','omniscient-v2') for light in ('equipment','full')]
    if name == 'offhand':
        return [(label,'explorer-v1',{'offhand_mode':mode},['legacy-awareness']) for mode,label in enumerate(
            ('legacy','strong-light','contextual','always-light','always-shield','no-switch'))]
    if name == 'ablations':
        return [('combined','explorer-v1',{},['legacy-awareness'])]+[(f'without-{key}','explorer-v1',{key:0},['legacy-awareness']) for key in
            ('tactical_risk','context_healing','obstruction_gain','invisible_response','resource_planning')]
    if name == 'awareness':
        return [(f'{agent}-{light}-{mode}',agent,{}, (['full-light'] if light=='full' else [])+([mode] if mode=='light-awareness' else ['legacy-awareness']))
                for agent in ('explorer-v1','omniscient-v2') for light in ('equipment','full') for mode in ('legacy','light-awareness')]
    if name == 'balance':
        return [(label,'explorer-v1',{},rules) for label,rules in (
            ('control',[]),('early-snake',['replace-monster:SNAKE:BAT:floor=0:max=1']),
            ('phantom',['replace-monster:PHANTOM:ZOMBIE']),('dragon',['replace-monster:DRAGON:GRIFFIN:max=1']),
            ('food',['replace-item:BUCKLER:FOOD']),('healing',['replace-item:BUCKLER:POTION_HEALING']),
            ('escape',['replace-item:BUCKLER:SCROLL_TELEPORT']))]
    raise ValueError(name)


def comparisons(name, root):
    if name=='awareness':
        pairs=[(f'{a}-{l}-legacy',f'{a}-{l}-light-awareness') for a in ('explorer-v1','omniscient-v2') for l in ('equipment','full')]
    elif name in ('balance','offhand','ablations'):
        labels=[s[0] for s in suite(name)]
        pairs=[(labels[0],label) for label in labels[1:]]
    else:
        pairs=[]
    reports={f'{a}__{b}':paired(root/a,root/b,name in ('offhand','ablations')) for a,b in pairs}
    if reports:
        from statsmodels.stats.multitest import multipletests
        if all(r['exact_mcnemar_p'] is None for r in reports.values()):
            for report in reports.values(): report['exploratory_bh_q']=None
        else:
            qs=multipletests([r['exact_mcnemar_p'] for r in reports.values()],method='fdr_bh')[1]
            for report,q in zip(reports.values(),qs): report['exploratory_bh_q']=float(q)
        (root/'paired.json').write_text(json.dumps(reports,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulator',type=Path)
    parser.add_argument('--parameters',type=Path)
    parser.add_argument('--suite',choices=('baseline','validation','offhand','ablations','awareness','balance'))
    parser.add_argument('--seeds',default='1:10000')
    parser.add_argument('--jobs',type=int,default=8)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compare',nargs=2,type=Path)
    parser.add_argument('--policy-comparison',action='store_true')
    parser.add_argument('--awareness',action='store_true',help='Apply the awareness candidate to every balance-suite condition')
    args=parser.parse_args()
    if args.compare:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(paired(*args.compare,args.policy_comparison),indent=2)+'\n')
        return
    if not args.suite or not args.simulator:
        parser.error('--suite and --simulator required for experiments')
    args.output.mkdir(parents=True,exist_ok=False)
    binary=args.output/'bin'; binary.mkdir()
    exe=binary/args.simulator.name
    shutil.copy2(args.simulator,exe)
    for dll in args.simulator.parent.glob('*.dll'):
        shutil.copy2(dll,binary/dll.name)
    frozen=subprocess.run([str(exe.resolve()),'--write-agent-defaults',str(binary/'parameters.json')],check=True)
    parameters=json.loads((binary/'parameters.json').read_text())
    if args.parameters:
        parameters.update(json.loads(args.parameters.read_text()))
    spec=dict(suite=args.suite,seeds=args.seeds,jobs=args.jobs,executable_sha256=digest(exe),parameters=parameters,conditions=suite(args.suite))
    if args.awareness:
        if args.suite!='balance': parser.error('--awareness requires --suite balance')
        for condition in spec['conditions']: condition[3].append('light-awareness')
    (args.output/'experiment.json').write_text(json.dumps(spec,indent=2)+'\n')
    summaries={}
    for label,agent,overrides,rules in spec['conditions']:
        configuration=args.output/f'{label}.json'
        configuration.write_text(json.dumps(dict(parameters,**overrides),indent=2)+'\n')
        command=[str(exe.resolve()),'--agent',agent,'--seeds',args.seeds,'--jobs',str(args.jobs),'--entry-state',
                 '--max-actions','50000','--output',str((args.output/label).resolve()),'--experiment',args.suite,'--variant',label]
        if agent=='explorer-v1': command+=['--agent-params',str(configuration.resolve())]
        for rule in rules: command+=['--intervention',rule]
        started=time.monotonic()
        with (args.output/f'{label}.log').open('w') as log:
            subprocess.run(command,stdout=log,stderr=log,check=True)
        s,_=summary(args.output/label); s['seconds']=time.monotonic()-started
        summaries[label]=s
        (args.output/'summary.json').write_text(json.dumps(summaries,indent=2)+'\n')
        print(f"{label}: escapes={s['escapes']}/{s['seeds']} mean_floor={s['mean_floor']:.3f} failures=0",flush=True)
    comparisons(args.suite,args.output)


if __name__=='__main__':
    main()
