#!/usr/bin/env python3
"""Isolated processes make one stuck modal/control failure visible, not a lost suite."""
import pathlib,subprocess,sys
binary=pathlib.Path(sys.argv[1]).resolve()
failed=[]
for name in ['button','keyboard','advanced','axis','dpad','stick','mouse-button','mouse-axis','mouse-dpad','mouse-stick','profile','settings','calibration','about','join-split','autoprofile','throttle','key-display','stick-assignment','quick-set','controller-mapping','profile-operations']:
    try:
        result=subprocess.run([str(binary),name],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=15)
        lines=[line for line in result.stdout.splitlines() if line.startswith(('PASS','FAIL'))]
        if result.returncode or not lines:
            failed.append(name); print(f'FAIL {name}: exit {result.returncode}'); print(result.stdout[-3000:])
        else: print('\n'.join(lines))
    except subprocess.TimeoutExpired as exc:
        failed.append(name); print(f'FAIL {name}: 15-second timeout')
        print((exc.stdout or b'')[-1500:].decode(errors='replace'))
print(f'{22-len(failed)}/22 dialog test groups passed')
sys.exit(bool(failed))
