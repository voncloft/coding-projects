"""Version 1 framed serial protocol shared with the RP2040 and ESP32-C3."""
from dataclasses import dataclass
from enum import IntEnum
import struct

class Kind(IntEnum):
    HELLO=1; HOST_SELECT=2; HOST_STATUS=3; KEYS=4; CONSUMER=5; SYSTEM=6; MOUSE=7
    POWER=8; KEY_EVENT=9; ENCODER=10; MACRO=11; BANK=12; MODE=13; LOADS=14
    SHUTDOWN=15; HALTED=16; TAP=17; PING=18; PONG=19; IR_LEARN=20; IR_META=21
    IR_DATA=22; IR_BEGIN=23; IR_CHUNK=24; IR_COMMIT=25; IR_STATE=26; SOC=27
    READY=28; RGB=29; CONFIG=30; PASSKEY=31; CONFIRM=32; UNPAIR=33; ACK=34

@dataclass(frozen=True)
class Packet:
    kind: int
    seq: int
    data: bytes

def crc(data):
    value=0xffff
    for b in data:
        value ^= b << 8
        for _ in range(8):
            value=((value<<1)^0x1021 if value&0x8000 else value<<1)&0xffff
    return value

def encode(kind,seq,data=b''):
    if len(data)>256: raise ValueError('Packet exceeds 256 bytes')
    raw=struct.pack('<BBBH',1,kind,seq,len(data))+data
    raw+=struct.pack('<H',crc(raw))
    result=bytearray([0]); code_at=0; code=1
    for b in raw:
        if b:
            result.append(b);code+=1
            if code==255:
                result[code_at]=code;code_at=len(result);result.append(0);code=1
        else:
            result[code_at]=code;code_at=len(result);result.append(0);code=1
    result[code_at]=code;result.append(0)
    return bytes(result)

class Parser:
    def __init__(self): self.buffer=bytearray();self.dropping=False;self.bad=0
    def feed(self,chunk):
        packets=[]
        for b in chunk:
            if b:
                if self.dropping: continue
                if len(self.buffer)==272:
                    self.dropping=True;self.bad+=1
                else: self.buffer.append(b)
                continue
            if self.dropping:
                self.buffer.clear();self.dropping=False;continue
            if not self.buffer: continue
            encoded=self.buffer;self.buffer=bytearray();raw=bytearray();i=0
            try:
                while i<len(encoded):
                    code=encoded[i];i+=1
                    if not code or i+code-1>len(encoded):raise ValueError()
                    raw.extend(encoded[i:i+code-1]);i+=code-1
                    if code!=255 and i<len(encoded):raw.append(0)
                    if len(raw)>263:raise ValueError()
                if len(raw)<7 or raw[0]!=1:raise ValueError()
                n=struct.unpack_from('<H',raw,3)[0]
                if n>256 or n+7!=len(raw) or crc(raw[:-2])!=int.from_bytes(raw[-2:],'little'):raise ValueError()
                packets.append(Packet(raw[1],raw[2],bytes(raw[5:-2])))
            except ValueError:self.bad+=1
        return packets
