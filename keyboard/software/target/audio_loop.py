#!/usr/bin/env python3
"""Route USB PC playback into the same PipeWire output as Plexamp and key sounds."""
import subprocess,time,signal,json,sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from guard import require_target
from keyboard_ui.audio import pipewire_nodes

def capture_name(data):
    for node in pipewire_nodes(data,'Audio/Source'):
        if node['name'].startswith('keyboard-pc-audio'):continue
        label=' '.join(str(v) for k,v in node['properties'].items() if k in ['api.alsa.card.name','alsa.card_name','alsa.name','device.name','device.description','node.name','node.description']).lower()
        compact=label.replace('_','').replace('-','').replace(' ','')
        if 'uac2gadget' in compact or 'usbgadget' in compact:return node['name']
    return None

def main():
    require_target();stopping=False;child=None
    def stop(*args):
        nonlocal stopping
        stopping=True
        if child and child.poll() is None:child.terminate()
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    while not stopping:
        try:
            data=json.loads(subprocess.run(['pw-dump'],capture_output=True,text=True,check=True,timeout=5).stdout)
            source=capture_name(data)
            if not source:time.sleep(1);continue
            # No fixed playback target: follow WirePlumber's selected/default output.
            child=subprocess.Popen(['pw-loopback','--name','keyboard-pc-audio','--channels','2','--latency','40','--capture',source,
                                    '--capture-props','{"node.dont-reconnect":true}',
                                    '--playback-props','{"media.role":"Music","node.autoconnect":true}'])
            child.wait()
        except (OSError,ValueError,subprocess.SubprocessError) as e:print('USB audio waiting:',e,flush=True)
        if not stopping:time.sleep(1)
if __name__=='__main__':main()
