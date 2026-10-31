#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,os,json,hashlib
R=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True);exe=out/'power-fault-matrix'
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(R/'tests/stubs'),str(R/'tests/power_fault_matrix.c'),'-o',str(exe)],check=True)
cases=[]
for phase,kind,addr,regs in [
 ('initial','read',0x6b,[0x48,0x0a,0x0f,0x01,0x03,0x06,0x09,0x10,0x11,0x14]),
 ('initial','write',0x6b,[0x0f,0x01,0x03,0x06,0x09,0x10,0x11,0x14,0x2f,0x30,0x2e]),
 ('initial','write',0x20,[1,3]),('initial','read',8,[0,1]),
 ('runtime','read',0x6b,[0x3b,0x35,0x33,0x3f,0x20,0x21,0x1d,0x10,0x06,0x0f,0x2e]),
 ('runtime','read',0x36,[4]),('runtime','read',0x20,[0]),('runtime','read',8,[0,1]),
 ('runtime','write',0x6b,[0x06,0x10,0x0f,0x2e]),('runtime','write',0x20,[1])]:
 for reg in regs:cases.append([phase,kind,str(addr),str(reg)])
rows=[];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
for args in cases:
 p=subprocess.run([str(exe),*args],capture_output=True,text=True,timeout=10,env=env);rows.append(dict(case=args,exit=p.returncode,output=p.stdout+p.stderr));print(' '.join(args),p.returncode,flush=True)
report=dict(passed=all(r['exit']==0 for r in rows),cases=rows,source_sha256=hashlib.sha256((R/'firmware/qmk/power.c').read_bytes()).hexdigest(),scope='45 individually addressed persistent I2C read/write faults, at initial configuration and while running, against real power.c. PD status failures deliberately fall back to battery; charger/gauge/expander failures must inhibit startup or shut down. Address/undefined behavior sanitized; no physical hardware.')
(out/'power-fault-matrix.json').write_text(json.dumps(report,indent=2));sys.exit(0 if report['passed'] else 1)
