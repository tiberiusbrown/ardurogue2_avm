"""Compare real AVM floor generation with the native oracle, including scratch.

No firmware/debug state is added. LLDB sets only the floor identity at the
production make_floor entry, then runs its actual code to return.
"""

import argparse
import json
from pathlib import Path
import subprocess
import sys


def quote(path):
    return '"' + str(path).replace('\\', '/').replace('"', '\\"') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seed', type=lambda s: int(s, 0), default=0x1234)
    parser.add_argument('--floor', type=int, default=0)
    parser.add_argument('--ascent', action='store_true')
    parser.add_argument('--profile', action='store_true')
    args = parser.parse_args()
    if not 0 <= args.seed <= 65535 or not 0 <= args.floor < 16:
        parser.error('seed must be 0..65535 and floor 0..15')
    args.output.mkdir(parents=True, exist_ok=True)
    state = 'ascent' if args.ascent else 'descent'
    folder = args.output / f'{args.seed:04x}-{args.floor}-{state}'
    folder.mkdir(exist_ok=False)
    native = folder / 'native.bin'
    subprocess.run([str(args.native.resolve()), '--floor-state', str(args.seed),
                    str(args.floor), state, str(native.resolve())], check=True)
    fields = ['walls', 'explored', 'doors', 'monsters', 'ground', 'player', 'up', 'down', 'door_count']
    root = Path(__file__).resolve().parents[1]
    idle_line = next(i for i, line in enumerate((root / 'src/main.cpp').read_text().splitlines(), 1)
                     if 'avm_idle();' in line)
    commands = [
        f'breakpoint set --file main.cpp --line {idle_line}', 'run',
        'breakpoint disable 1', 'breakpoint set --name rogue::make_floor',
        'avm button set A', 'avm run-for 1s',
        f'expr -- (unsigned int)(rogue::game.run_seed = {args.seed})',
        f'expr -- (unsigned int)(rogue::game.floor = {args.floor})',
        f'expr -- (unsigned int)(rogue::game.has_amulet = {int(args.ascent)})',
        'expr -- (unsigned int)(rogue::game.random_state = 0x51ad)',
        'breakpoint disable 2', 'avm time', 'thread step-out', 'avm time',
        'expr -- (unsigned int)(rogue::game.random_state)',
    ]
    for field in fields + ['random_state']:
        commands.append(f'memory read --binary --outfile {quote((folder / (field + ".bin")).resolve())} '
                        f'--size 1 --count `sizeof(rogue::game.{field})` '
                        f'`(unsigned int)&rogue::game.{field} + 0x01000000`')
    if args.profile:
        index = commands.index('thread step-out')
        commands.insert(index, 'avm profile start')
        commands.insert(index + 2, 'avm profile stop')
        commands.insert(index + 3, f'avm profile save {quote((folder / "generation.avmp").resolve())}')
    script = folder / 'generation.lldb'
    script.write_text('\n'.join(commands) + '\n')
    suffix = '.exe' if sys.platform == 'win32' else ''
    result = subprocess.run([str((args.sdk_root / 'bin' / ('avm-lldb' + suffix)).resolve()),
                             '--batch', '--source', str(script.resolve()), str(args.elf.resolve())],
                            capture_output=True, text=True, timeout=180)
    (folder / 'lldb.txt').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    expected = native.read_bytes()
    actual = b''.join((folder / (field + '.bin')).read_bytes() for field in fields)
    if (folder / 'random_state.bin').read_bytes() != b'\xad\x51':
        raise RuntimeError('production generation consumed gameplay randomness')
    if actual != expected:
        offset = next(i for i, pair in enumerate(zip(expected, actual)) if pair[0] != pair[1]) if len(expected) == len(actual) else -1
        raise RuntimeError(f'AVM/native generation mismatch at byte {offset}; see {folder}')
    records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{') and line.endswith('}')]
    times = [record for record in records if 'seconds_since_reset' in record]
    if len(times) != 2:
        raise RuntimeError('missing generation timing boundaries')
    cycles = times[1]['cycles'] - times[0]['cycles']
    summary = dict(seed=args.seed, floor=args.floor, ascent=args.ascent, bytes=len(actual),
                   cycles=cycles, milliseconds=cycles / 16000, matches=True)
    (folder / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
