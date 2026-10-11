"""Descriptive same-seed policy comparison; never a same-policy gameplay A/B.

Standard library only. Retains executable/config snapshots, raw CSV seals,
conditional floor survival, paired depth/death discordances and selected traces.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import time


def rows(path):
    with path.open(newline='') as stream:
        return list(csv.DictReader(stream))


def seal(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def summarize(directory):
    runs = rows(directory / 'runs.csv')
    floors = rows(directory / 'floors.csv')
    exploration = rows(directory / 'exploration.csv')
    items = rows(directory / 'items.csv')
    result = {
        'n': len(runs), 'escapes': sum(r['result'] == 'escaped' for r in runs),
        'failures': [r for r in runs if r['result'].startswith('SIM_')],
        'phantom_deaths': sum(r['death_cause'] == 'PHANTOM' for r in runs),
        'starvation': sum(r['death_cause'] == 'starvation' for r in runs),
        'mean_depth': statistics.mean(int(r['deepest_floor']) + 1 for r in runs),
        'median_depth': statistics.median(int(r['deepest_floor']) + 1 for r in runs),
        'mean_actions': statistics.mean(int(r['actions']) for r in runs),
        'mean_turns': statistics.mean(int(r['turns']) for r in runs),
        'floor_survival': {}, 'exploration_totals': {}, 'resources': {},
    }
    for direction in ('descent', 'ascent'):
        for floor in range(16):
            visits = [r for r in floors if r['direction'] == direction and int(r['floor']) == floor]
            if visits:
                result['floor_survival'][f'{direction}:{floor + 1}'] = {
                    'entered': len(visits), 'exited': sum(int(r['exited']) for r in visits)}
    for field in exploration[0] if exploration else []:
        if field.startswith('unseen_') or field in ('combat_turns', 'exploring_turns', 'travel', 'repeated_visits', 'phantom_deaths'):
            values = [int(r[field]) for r in exploration]
            result['exploration_totals'][field] = max(values) if field == 'unseen_max_consecutive' else sum(values)
    for item in ('HEALING', 'EXPERIENCE', 'FOOD', 'SCROLL_TELEPORT', 'SCROLL_FEAR'):
        matches = [r for r in items if r['item'] == item]
        result['resources'][item] = sum(int(r['consumed']) for r in matches)
    result['manifest'] = json.loads((directory / 'manifest.json').read_text())
    result['seals'] = {p.name: seal(p) for p in directory.glob('*.csv')}
    return result


def paired(control, treatment):
    a = {r['seed']: r for r in rows(control / 'runs.csv')}
    b = {r['seed']: r for r in rows(treatment / 'runs.csv')}
    assert a.keys() == b.keys()
    delta = [int(b[s]['deepest_floor']) - int(a[s]['deepest_floor']) for s in a]
    return {'mean_depth_difference': statistics.mean(delta),
            'depth_gains': sum(d > 0 for d in delta), 'depth_losses': sum(d < 0 for d in delta),
            'phantom_deaths_avoided': [s for s in a if a[s]['death_cause'] == 'PHANTOM' and b[s]['death_cause'] != 'PHANTOM'],
            'new_phantom_deaths': [s for s in a if a[s]['death_cause'] != 'PHANTOM' and b[s]['death_cause'] == 'PHANTOM'],
            'escape_gains': [s for s in a if a[s]['result'] != 'escaped' and b[s]['result'] == 'escaped'],
            'escape_losses': [s for s in a if a[s]['result'] == 'escaped' and b[s]['result'] != 'escaped']}


def run(exe, directory, args):
    started = time.monotonic()
    with directory.with_suffix('.stdout').open('w') as out, directory.with_suffix('.stderr').open('w') as err:
        subprocess.run([str(exe), *args, '--max-actions', '50000', '--output', str(directory)],
                       stdout=out, stderr=err, check=True)
    print(f'{directory.name}: {time.monotonic() - started:.1f}s', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulator', type=Path, required=True)
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--seeds', default='1:1000')
    parser.add_argument('--jobs', default='8')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--traces', action='store_true')
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    binary = root / 'bin'; binary.mkdir()
    exe = binary / args.simulator.name
    shutil.copy2(args.simulator, exe)
    for dll in args.simulator.parent.glob('*.dll'):
        shutil.copy2(dll, binary / dll.name)
    subprocess.run([str(exe), '--write-agent-defaults', str(binary / 'defaults.json')], check=True)
    defaults = json.loads((binary / 'defaults.json').read_text())
    variants = {'legacy': {'contact_aware': 0}, 'disabled': {'contact_aware': 0, 'invisible_response': 0},
                'contact': {'contact_aware': 1}}
    for name, changes in variants.items():
        params = defaults | changes
        (binary / f'{name}.json').write_text(json.dumps(params, indent=2) + '\n')
        run(exe, root / name, ['--agent', 'explorer-v1', '--agent-params', str(binary / f'{name}.json'),
                              '--seeds', args.seeds, '--jobs', args.jobs, '--entry-state'])
    summary = {name: summarize(root / name) for name in variants}
    summary['comparisons'] = {name: paired(root / name, root / 'contact') for name in ('legacy', 'disabled')}
    summary['kind'] = 'descriptive policy comparison; distinct policy hashes, fixed production mechanics'
    summary['executable_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
    if args.reference:
        for policy in ('explorer-v1', 'omniscient-v2'):
            extra = ['--agent-params', str(binary / 'legacy.json')] if policy == 'explorer-v1' else []
            run(exe, root / f'compat-{policy}', ['--agent', policy, '--seeds', '1:32', *extra])
            run(args.reference.resolve(), root / f'frozen-{policy}', ['--agent', policy, '--seeds', '1:32'])
            current = rows(root / f'compat-{policy}' / 'runs.csv'); frozen = rows(root / f'frozen-{policy}' / 'runs.csv')
            assert current == frozen, f'{policy} frozen behavior changed'
            summary[f'{policy}_frozen_parity'] = True
    if args.traces:
        comparison = summary['comparisons']['legacy']
        seeds = comparison['phantom_deaths_avoided'][:2] + comparison['new_phantom_deaths'][:2]
        for seed in seeds:
            for name in variants:
                run(exe, root / f'trace-{name}-{seed}', ['--agent', 'explorer-v1', '--agent-params', str(binary / f'{name}.json'), '--seed', seed, '--trace'])
        # Previously recorded replenishment loop, evaluated without tuning to it.
        for name in variants:
            run(exe, root / f'summon-{name}-9145', ['--agent', 'explorer-v1', '--agent-params', str(binary / f'{name}.json'), '--seed', '9145', '--trace', '--intervention', 'full-light'])
        summary['trace_seeds'] = seeds
        summary['summoning_loop'] = {name: rows(root / f'summon-{name}-9145' / 'runs.csv')[0] for name in variants}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    for name in variants:
        s = summary[name]
        print(name, 'depth', s['mean_depth'], 'phantom deaths', s['phantom_deaths'], 'escapes', s['escapes'], 'failures', len(s['failures']), flush=True)
    assert not any(summary[name]['failures'] for name in variants), 'Simulator failures invalidate this comparison'


if __name__ == '__main__':
    main()
