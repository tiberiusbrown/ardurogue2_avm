"""Host-only reconciliation and descriptive bow telemetry, never causal ranking."""
import numpy as np
import pandas as pd


def reconcile(data):
    r = data['ranged'].set_index('effective_seed')
    if r.index.duplicated().any() or set(r.index) != set(data['runs'].effective_seed):
        raise ValueError('missing/duplicate ranged run keys')
    items = data['items']
    ammo = items[items.item.eq('ARROWS')].set_index('effective_seed').reindex(r.index, fill_value=0)
    for field, item_field in [('generated', 'generated'), ('picked', 'picked_up'), ('carried', 'carried')]:
        if not np.array_equal(r[field], ammo[item_field]):
            raise ValueError(f'ranged/item {field} does not reconcile')
    if not (r.fired + r.thrown).eq(ammo.consumed).all():
        raise ValueError('arrow consumption does not reconcile')
    if not ammo.picked_up.eq(ammo.consumed + ammo.dropped + ammo.discarded + ammo.carried).all():
        raise ValueError('arrow inventory conservation failed')
    f = data['floors'].groupby('effective_seed')
    for field, floor_field in [('bundles', 'arrow_bundles'), ('generated', 'arrow_generated'), ('picked', 'arrow_picked')]:
        if not np.array_equal(r[field], f[floor_field].sum().reindex(r.index)):
            raise ValueError(f'ranged/floor {field} does not reconcile')
    if not r.fired.eq(r.short_shots + r.long_shots).all():
        raise ValueError('bow shot types do not reconcile')
    for field in ['shots', 'hits', 'damage', 'kills']:
        total = r[f'short_{field}'] + r[f'long_{field}']
        if not r[[f'distance{d}_{field}' for d in range(7)]].sum(axis=1).eq(total).all():
            raise ValueError(f'bow distance {field} does not reconcile')
        target_cols = [c for c in r if c.endswith('_' + field) and c.split('_')[0].isupper()]
        target_total = r[target_cols].sum(axis=1)
        if not (target_total.le(total).all() if field == 'shots' else target_total.eq(total).all()):
            raise ValueError(f'bow target {field} does not reconcile')


def report(data, output):
    reconcile(data)
    r = data['ranged']
    n = len(r)
    shots = int(r.fired.sum())
    hits = int((r.short_hits + r.long_hits).sum())
    s = {c: float(r[c].mean()) for c in ['bundles', 'generated', 'picked', 'fired', 'thrown', 'carried',
        'bow_turns', 'switches_to', 'switches_away', 'generation_gap', 'acquisition_gap']}
    s.update(hit_pct=100*hits/shots if shots else 0,
        damage_per_run=float((r.short_damage+r.long_damage).mean()),
        kills_per_run=float((r.short_kills+r.long_kills).mean()))
    distances = [int(r[f'distance{d}_shots'].sum()) for d in range(7)]
    s['mean_distance'] = sum(d*count for d, count in enumerate(distances))/shots if shots else 0
    cumulative = np.cumsum(distances)
    # Weighted median of all actual shots, including a distance-zero blocker.
    def at(rank):
        return int(np.searchsorted(cumulative, rank, side='right'))
    s['median_distance'] = (at((shots-1)//2)+at(shots//2))/2 if shots else 0
    for field in ['generation_gap', 'acquisition_gap']:
        s[field+'_ge3_pct'] = float(100*r[field].ge(3).mean())
        s[field+'_ge5_pct'] = float(100*r[field].ge(5).mean())
        s[field+'_p95'] = float(r[field].quantile(.95))
        s[field+'_max'] = int(r[field].max())
    for field in ['first_generated', 'first_picked']:
        found = r.loc[r[field].ne(255), field]
        s[field+'_mean'] = float(found.mean()) if len(found) else None
        s[field+'_median'] = float(found.median()) if len(found) else None
        s[field+'_never_pct'] = float(100*r[field].eq(255).mean())
    text = ['## Ranged combat', '',
        'Descriptive usage under the maintained policy. Acquisition/use association is not causal power; use the matched-policy bow replacement experiment.', '',
        '| Metric | Per starting run / value |', '| --- | --- |']
    text += [f'| {k.replace("_", " ")} | {v:.3f} |' for k, v in s.items() if v is not None]
    text += ['', '| Bow | Generated/run | Picked/run | Equipped/run | Shots/run | Hits % | Damage/run | Kills/run |', '| --- | --- | --- | --- | --- | --- | --- | --- |']
    for bow, label in [('short', 'SHORT_BOW'), ('long', 'LONG_BOW')]:
        item = data['items'].loc[data['items'].item.eq(label)]
        count = int(r[bow+'_shots'].sum())
        vals = [item.generated.sum()/n, item.picked_up.sum()/n, item.equipped.sum()/n,
            count/n, 100*r[bow+'_hits'].sum()/count if count else 0, r[bow+'_damage'].sum()/n, r[bow+'_kills'].sum()/n]
        text.append('| '+label+' | '+' | '.join(f'{v:.3f}' for v in vals)+' |')
    targets=[]
    for c in r:
        if c.endswith('_shots') and c.split('_')[0].isupper():
            t=c[:-6]
            targets.append(dict(target=t, **{field: int(r[t+'_'+field].sum()) for field in ['shots','hits','damage','kills']}))
    text += ['', '| Target | Shots | Hits | Damage | Kills |', '| --- | --- | --- | --- | --- |']
    text += [f'| {t["target"]} | {t["shots"]} | {t["hits"]} | {t["damage"]} | {t["kills"]} |' for t in targets]
    visits = data['floors'].loc[data['floors'].direction.eq('descent')]
    drought = visits.groupby('floor').agg(entered=('visit','size'), bundles=('arrow_bundles','sum'), units=('arrow_generated','sum'), picked=('arrow_picked','sum'))
    drought['bundles_per_reached_floor'] = drought.bundles / drought.entered
    drought['units_per_reached_floor'] = drought.units / drought.entered
    drought.to_csv(output/'ammo_floors.csv')
    pd.DataFrame(targets).to_csv(output/'ranged_targets.csv', index=False)
    pd.DataFrame([s]).to_csv(output/'ranged_summary.csv', index=False)
    text += ['', 'Gaps count consecutive reached descent floors. Generation gaps require no generated bundle. Acquisition gaps require no picked arrows while any bow is owned during the visit, including stocked or cursed bows; they do not by themselves prove unusable inventory. First-floor values exclude never-found (255). Damage is effective HP removed, excluding overkill.', '']
    return '\n'.join(text)
