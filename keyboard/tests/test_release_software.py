import sys,tempfile,unittest,json,threading,time,subprocess,os
from pathlib import Path
from unittest.mock import patch
SRC=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(SRC/'software'))
import install_target as install
from keyboard_ui.app import Controller
from keyboard_ui.protocol import Kind,Packet
class Tests(unittest.TestCase):
 def test_preview_isolation(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'busy').mkdir();(p/'busy/file').write_text('preserve');(p/'link').symlink_to(p/'busy')
   for bad in [Path('/'),p/'busy',p/'link',p/'link/new']:
    with self.assertRaises(ValueError):install.preview_root(bad)
   self.assertEqual(install.preview_root(p/'new'),p/'new')
   units=install.prepare(p/'new','keyboard')
   self.assertEqual(len(units),5)
   self.assertEqual((p/'busy/file').read_text(),'preserve')
   self.assertTrue((p/'new/home/keyboard/.config/systemd/user/default.target.wants/pipewire.socket').is_symlink())
 def test_boot_preflight_idempotence(self):
  with self.assertRaises(ValueError):install.boot_text('dtoverlay=vc4-fkms-v3d\n')
  text=install.boot_text('[all]\ninclude keyboard-revf.txt\n')
  self.assertIn('dtoverlay=vc4-kms-v3d',text);self.assertEqual(install.boot_text(text),text)
  self.assertEqual(text.count('include keyboard-revg.txt'),1);self.assertNotIn('include keyboard-revf.txt',text)
 def test_desktop_install_refusal(self):
  # The host has no CM4 model. This test executes only the refusal path.
  
  if Path('/proc/device-tree/model').exists():self.skipTest('Desktop refusal test is only for non-CM4 test hosts')
  p=subprocess.run([sys.executable,str(SRC/'software/install_target.py'),'--apply-on-cm4','--desktop-user','keyboard'],capture_output=True,text=True)
  self.assertNotEqual(p.returncode,0);self.assertIn('not the keyboard CM4',p.stderr)
 def controller(self,d):return Controller(d,simulate=True)
 def test_queued_host_switch(self):
  with tempfile.TemporaryDirectory() as d,patch.object(threading.Thread,'start'):
   c=self.controller(d);epoch=c.host_epoch
   c.action('host',{'slot':2})
   self.assertTrue(c.cancel.is_set());c.cancel.clear()
   with self.assertRaisesRegex(ValueError,'Host changed'):c.run_steps([{'type':'shortcut','value':'CTRL+C'}],epoch)
 def test_queued_cancel_race(self):
  with tempfile.TemporaryDirectory() as d,patch.object(threading.Thread,'start'):
   c=self.controller(d);c.submit('macro',[{'type':'shortcut','value':'CTRL+C'}]);job=c.jobs.get_nowait();c.jobs.task_done()
   c.action('cancel',{});self.assertNotEqual(job[3],c.cancel_epoch)
   c.jobs.put_nowait(job)
   worker=threading.Thread(target=c.worker,daemon=True)
  # Start after leaving the patch; no serial or network operations in simulation.
  worker.start();c.jobs.join()
  self.assertEqual(c.state['message'],'Queued action cancelled')
  self.assertFalse(any(x.get('command')==int(Kind.TAP) for x in c.events))
 def test_ir_save_requires_test_and_confirmation(self):
  with tempfile.TemporaryDirectory() as d:
   c=self.controller(d);c.action('ir-import',{'frequency':38000,'pulses':[9000,4500,560,560]})
   with self.assertRaises(ValueError):c.action('ir-save',{'name':'Projector off','worked':True})
   c.action('ir-test',{});c.jobs.join()
   with self.assertRaises(ValueError):c.action('ir-save',{'name':'Projector off','worked':False})
   c.action('ir-save',{'name':'Projector off','worked':True})
   self.assertIn('Projector off',json.loads((Path(d)/'remotes.json').read_text()))
 def test_network_disabled_in_simulation(self):
  with tempfile.TemporaryDirectory() as d:
   c=self.controller(d);c.home={'url':'http://example.invalid','token':'fixture'}
   with patch('keyboard_ui.app.urlopen',side_effect=AssertionError('network attempted')):
    with self.assertRaisesRegex(ValueError,'disabled'):c.home_request('states')
 def test_ack_failure_stops_text(self):
  with tempfile.TemporaryDirectory() as d,patch.object(threading.Thread,'start'):
   c=self.controller(d)
   with patch.object(c,'send',side_effect=ValueError('Selected computer is not connected')) as send:
    with self.assertRaises(ValueError):c.run_steps([{'type':'text','value':'abc'}])
    self.assertEqual(send.call_count,1)
if __name__=='__main__':unittest.main(verbosity=2)
