"""Fail if the independently compiled policy imports production state/functions."""
from pathlib import Path
import re
import subprocess
import sys

policy=Path(__file__).with_name('explorer_agent.cpp').read_text()
protocol=Path(__file__).with_name('explorer.hpp').read_text()
assert re.findall(r'#include "([^"]+)"',policy)==['explorer.hpp','explorer_public_rules.hpp']
assert re.findall(r'#include "([^"]+)"',protocol)==['explorer_parameters.hpp']
parameters=Path(__file__).with_name('explorer_parameters.hpp').read_text()
assert not re.findall(r'#include "([^"]+)"',parameters)
for text in (policy,protocol,parameters):
    # Comments may name the forbidden capabilities; executable code may not.
    code=re.sub(r'//[^\n]*|/\*.*?\*/','',text,flags=re.S)
    assert not re.search(r'\b(Game|DecisionContext|game|session|run_seed|scan_ray|can_see|roll|next_random)\b',code)
public=Path(sys.argv[1]).read_text()
assert not re.search(r'\b(extern|Game|Session|GroundItem)\b',public)
if len(sys.argv)>3:
    symbols=subprocess.check_output([sys.argv[2],'-C','-u',sys.argv[3]],text=True)
    assert 'rogue::' not in symbols, symbols
print('Explorer compilation boundary verified')
