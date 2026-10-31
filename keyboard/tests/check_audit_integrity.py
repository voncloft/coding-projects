#!/usr/bin/env python3
"""Read-only evidence/source/export consistency check. Does not authorize fabrication."""
from pathlib import Path
import argparse,csv,hashlib,json,zipfile
R=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads((R/p).read_text())
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 native=read('checks/current-h-native/summary.json');assert native['passed'] and len(native['boards'])==8
 current={};exports=[]
 for row in native['boards']:
  b=row['board'];p=R/'kicad'/b/(b+'.kicad_pcb');h=sha(p);current[b]=h
  assert h==row['pcb_sha256'],b
  assert all(row[k]==0 for k in ['drc_violations','unconnected','parity','erc_violations']),b
  assert sha(R/'board-assemblies'/(b+'.step'))==row['step_sha256'],b
  assert read('manufacturing/'+b+'/board-info.json')['pcb_sha256']==h,b
  fab=R/'manufacturing'/b
  with zipfile.ZipFile(fab/(b+'-gerbers.zip')) as z:
   assert z.testzip() is None,b
   files={p.name:p for p in (fab/'gerbers').iterdir() if p.is_file()}
   assert set(z.namelist())==set(files),b
   assert all(z.read(n)==p.read_bytes() for n,p in files.items()),b
  for p in sorted(fab.rglob('*')):
   if p.is_file():exports.append({'path':str(p.relative_to(R)),'sha256':sha(p)})
 for name in ['strict-rule-audit.json','model-and-rule-audit.json']:
  rows=read('checks/'+name);assert len(rows)==8
  for x in rows:assert x['pcb_sha256']==current[x['board']],(name,x['board'])
 models=read('checks/model-and-rule-audit.json');assert sum(x['model_instances'] for x in models)==937
 assert all(not any(x[k] for k in ['missing_models','non_mounting_without_model','drc_exclusions','erc_exclusions']) for x in models)
 assert all(not any(x[k] for k in ['errors','warnings','unconnected']) for x in read('checks/strict-rule-audit.json'))
 bom=read('checks/bom-cpl-audit.json');assert sum(x['populated'] for x in bom)==829 and all(x['bom_cpl_match'] for x in bom)
 assert read('checks/harness-check.json')['passed']
 for name in ['signal-paths.json','critical-pin-review.json']:
  x=read('checks/'+name);assert x['passed']
  for b,h in x['pcb_hashes'].items():assert h==current[b],name
 fc=sha(R/'enclosure/keyboard-enclosure.FCStd');assembly=read('checks/current-assembly-sources.json');assert assembly['passed'] and assembly['fcstd_sha256']==fc
 for row in assembly['boards']:
  assert row['pcb_sha256']==current[row['board']]
  assert row['step_sha256']==sha(R/'board-assemblies'/(row['board']+'.step'))
 for name in ['full-assembly-collisions.json','support-and-key-travel-check.json','detailed-fit-check.json','aon-mechanical.json']:
  x=read('checks/'+name);assert x['passed'] and x['fcstd_sha256']==fc,name
 assert read('checks/full-assembly-collisions.json')['pair_count']==171
 assert read('checks/support-and-key-travel-check.json')['keycap_travel']['key_count']==108
 d=read('checks/detailed-fit-check.json');assert len(d['accessory_checks'])==23 and len(d['step_reload'])==26
 f=read('firmware/binaries/manifest.json');sources={};binaries={}
 for n,meta in f['files'].items():
  p=R/'firmware/binaries'/n;assert p.stat().st_size==meta['bytes'] and sha(p)==meta['sha256'],n;binaries[n]=meta['sha256']
 for n,h in f['source_files'].items():
  assert sha(R/'firmware'/n)==h,n;sources[n]=h
 logic=read('checks/logic-current/logic-tests.json');assert logic['passed'] and len(logic['results'])==26
 for n,h in logic['sources'].items():assert sha(R/n)==h,('logic source',n)
 power=read('checks/power-matrix/power-fault-matrix.json');assert power['passed'] and len(power['cases'])==45 and power['source_sha256']==sha(R/'firmware/qmk/power.c')
 basic=read('checks/firmware-current/firmware-tests.json');assert basic['passed'] and len(basic['power_scenarios'])==29 and basic['cross_language_packet_vectors']==771 and basic['corruption_recovery_trials']==400
 for item in read('checks/source-archive-check.json')['archives']:
  assert sha(R/'firmware/upstream'/item['archive'])==item['sha256'],item['archive']
 for x in read('checks/assembly-pdf-check.json')['boards']:
  assert x['pcb_sha256']==current[x['board']] and x['pdf_sha256']==sha(R/'manufacturing'/x['board']/'assembly.pdf')
 with (R/'assembly/acceptance-record.csv').open(newline='') as f:acceptance=list(csv.DictReader(f))
 assert len(acceptance)==26 and all(x['Status']=='NOT TESTED' for x in acceptance)
 ods=R/'bom/keyboard-parts-rev-h-audit.ods'
 with zipfile.ZipFile(ods) as z:assert z.testzip() is None
 assert read('checks/ods-check.json')['passed'] and read('checks/ods-check.json')['pcb_positions']==829
 result={'passed':True,'status':'Engineering audit consistency passed. Manufacturing HOLD; no physical acceptance passed.','scope':'Hash, count and report correspondence; eight Gerber ZIP CRC/content comparisons. No new electrical or physical qualification.','pcb_sha256':current,'fcstd_sha256':fc,'source_sha256':sources,'binary_sha256':binaries,'ods_sha256':sha(ods),'exports':exports,'acceptance_rows_not_tested':26,'manufacturing_ready':False,'release_zip_issued':False}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
 print('PASS: current audit files agree. 8 boards; 829 positions; 937 models; 26 acceptance rows NOT TESTED. Manufacturing HOLD.')
if __name__=='__main__':main()
