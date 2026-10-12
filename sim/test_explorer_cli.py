"""Explorer selection, isolation, worker forwarding and balance policy identity."""
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile

def main():
    exe=sys.argv[1]
    def invoke(*args, valid=True):
        p=subprocess.run([exe,*args],capture_output=True,text=True)
        assert (p.returncode==0)==valid,(args,p.stderr)
        return p
    invoke('--agent','unknown',valid=False)
    invoke('--agent',valid=False)
    default=invoke('--seed','42')
    assert default.stdout==invoke('--agent','omniscient-v2','--seed','42').stdout
    invoke('--intervention','light-awareness','--intervention','legacy-awareness',valid=False)
    quiet=invoke('--agent','explorer-v1','--seed','42')
    assert quiet.stdout==invoke('--agent','explorer-v1','--seed','42','--trace').stdout
    row=list(csv.DictReader(io.StringIO(quiet.stdout)))[0]
    assert row['agent']=='explorer-v1' and row['result'] in ('death','escaped')
    disabled=list(csv.DictReader(io.StringIO(invoke('--agent','explorer-v1','--seed','42','--no-telemetry').stdout)))[0]
    assert all(row[k]==disabled[k] for k in ('result','actions','turns','action_hash','score','hp','level'))
    bottleneck=list(csv.DictReader(io.StringIO(invoke('--agent','explorer-v1','--seed','9145','--intervention','full-light','--max-actions','50000').stdout)))[0]
    assert bottleneck['result'] in ('death','escaped'),bottleneck
    limited=invoke('--agent','explorer-v1','--seeds','0:4','--max-actions','1')
    assert limited.stdout==invoke('--agent','explorer-v1','--seeds','0:4','--max-actions','1','--jobs','3').stdout
    assert all(r['result']=='SIM_STUCK' and not r['death_cause'] for r in csv.DictReader(io.StringIO(limited.stdout)))
    with tempfile.TemporaryDirectory(prefix='explorer-cli-') as temp:
        root=Path(temp); serial=root/'serial'; parallel=root/'parallel'; omni=root/'oracle'
        config=root/'params.json'; config.write_text('{"frontier_adjacent_weight": 140, "shield_contact_bonus": 250}')
        defaults=root/'defaults.json'
        invoke('--write-agent-defaults',str(defaults))
        assert len(json.loads(defaults.read_text()))>=50
        invoke('--agent-params',str(config),valid=False)
        invalid=root/'invalid.json'; invalid.write_text('{"healing_percent": 999}')
        invoke('--agent','explorer-v1','--agent-params',str(invalid),valid=False)
        common=('--agent','explorer-v1','--agent-params',str(config),'--seeds','0:8','--entry-state')
        invoke(*common,'--output',str(serial))
        invoke(*common,'--jobs','3','--output',str(parallel))
        invoke('--agent','omniscient-v2','--seed','42','--output',str(omni))
        for file in serial.glob('*.csv'):
            assert file.read_bytes()==(parallel/file.name).read_bytes(),file.name
            assert all(r['agent']=='explorer-v1' for r in csv.DictReader(file.open(newline='')))
        a=json.loads((serial/'manifest.json').read_text()); b=json.loads((omni/'manifest.json').read_text())
        assert a['agent']=='explorer-v1' and b['agent']=='omniscient-v2'
        assert len(a['agent_policy_hash'])==64 and a['agent_policy_hash']!=b['agent_policy_hash']
        assert a['observation_interface_version']==5 and len(a['observation_interface_hash'])==64
        assert a['exploration_schema_version']==6 and a['survival_schema_version']==2 and a['effective_seed_count']==9
        assert Path(a['executable']).is_file()
        assert a['agent_parameters']['shield_contact_bonus']==250
        canonical=json.dumps(a['agent_parameters'],indent=2)+'\n'
        assert hashlib.sha256(canonical.encode()).hexdigest()==a['agent_parameter_hash']
        expected=hashlib.sha256((a['agent_code_hash']+':'+a['agent_parameter_hash']).encode()).hexdigest()
        assert expected==a['agent_policy_hash']
        fresh=root/'fresh'
        invoke('--agent','explorer-v1','--seed','42','--output',str(fresh))
        assert json.loads((fresh/'manifest.json').read_text())['agent_policy_hash']!=a['agent_policy_hash']
        aware_serial=root/'aware-serial'; aware_parallel=root/'aware-parallel'
        invoke('--agent','explorer-v1','--seeds','1:8','--intervention','light-awareness','--output',str(aware_serial))
        invoke('--agent','explorer-v1','--seeds','1:8','--intervention','light-awareness','--jobs','3','--output',str(aware_parallel))
        for file in aware_serial.glob('*.csv'):
            assert file.read_bytes()==(aware_parallel/file.name).read_bytes(),file.name
        # Expanded ground/mimic handles and new item/effect counters must merge
        # deterministically across workers using the same frozen policy.
        sight_serial=root/'sight-serial'; sight_parallel=root/'sight-parallel'
        sight=('--agent','explorer-v1','--seeds','1:16','--entry-state',
               '--intervention','balance:ground=24','--intervention','balance:inventory=20',
               '--intervention','balance:sight-percent=100','--intervention','balance:sight-duration=20')
        invoke(*sight,'--output',str(sight_serial))
        invoke(*sight,'--jobs','3','--output',str(sight_parallel))
        for file in sight_serial.glob('*.csv'):
            assert file.read_bytes()==(sight_parallel/file.name).read_bytes(),file.name
        rows=list(csv.DictReader((sight_serial/'items.csv').open(newline='')))
        assert any(r['item']=='SEE_INVISIBLE' and int(r['generated']) for r in rows)
        assert all('identified' in r and 'expired' in r and 'active_turns' in r for r in rows)
    print('Explorer CLI and worker checks passed')

if __name__=='__main__': main()
