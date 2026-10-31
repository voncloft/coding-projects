import io,contextlib
import sys,tempfile,unittest,json,time,shutil,subprocess
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'software'),str(ROOT/'software/target')]
from keyboard_ui.audio import Audio,pipewire_nodes
from audio_loop import capture_name
from flash_esp32 import images,power_ok
import install_target,flash_esp32

class RevisionG(unittest.TestCase):
 def wait(self,a):
  until=time.monotonic()+2
  while a.busy and time.monotonic()<until:time.sleep(.01)
  self.assertFalse(a.busy)
 def test_simulated_headphones_never_execute(self):
  with patch('subprocess.run',side_effect=AssertionError('Live command attempted')):
   a=Audio(simulate=True);mac=a.devices()[0]['address']
   a.action('scan',{});self.wait(a)
   a.action('connect',{'address':mac});self.wait(a);self.assertTrue(a.devices()[0]['connected'])
   a.action('output',{'id':2});a.action('volume',{'value':40});self.assertEqual(a.fake_output,2)
   a.action('disconnect',{'address':mac});self.wait(a);self.assertFalse(a.devices()[0]['connected']);self.assertTrue(a.devices()[0]['paired'])
   self.assertEqual(len(a.outputs()),1)
 def test_audio_rejects_unlisted_targets_and_levels(self):
  a=Audio(simulate=True)
  for op,data in [('connect',{'address':'11:22:33:44:55:66'}),('connect',{'address':None}),('output',{'id':99}),('output',{'id':True}),('volume',{'value':101}),('volume',{'value':-1})]:
   with self.assertRaises(ValueError):a.action(op,data)
 def test_pipewire_gadget_detection_uses_device_metadata(self):
  data=[{'id':6,'type':'PipeWire:Interface:Device','info':{'props':{'alsa.card_name':'UAC2_Gadget'}}},
        {'id':9,'type':'PipeWire:Interface:Node','info':{'props':{'media.class':'Audio/Source','device.id':6,'node.name':'alsa_input.usb_controller'}}},
        {'id':10,'type':'PipeWire:Interface:Node','info':{'props':{'media.class':'Audio/Sink','node.name':'speakers'}}}]
  self.assertEqual(capture_name(data),'alsa_input.usb_controller');self.assertEqual(len(pipewire_nodes(data,'Audio/Sink')),1)
  self.assertIsNone(capture_name(data[2:]))
 def test_select_output_invokes_only_known_sink(self):
  data=[{'id':51,'type':'PipeWire:Interface:Node','info':{'props':{'media.class':'Audio/Sink','node.name':'bluez_output.fixture','node.description':'Fixture'}}}]
  calls=[]
  def run(args,**kw):
   calls.append(args);return subprocess.CompletedProcess(args,0,json.dumps(data) if args==['pw-dump'] else '', '')
  a=Audio(run=run)
  with patch.object(a,'devices',return_value=[]):a.action('output',{'id':51})
  self.assertIn(['wpctl','set-default','51'],calls)
 def test_programming_requires_power_reserve(self):
  good={'connected':True,'power_flags':9,'battery':30};self.assertTrue(power_ok(good))
  for bad in [{**good,'connected':False},{**good,'power_flags':25},{**good,'power_flags':8},{**good,'battery':29},{**good,'battery':None}]:self.assertFalse(power_ok(bad))
  self.assertTrue(power_ok({**good,'power_flags':13,'battery':10}))
 def test_flash_bundle_checks_all_images(self):
  self.assertEqual(len(images(ROOT/'firmware/binaries')),8)
  with tempfile.TemporaryDirectory() as d:
   target=Path(d);shutil.copytree(ROOT/'firmware/binaries',target,dirs_exist_ok=True)
   with (target/'esp32/firmware.bin').open('ab') as f:f.write(b'corrupt')
   with self.assertRaisesRegex(ValueError,'checksum'):images(target)
 def test_boot_config_rejects_radio_disable(self):
  with self.assertRaisesRegex(ValueError,'disable-bt'):install_target.boot_text('dtoverlay=disable-bt\n')
 def test_prepared_target_audio_and_serial_configuration(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);install_target.prepare(root,'keyboard',1042)
   ui=(root/'etc/systemd/system/keyboard-ui.service').read_text()
   self.assertIn('XDG_RUNTIME_DIR=/run/user/1042',ui);self.assertIn('Requires=user@1042.service',ui)
   boot=(root/'boot/firmware/keyboard-revg.txt').read_text();self.assertNotIn('disable-bt',boot);self.assertIn('core_freq=250',boot)
   gadget=(root/'opt/keyboard/rev-g/target/usb_audio.py').read_text();self.assertIn('functions/acm.usb0',gadget)
   self.assertIn('type pipewire',(root/'etc/asound.conf').read_text())
   self.assertTrue((root/'opt/keyboard/rev-g/firmware/esp32/firmware.bin').is_file())
 def test_flash_execution_and_failure_restore_services(self):
  for fail_at in [None,'flash','stop']:
   with self.subTest(fail_at=fail_at):
    calls=[]
    class Pins:
     def __init__(self):calls.append('pins')
     def bootloader(self):calls.append('bootloader')
     def normal_boot(self):calls.append('normal_boot')
     def close(self):calls.append('release_pins')
    def run(args,**kw):
     calls.append(args)
     if fail_at=='flash' and args[0]=='esptool' or fail_at=='stop' and args[:2]==['systemctl','stop']:raise subprocess.CalledProcessError(1,args)
     return subprocess.CompletedProcess(args,0)
    response=io.BytesIO(json.dumps({'state':{'connected':True,'power_flags':9,'battery':70}}).encode())
    with contextlib.ExitStack() as stack:
     stack.enter_context(patch.object(sys,'argv',['flash_esp32.py','--bundle',str(ROOT/'firmware/binaries')]))
     for name,value in [('require_target',lambda:None),('BootPins',Pins),('urlopen',lambda *a,**k:response)]:stack.enter_context(patch.object(flash_esp32,name,value))
     stack.enter_context(patch.object(flash_esp32.os,'geteuid',return_value=0))
     stack.enter_context(patch.object(flash_esp32.signal,'signal'))
     stack.enter_context(patch.object(flash_esp32.subprocess,'run',side_effect=run))
     if fail_at:
      with self.assertRaises(subprocess.CalledProcessError):flash_esp32.main()
     else:flash_esp32.main()
    self.assertEqual(calls[-3:],['normal_boot','release_pins',['systemctl','start','keyboard-ui.service']])
    if fail_at!='stop':
     command=next(c for c in calls if isinstance(c,list) and c[0]=='esptool')
     self.assertEqual(command[command.index('--port')+1],'/dev/serial0');self.assertLess(calls.index('bootloader'),calls.index(command))
     self.assertEqual([v for v in command if v.startswith('0x')],flash_esp32.OFFSETS)
 def test_audio_entrypoint_resolves_imports_from_any_directory(self):
  if Path('/proc/device-tree/model').exists():self.skipTest('Non-CM4 fixture only')
  with tempfile.TemporaryDirectory() as d:
   r=subprocess.run([sys.executable,str(ROOT/'software/target/audio_loop.py')],cwd=d,capture_output=True,text=True)
  self.assertNotEqual(r.returncode,0);self.assertNotIn('ModuleNotFoundError',r.stderr);self.assertIn('restricted to the keyboard',r.stderr)
 def test_flasher_refuses_desktop_before_side_effects(self):
  if Path('/proc/device-tree/model').exists():self.skipTest('Non-CM4 fixture only')
  r=subprocess.run([sys.executable,str(ROOT/'software/target/flash_esp32.py')],capture_output=True,text=True)
  self.assertNotEqual(r.returncode,0);self.assertIn('restricted to the keyboard',r.stderr)
if __name__=='__main__':unittest.main(verbosity=2)
