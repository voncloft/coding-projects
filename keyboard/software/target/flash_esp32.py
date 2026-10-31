#!/usr/bin/env python3
"""Update the keyboard's ESP32 through the CM4 UART, including from its USB console."""
import argparse,fcntl,hashlib,json,os,signal,struct,subprocess,time
from pathlib import Path
from urllib.request import urlopen
from guard import require_target

OFFSETS=['0x0000','0x8000','0xe000','0x10000']
class BootPins:
    """GPIO17/27 drive the carrier's open-collector BOOT/EN pull-down transistors."""
    def __init__(self):
        self.fd=None
        for path in sorted(Path('/dev').glob('gpiochip*')):
            chip=os.open(path,os.O_RDONLY)
            try:
                info=bytearray(68);fcntl.ioctl(chip,0x8044b401,info,True)
                if bytes(info[32:64]).split(b'\0')[0]!=b'pinctrl-bcm2711':continue
                request=bytearray(364);struct.pack_into('II',request,0,17,27);struct.pack_into('I',request,256,2)
                request[324:356]=b'keyboard-c3-update'.ljust(32,b'\0');struct.pack_into('I',request,356,2)
                fcntl.ioctl(chip,0xC16CB403,request,True);self.fd=struct.unpack_from('i',request,360)[0];break
            finally:os.close(chip)
        if self.fd is None:raise RuntimeError('Could not request the carrier BOOT/RESET controls')
    def set(self,boot,reset):
        values=bytearray(64);values[0]=bool(boot);values[1]=bool(reset);fcntl.ioctl(self.fd,0xC040B409,values,True)
    def bootloader(self):
        self.set(True,True);time.sleep(.15);self.set(True,False);time.sleep(.3)
    def normal_boot(self):
        self.set(False,True);time.sleep(.15);self.set(False,False);time.sleep(.3)
    def close(self):
        if self.fd is not None:
            try:self.set(False,False)
            finally:os.close(self.fd);self.fd=None

def images(root):
    root=root.resolve();manifest=json.loads((root/'manifest.json').read_text())
    if (manifest.get('chip'),manifest.get('flash_mode'),manifest.get('flash_frequency'),manifest.get('flash_size'))!=('esp32c3','dio','80m','4MB'):raise ValueError('Bundle does not match the keyboard ESP32-C3 hardware')
    if list(manifest.get('flash_images',{}))!=OFFSETS:raise ValueError('Unexpected image layout')
    args=[]
    for offset in OFFSETS:
        relative=manifest['flash_images'][offset];path=(root/relative).resolve()
        if not path.is_relative_to(root) or not path.is_file():raise ValueError('Image is outside its bundle or missing')
        data=path.read_bytes();record=manifest['files'].get(relative,{})
        if len(data)!=record.get('bytes') or hashlib.sha256(data).hexdigest()!=record.get('sha256'):raise ValueError('Image checksum mismatch: '+relative)
        args.extend([offset,str(path)])
    return args

def power_ok(state):
    flags=state.get('power_flags',0);percent=state.get('battery')
    return bool(state.get('connected') and flags&1 and flags&8 and not flags&16 and
                (flags&4 or type(percent) is int and percent>=30))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bundle',type=Path,default=Path(__file__).resolve().parents[1]/'firmware');args=p.parse_args()
    require_target()
    if os.geteuid()!=0:p.error('Run this updater with sudo on the keyboard CM4')
    flash=images(args.bundle)
    with urlopen('http://127.0.0.1:8765/api/state',timeout=5) as response:state=json.load(response)['state']
    if not power_ok(state):p.error('Wait for a valid power reading and at least 30% battery or the full-power PD charger. Do not switch off or unplug power during updating.')
    pins=BootPins();stopped=False
    def interrupted(*_):raise KeyboardInterrupt()
    signal.signal(signal.SIGTERM,interrupted)
    try:
        stopped=True
        subprocess.run(['systemctl','stop','keyboard-ui.service'],check=True,timeout=15)
        pins.bootloader()
        subprocess.run(['esptool','--chip','esp32c3','--port','/dev/serial0','--baud','460800','--before','no_reset','--after','no_reset',
                        'write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','4MB',*flash],check=True,timeout=90)
        print('ESP32 write and esptool verification completed.')
    finally:
        try:pins.normal_boot()
        finally:
            pins.close()
            if stopped:subprocess.run(['systemctl','start','keyboard-ui.service'],check=True,timeout=20)
if __name__=='__main__':main()
