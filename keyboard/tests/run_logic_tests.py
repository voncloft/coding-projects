#!/usr/bin/env python3
"""Compile and exercise the shipped QMK/ESP sources with mocked hardware APIs.

This is desktop logic verification; it does not qualify electrical or radio hardware.
Usage: python3 tests/run_logic_tests.py /path/to/test-output
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
R=Path(__file__).resolve().parents[1]
OUT=Path(sys.argv[1]).resolve();OUT.mkdir(parents=True,exist_ok=True)
CASES={
 'qmk':['release-during-off','host-at-timer-zero','queued-encoder-steps','banks-and-keymap','tap-gates','encoder-hold-wrap','host-short-and-wrap','rgb-cap'],
 'esp':['sequence-255','soc-all-values','soc-stale-at-zero','soc-wrap','ir-cancel-upload-on-learn','ir-malformed-learn','ir-malformed-commit','ir-upload-boundaries','ir-incomplete-and-offset','ir-duration-and-driver-faults','ir-learn-valid','ir-learn-short-pulse','ble-host-isolation','ble-stale-auth-epoch','ble-passkey','ble-overflow-and-timeout','ble-report-timeout','encoder-and-debounce']}
report={'scope':'Actual firmware source with mocked hardware APIs. Address/undefined behavior sanitizers enabled; LeakSanitizer disabled because sandbox ptrace is unavailable. No radio or electrical hardware is exercised.','sources':{},'results':[],'passed':False}
for directory in ['firmware/qmk','firmware/esp32','tests/qmk_logic','tests/esp_logic']:
 for p in sorted((R/directory).rglob('*')):
  if p.is_file():report['sources'][str(p.relative_to(R))]=hashlib.sha256(p.read_bytes()).hexdigest()
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
for family,cases in CASES.items():
 base=R/'tests'/f'{family}_logic';exe=OUT/f'{family}-logic'
 flags=['-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(base/'stubs')]
 if family=='qmk':cmd=['cc','-std=c11',*flags,'-I'+str(R/'tests/stubs'),str(base/'harness.c'),'-o',str(exe)]
 else:cmd=['c++','-std=c++17',*flags,'-I'+str(R/'firmware/esp32/include'),str(base/'harness.cpp'),'-o',str(exe)]
 build=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(OUT/f'{family}-build.log').write_text(build.stdout+build.stderr);build.check_returncode()
 for case in cases:
  run=subprocess.run([str(exe),case],capture_output=True,text=True,timeout=15,env=env)
  row={'family':family,'case':case,'exit':run.returncode,'output':run.stdout+run.stderr};report['results'].append(row);print(f'{family}/{case}: {"PASS" if run.returncode==0 else "FAIL"}',flush=True)
report['passed']=all(r['exit']==0 for r in report['results']);(OUT/'logic-tests.json').write_text(json.dumps(report,indent=2));sys.exit(0 if report['passed'] else 1)
