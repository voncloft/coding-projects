#!/usr/bin/env python3
"""Self-powered composite USB speaker and programming console gadget."""
from pathlib import Path
import fcntl,os,signal,struct,subprocess,time
from guard import require_target

class VbusInput:
    def __init__(self):
        self.fd=None
        for path in sorted(Path('/dev').glob('gpiochip*')):
            chip=os.open(path,os.O_RDONLY)
            try:
                info=bytearray(68);fcntl.ioctl(chip,0x8044b401,info,True)
                label=bytes(info[32:64]).split(b'\0')[0]
                if label!=b'pinctrl-bcm2711':continue
                request=bytearray(364);struct.pack_into('I',request,0,24);struct.pack_into('I',request,256,1)
                request[324:356]=b'keyboard-pc-vbus'.ljust(32,b'\0');struct.pack_into('I',request,356,1)
                fcntl.ioctl(chip,0xC16CB403,request,True);self.fd=struct.unpack_from('i',request,360)[0];break
            finally:os.close(chip)
        if self.fd is None:raise RuntimeError('BCM2711 VBUS input GPIO could not be requested')
    def present(self):
        state=bytearray(64);fcntl.ioctl(self.fd,0xC040B408,state,True);return bool(state[0])

def write(path,value):path.write_text(str(value))
def run_gadget(sense):
    subprocess.run(['modprobe','libcomposite'],check=True)
    base=Path('/sys/kernel/config/usb_gadget/keyboard-revg')
    if base.exists():
        serial=base/'strings/0x409/serialnumber'
        if not serial.exists() or serial.read_text().strip()!='REVG-PROTOTYPE':raise RuntimeError('Existing gadget is not owned by this project')
        write(base/'UDC','')
        if not (base/'configs/c.1/acm.usb0').is_symlink():raise RuntimeError('Incomplete project USB gadget; reboot the CM4 after installation')
    else:
        base.mkdir()
        for name,value in {'idVendor':'0x1d6b','idProduct':'0x0104','bcdDevice':'0x0100','bcdUSB':'0x0200','bDeviceClass':'0xef','bDeviceSubClass':'0x02','bDeviceProtocol':'0x01'}.items():write(base/name,value)
        strings=base/'strings/0x409';strings.mkdir()
        for name,value in {'serialnumber':'REVG-PROTOTYPE','manufacturer':'Personal keyboard prototype','product':'Keyboard audio and programming console'}.items():write(strings/name,value)
        config=base/'configs/c.1';config.mkdir();write(config/'bmAttributes','0xc0');write(config/'MaxPower','0')
        labels=config/'strings/0x409';labels.mkdir();write(labels/'configuration','Stereo audio and serial console')
        audio=base/'functions/uac2.usb0';audio.mkdir()
        for name,value in {'c_chmask':3,'c_srate':48000,'c_ssize':2,'p_chmask':0}.items():write(audio/name,value)
        # PC volume is handled in its playback stream; omit unimplemented device mixer controls.
        for name in ['c_mute_present','c_volume_present']:
            if (audio/name).exists():write(audio/name,0)
        (config/'uac2.usb0').symlink_to(audio)
        console=base/'functions/acm.usb0';console.mkdir()
        (config/'acm.usb0').symlink_to(console)
    udcs=list(Path('/sys/class/udc').iterdir())
    if len(udcs)!=1:raise RuntimeError('Expected exactly one CM4 USB device controller')
    stopping=False
    def stop(*args):
        nonlocal stopping
        stopping=True
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    bound=False
    try:
        while not stopping:
            present=sense.present()
            if present!=bound:write(base/'UDC',udcs[0].name if present else '');bound=present
            time.sleep(.2)
    finally:write(base/'UDC','')

def main():
    require_target()
    sense=VbusInput()
    try:run_gadget(sense)
    finally:os.close(sense.fd)
if __name__=='__main__':main()
