"""Encounter and milestone diagnostics used by the existing competence reports.

Conditional cohorts are selected by each policy. Their survival differences
are descriptive; these measures are neither tuning scores nor policy inputs.
"""
import collections
import csv
import statistics


def rows(path):
    with path.open(newline='') as f:
        yield from csv.DictReader(f)


def exact_interval(k, n):
    from scipy.stats import beta
    if not n:
        return None
    return [float(beta.ppf(.025,k,n-k+1)) if k else 0.,
            float(beta.ppf(.975,k+1,n-k)) if k<n else 1.]


def mean(values):
    return statistics.mean(values) if values else None


def encounter_group(group):
    n=len(group); survived=sum(int(r['survived']) for r in group)
    out=dict(n=n,survived=survived,survival_fraction=survived/n if n else None,
             survival_exact_95=exact_interval(survived,n),deaths_during=n-survived,
             mixed=sum(int(r['mixed']) for r in group),known_trap=sum(int(r['known_trap']) for r in group),
             summoning=sum(int(r['summoning']) for r in group),killed=sum(int(r['phantom_kills'])>0 for r in group),
             outcomes=dict(collections.Counter(r['outcome'] for r in group)))
    out['player_killed_phantom']=sum(int(r['phantom_player_kills'])>0 for r in group) if all('phantom_player_kills' in r for r in group) else None
    for field in ('damage','healing_consumed','escape_consumed','food_consumed','combat_turns','movement_turns','actions'):
        v=[int(r[field]) for r in group]
        out[field]=dict(mean=mean(v),median=statistics.median(v) if v else None,total=sum(v))
    # Mixed episodes may begin before the phantom joins. Keep whole-episode
    # costs above and expose the costs from actual phantom contact separately.
    out['from_phantom_contact']={}
    for field in ('damage','healing_consumed','escape_consumed'):
        v=[int(r.get('phantom_'+field,r[field])) for r in group]
        out['from_phantom_contact'][field]=dict(mean=mean(v),median=statistics.median(v) if v else None,total=sum(v))
    v=[int(r['phantom_start_hp'] if r['phantom']=='1' else r['start_hp'])-int(r['exit_hp']) for r in group]
    out['net_hp_loss']=dict(mean=mean(v),median=statistics.median(v) if v else None)
    for h in (25,50,100):
        v=[int(r[f'survived_{h}']) for r in group if int(r[f'survived_{h}'])>=0]
        out[f'survived_{h}']=dict(observed=len(v),censored=n-len(v),survived=sum(v),fraction=mean(v),exact_95=exact_interval(sum(v),len(v)))
    out['reached_next_floor']=sum(int(r['next_floor']) for r in group)
    out['deaths_within_25_after']=sum(r['survived']=='1' and r['survived_25']=='0' for r in group)
    return out


MILESTONES=['floor_1_reached','floor_1_complete',*[f'floor_{i}_reached' for i in (3,5,8,10,12,15)],
            'lord_encountered','lord_defeated','yendor','ascent_started',
            *[f'ascent_floor_{i}_complete' for i in range(15,0,-1)],'escaped']
NAV_FIELDS=('known_trap_crossings','repeated_known_trap_crossings','hazard_following_damage','safe_detours',
            'hazard_objectives_abandoned','essential_hazard_crossings','no_safe_routes','movement_reversals',
            'objective_changes','goal_failures','navigation_cycles','exploring_turns','combat_turns','travel',
            'repeated_visits','unreachable_attempts','frontier_changes')


def diagnostics(path,runs):
    if not (path/'encounters.csv').exists():
        return None
    first=[]; known_hazard=[]; categories=collections.defaultdict(collections.Counter)
    species=collections.defaultdict(collections.Counter)
    for r in rows(path/'encounters.csv'):
        group='mixed_phantom' if r['phantom']=='1' and r['mixed']=='1' else 'phantom' if r['phantom']=='1' else 'other'
        c=categories[group]; c['encounters']+=1
        for type_id in range(1,17):
            if int(r.get('species_mask',0)) & (1<<type_id):
                s=species[type_id]; s['encounters']+=1; s['survived']+=int(r['survived']); s['mixed']+=int(r['mixed'])
        for k in ('survived','damage','healing_consumed','escape_consumed','combat_turns','movement_turns','kills'):
            c[k]+=int(r[k])
        if r['first_phantom']=='1': first.append(r)
        if r['known_trap']=='1': known_hazard.append(r)
    if len({r['effective_seed'] for r in first})!=len(first): raise ValueError('duplicate first phantom')
    breakdown={}
    for field in ('floor','weapon','armor','offhand','light','phantom_visible_first'):
        groups=collections.defaultdict(list)
        for r in first: groups[r.get('phantom_'+field,r.get(field,'unrecorded'))].append(r)
        breakdown[field]={k:encounter_group(v) for k,v in groups.items()}
    hp=collections.defaultdict(list)
    for r in first:
        percent=100*int(r['phantom_start_hp'])/max(1,int(r['phantom_max_hp']))
        hp['below_25' if percent<25 else '25_to_49' if percent<50 else '50_to_74' if percent<75 else '75_to_100'].append(r)
    breakdown['starting_hp_percent']={k:encounter_group(v) for k,v in hp.items()}
    entered=collections.defaultdict(dict)
    for r in rows(path/'milestones.csv'):
        if r['effective_seed'] in entered[r['milestone']]: raise ValueError('duplicate milestone')
        entered[r['milestone']][r['effective_seed']]=r
    milestones={}
    for i,name in enumerate(MILESTONES):
        group=list(entered[name].values()); n=len(group)
        next_name=MILESTONES[i+1] if i+1<len(MILESTONES) else None
        both=len(entered[name].keys() & entered[next_name].keys()) if next_name else None
        item=dict(reached=n,percent=100*n/len(runs),next=next_name,next_reached=both,
                  conditional_to_next=both/n if n and next_name else None)
        for field in ('hp','max_hp','level','armor','weapon','weapon_enchant','offhand','food','healing','escape','actions','turns'):
            if field in ('weapon','offhand'): item[field]=dict(collections.Counter(r[field] for r in group))
            else: item[f'mean_{field}']=mean([int(r[field]) for r in group])
        milestones[name]=item
    nav=collections.Counter(); stretch=[]; discovered=[]; reached=[]
    for r in rows(path/'exploration.csv'):
        for k in NAV_FIELDS: nav[k]+=int(r.get(k,0))
        if int(r['shortest_objective_route'])>0: stretch.append(int(r['travel'])/int(r['shortest_objective_route']))
        if int(r['stair_discovery_turn'])>=0: discovered.append(int(r['stair_discovery_turn']))
        if int(r['stair_reached_turn'])>=0: reached.append(int(r['stair_reached_turn']))
    features=collections.Counter()
    for r in rows(path/'features.csv'):
        if int(r['feature_type'])==18: # stable FEATURE_SUMMONING_TRAP telemetry ID
            for k in ('activated','repeated','spawned','monster_activated','monster_spawned'):
                features[k]+=int(r[k])
    return dict(schema=1,first_phantom=encounter_group(first),first_phantom_by=breakdown,
                known_hazard_encounters=encounter_group(known_hazard),summoning=dict(features),
                encounter_categories=dict(categories),monster_categories=dict(species),milestones=milestones,navigation=dict(nav),
                mean_movement_stretch=mean(stretch),mean_stair_discovery=mean(discovered),mean_stair_reached=mean(reached),
                selection_note='Policies select different contact/entry HP, equipment, floors and enemies; conditional survival is descriptive.')


def paired_diagnostics(control,treatment,ar,br):
    import numpy as np
    from scipy.stats import t
    a={int(s):collections.Counter() for s in ar}; b={int(s):collections.Counter() for s in br}
    for path,target in ((control,a),(treatment,b)):
        for r in rows(path/'exploration.csv'):
            for k in NAV_FIELDS: target[int(r['effective_seed'])][k]+=int(r.get(k,0))
        if (path/'encounters.csv').exists():
            for r in rows(path/'encounters.csv'):
                if r['first_phantom']=='1':
                    target[int(r['effective_seed'])]['first_phantom_reached']=1
                    target[int(r['effective_seed'])]['first_phantom_survived']=int(r['survived'])
    with (control/'exploration.csv').open(newline='') as f: ac=set(next(csv.reader(f)))
    with (treatment/'exploration.csv').open(newline='') as f: bc=set(next(csv.reader(f)))
    # These counters instrument candidate-only algorithms; baseline zero does
    # not mean an absence of geometric failures or movement cycles.
    internal={'no_safe_routes','goal_failures','navigation_cycles'}
    keys=[k for k in NAV_FIELDS if k in ac & bc and k not in internal]
    if (control/'encounters.csv').exists() and (treatment/'encounters.csv').exists(): keys+=['first_phantom_reached','first_phantom_survived']
    out={}
    for key in (*keys,'actions','turns'):
        delta=np.array([int(br[s][key])-int(ar[s][key]) if key in ('actions','turns') else b[int(s)][key]-a[int(s)][key] for s in ar],dtype=float)
        avg=float(delta.mean()); se=float(delta.std(ddof=1)/np.sqrt(len(delta))) if len(delta)>1 else 0.
        margin=float(t.ppf(.975,len(delta)-1))*se if len(delta)>1 else 0.
        out[key]=dict(mean_difference=avg,paired_mean_95=[avg-margin,avg+margin],
                      method='paired Student mean interval for common diagnostics; rare escapes use exact intervals')
    return out
