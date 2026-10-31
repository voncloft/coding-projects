"""Exercise target I/O boundaries using private filesystem, PTY and ioctl fixtures.

No real GPIO, Bluetooth bus, USB controller, audio device or service is touched.
"""
import contextlib,fcntl,json,os,pty,signal,struct,subprocess,sys,tempfile,threading,time,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'software'),str(ROOT/'software/target')]
import usb_audio,flash_esp32
from keyboard_ui.app import SerialLink
from keyboard_ui.protocol import Kind,Packet,encode,Parser
from keyboard_ui.audio import Audio,A2DP_SINK

class TargetBoundaries(unittest.TestCase):
 def test_gpio_requests_match_linux_v1_layout_and_boot_sequence(self):
  for target,klass,lines,flags in [(usb_audio,usb_audio.VbusInput,[24],1),(flash_esp32,flash_esp32.BootPins,[17,27],2)]:
   with self.subTest(lines=lines),tempfile.TemporaryDirectory() as folder:
    dev=Path(folder);(dev/'gpiochip0').touch();sets=[];closes=[]
    def ioctl(fd,cmd,buf,mutate):
     self.assertTrue(mutate)
     if cmd==0x8044b401:
      self.assertEqual(len(buf),68);buf[32:64]=b'pinctrl-bcm2711'.ljust(32,b'\0')
     elif cmd==0xC16CB403:
      self.assertEqual(len(buf),364);self.assertEqual(list(struct.unpack_from('I'*len(lines),buf)),lines)
      self.assertEqual(struct.unpack_from('I',buf,256)[0],flags);self.assertEqual(struct.unpack_from('I',buf,356)[0],len(lines))
      self.assertEqual(list(buf[260:260+len(lines)]),[0]*len(lines));struct.pack_into('i',buf,360,42)
     elif cmd==0xC040B409:sets.append(tuple(buf[:2]))
     elif cmd==0xC040B408:buf[0]=1
     else:self.fail(f'Unexpected ioctl {cmd:x}')
    with patch.object(target,'Path',side_effect=lambda p:dev if p=='/dev' else Path(p)),patch.object(target.os,'open',return_value=10),patch.object(target.os,'close',side_effect=closes.append),patch.object(target.fcntl,'ioctl',side_effect=ioctl),patch.object(target.time,'sleep'):
     obj=klass()
     if lines==[24]:self.assertTrue(obj.present());os.close(obj.fd)
     else:obj.bootloader();obj.normal_boot();obj.close();self.assertEqual(sets,[(1,1),(1,0),(0,1),(0,0),(0,0)])
     self.assertEqual(closes,[10,42])

 def gadget(self,failure=None):
  folder=tempfile.TemporaryDirectory();self.addCleanup(folder.cleanup);root=Path(folder.name)
  config=root/'sys/kernel/config/usb_gadget';config.mkdir(parents=True)
  udc=root/'sys/class/udc/fixture';udc.mkdir(parents=True)
  base=config/'keyboard-revg';writes=[];closed=[];handlers={};state=iter([False,True,False,True]);real_close=os.close
  def close(fd):
   if fd==42:closed.append(fd)
   else:real_close(fd)
  class Sense:
   fd=42
   def present(self):
    try:return next(state)
    except StopIteration:handlers[signal.SIGTERM](None,None);return False
  def write(path,value):
   if path.name=='idVendor':
    for x in ['strings','configs','functions']:(base/x).mkdir(exist_ok=True)
   if path.name=='bmAttributes':(path.parent/'strings').mkdir(exist_ok=True)
   if path.name=='c_chmask':
    for x in ['c_mute_present','c_volume_present']:(path.parent/x).touch()
   path.write_text(str(value));writes.append((str(path.relative_to(root)),str(value)))
  def run(args,**kw):
   if failure=='modprobe':raise subprocess.CalledProcessError(1,args)
   self.assertEqual(args,['modprobe','libcomposite']);return subprocess.CompletedProcess(args,0)
  with contextlib.ExitStack() as stack:
   stack.enter_context(patch.object(usb_audio,'require_target'))
   stack.enter_context(patch.object(usb_audio,'VbusInput',Sense))
   stack.enter_context(patch.object(usb_audio,'Path',side_effect=lambda p:root/str(p).lstrip('/')))
   stack.enter_context(patch.object(usb_audio,'write',side_effect=write))
   stack.enter_context(patch.object(usb_audio.subprocess,'run',side_effect=run))
   stack.enter_context(patch.object(usb_audio.signal,'signal',side_effect=lambda s,f:handlers.update({s:f})))
   stack.enter_context(patch.object(usb_audio.time,'sleep'))
   stack.enter_context(patch.object(usb_audio.os,'close',side_effect=close))
   if failure:
    with self.assertRaises(subprocess.CalledProcessError):usb_audio.main()
   else:usb_audio.main()
  return base,writes,closed

 def test_usb_composite_creation_and_cable_edges(self):
  base,writes,closed=self.gadget();self.assertEqual(closed,[42])
  self.assertEqual((base/'configs/c.1/MaxPower').read_text(),'0')
  for function in ['uac2.usb0','acm.usb0']:
   link=base/'configs/c.1'/function;self.assertTrue(link.is_symlink());self.assertEqual(link.resolve(),base/'functions'/function)
  audio=base/'functions/uac2.usb0'
  self.assertEqual({p.name:p.read_text() for p in audio.iterdir()},{'c_chmask':'3','c_srate':'48000','c_ssize':'2','p_chmask':'0','c_mute_present':'0','c_volume_present':'0'})
  self.assertEqual([v for p,v in writes if p.endswith('/UDC')],['fixture','','fixture','',''])

 def test_usb_early_failure_releases_gpio(self):
  _,_,closed=self.gadget('modprobe');self.assertEqual(closed,[42])

 def test_real_serial_api_on_private_pty(self):
  master,slave=pty.openpty();self.addCleanup(os.close,master);self.addCleanup(os.close,slave)
  got=[];ready=threading.Event()
  def callback(p):got.append(p);ready.set()
  link=SerialLink(os.ttyname(slave),callback)
  try:
   msg=encode(Kind.POWER,255,bytes(range(14)))
   for chunk in [msg[:2],msg[2:7],msg[7:]]:os.write(master,chunk)
   self.assertTrue(ready.wait(1));self.assertEqual(got,[Packet(Kind.POWER,255,bytes(range(14)))])
   link.send(Kind.PING,255,b'fixture');import select
   self.assertTrue(select.select([master],[],[],1)[0]);self.assertEqual(Parser().feed(os.read(master,256)),[Packet(Kind.PING,255,b'fixture')])
  finally:link.closed=True;time.sleep(.3);os.close(link.fd)

 def test_headphone_pair_failure_and_verified_success(self):
  mac='01:02:03:04:05:06'
  for succeed in [False,True]:
   calls=[];flags={'paired':False,'connected':False}
   def run(args,**kwargs):
    calls.append(args)
    if 'pair' in args and succeed:flags['paired']=True
    if 'connect' in args and succeed:flags['connected']=True
    return subprocess.CompletedProcess(args,0,'','')
   a=Audio(run=run)
   with patch.object(a,'devices',side_effect=lambda:[{'address':mac,'name':'fixture',**flags}]):
    a.action('connect',{'address':mac});deadline=time.monotonic()+2
    while a.busy and time.monotonic()<deadline:time.sleep(.005)
   self.assertFalse(a.busy)
   self.assertEqual(['bluetoothctl','trust',mac] in calls,succeed)
   self.assertIn(['bluetoothctl','--timeout','20','connect',mac,A2DP_SINK],calls)
   self.assertIn('connected' if succeed else 'did not connect',a.message)

if __name__=='__main__':unittest.main(verbosity=2)
