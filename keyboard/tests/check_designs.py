#!/usr/bin/env python3
"""Re-run KiCad DRC/ERC and portable model checks without changing designs."""
from pathlib import Path
import argparse,concurrent.futures,subprocess,json,hashlib
import pcbnew as P
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);args=ap.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
 def check(bp):
  name=bp.stem;records={}
  for mode,suffix,cmd in [('drc','.kicad_pcb',['pcb','drc','--schematic-parity']),('erc','.kicad_sch',['sch','erc'])]:
   report=out/(name+'-'+mode+'.json')
   p=subprocess.run(['kicad-cli',*cmd,'--format','json','-o',str(report),str(bp.with_suffix(suffix))],capture_output=True,text=True)
   (out/(name+'-'+mode+'.log')).write_text(p.stdout+p.stderr)
   if p.returncode:raise RuntimeError((name,mode,p.returncode))
   raw=json.loads(report.read_text())
   if mode=='drc':records.update(drc=len(raw['violations']),unrouted=len(raw['unconnected_items']),parity=len(raw['schematic_parity']))
   else:records['erc']=sum(len(s.get('violations',[])) for s in raw.get('sheets',[]))
  board=P.LoadBoard(str(bp));models=0;missing=[]
  for f in board.GetFootprints():
   for m in f.Models():
    models+=1
    if not Path(m.m_Filename.replace('${KIPRJMOD}',str(bp.parent))).is_file():missing.append([f.GetReference(),m.m_Filename])
  records.update(board=name,models=models,missing_models=missing,pcb_sha256=hashlib.sha256(bp.read_bytes()).hexdigest())
  assert not any(records[k] for k in ['drc','unrouted','parity','erc','missing_models']),records
  print(name,'PASS',flush=True);return records
 boards=sorted((ROOT/'kicad').glob('*/*.kicad_pcb'));assert len(boards)==8
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(check,boards))
 (out/'design-check-summary.json').write_text(json.dumps({'passed':True,'boards':results,'scope':'Native KiCad and file resolution only; no electrical hardware'},indent=2))
if __name__=='__main__':main()
