#!/usr/bin/env python3
"""Isolated processes make one stuck modal/control failure visible, not a lost suite."""
import pathlib,subprocess,sys
binary=pathlib.Path(sys.argv[1]).resolve()
failed=[]
cases = ['slot-owner-thread','button','keyboard','advanced','axis','dpad','stick','mouse-button','mouse-axis','mouse-dpad','mouse-stick','profile','settings','calibration','about','join-split','autoprofile','throttle','key-display','stick-assignment','quick-set','controller-mapping','profile-operations','sensor-accel','sensor-gyro','calibration-gyro','calibration-accel','calibration-stick-full','advanced-slots','menu-routes','advanced-text-files','main-menu-routes','sensor-zero-vector','sensor-default-preset','sensor-fresh-state']
for name in cases:
    try:
        result=subprocess.run([str(binary),name],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45 if name=="calibration-stick-full" else 15)
        lines=[line for line in result.stdout.splitlines() if line.startswith(('PASS','FAIL'))]
        if result.returncode or not lines:
            failed.append(name); print(f'FAIL {name}: exit {result.returncode}'); print(result.stdout[-3000:])
        else: print('\n'.join(lines))
    except subprocess.TimeoutExpired as exc:
        failed.append(name); print(f'FAIL {name}: timeout')
        print((exc.stdout or b'')[-1500:].decode(errors='replace'))
print(f'{len(cases)-len(failed)}/{len(cases)} dialog test groups passed')
sys.exit(bool(failed))
