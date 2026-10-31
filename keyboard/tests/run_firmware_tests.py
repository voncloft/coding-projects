#!/usr/bin/env python3
from pathlib import Path
import ctypes as C,json,random,subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'software'))
from keyboard_ui.protocol import Parser,Packet,encode,crc
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'tests/stubs'),str(root/'tests/power_harness.c'),'-o',str(out/'power-tests')],check=True)
names=['startup','pd','charge-while-off','wrong-part','wrong-cell-count','no-battery','open-ntc','hot-ntc','low-start','low-shutdown','overcurrent','fault','bus-loss','halt-sequence','ready-generation','shed-loads','no-restart-after-low','programming-halt','programming-timeout','default-usb','typec-1a5','false-pd-voltage','input-limit-write-failure','charge-register-readback-failure','adc-enable-write-failure','invalid-telemetry-light','power-timer-wrap','watch-fault','all-load-combinations']
for name in names:subprocess.run([str(out/'power-tests'),name],check=True)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC',str(root/'tests/wire_harness.c'),'-o',str(out/'wire.so')],check=True)
class Wire(C.Structure):_fields_=[('kind',C.c_uint8),('seq',C.c_uint8),('length',C.c_uint16),('data',C.c_uint8*256)]
lib=C.CDLL(str(out/'wire.so'));lib.encode_frame.argtypes=[C.c_uint8,C.c_uint8,C.c_void_p,C.c_uint16,C.c_void_p];lib.encode_frame.restype=C.c_size_t
lib.parse_stream.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Wire),C.POINTER(C.c_uint)];lib.parse_stream.restype=C.c_int
assert crc(b'123456789')==0x29b1
rng=random.Random(82472);checks=0
for n in range(257):
 for data in [bytes(n),bytes([255])*n,bytes(rng.randrange(256) for _ in range(n))]:
  t=rng.randrange(1,35);seq=rng.randrange(256);buf=C.create_string_buffer(272)
  length=lib.encode_frame(t,seq,data,n,buf);encoded=encode(t,seq,data)
  assert buf.raw[:length]==encoded
  parser=Parser();packets=[]
  for i in range(0,len(encoded),7):packets+=parser.feed(encoded[i:i+7])
  assert packets==[Packet(t,seq,data)]
  parsed=Wire();bad=C.c_uint();assert lib.parse_stream(encoded,len(encoded),C.byref(parsed),C.byref(bad))==1
  assert (parsed.kind,parsed.seq,bytes(parsed.data[:parsed.length]))==(t,seq,data)
  checks+=1
valid=encode(17,4,b'\x06\x01')
for junk in [b'\x01'*300+b'\0',b'\xff\x04\0',b'\x02\x01\0']:
 stream=junk+valid;parser=Parser();assert parser.feed(stream)==[Packet(17,4,b'\x06\x01')];assert parser.bad
 parsed=Wire();bad=C.c_uint();assert lib.parse_stream(stream,len(stream),C.byref(parsed),C.byref(bad))==1 and bad.value
for _ in range(400):
 frame=bytearray(encode(4,7,bytes(rng.randrange(256) for _ in range(80))));idx=rng.randrange(len(frame)-1);frame[idx]^=1<<rng.randrange(8)
 parser=Parser();packets=parser.feed(frame+valid);assert packets[-1]==Packet(17,4,b'\x06\x01');assert all(p.kind!=4 for p in packets)
report={'passed':True,'power_scenarios':names,'cross_language_packet_vectors':checks,'corruption_recovery_trials':400,'scope':'Shipped C power state machine against a register/GPIO fixture; C and Python wire format compatibility. No electrical hardware exercised.'}
(out/'firmware-tests.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
