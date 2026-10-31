#!/usr/bin/env python3
"""Local touchscreen service for the keyboard's CM4. --simulate never opens hardware."""
import argparse,copy,json,os,queue,re,secrets,select,struct,subprocess,termios,threading,time
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.request import Request,urlopen
from urllib.parse import urlsplit
from .protocol import Kind,Packet,Parser,encode
from .macros import DEFAULT,validate_config,validate_steps,chord,text_codes
from . import infrared
from .audio import Audio

STATIC=Path(__file__).parent/'static'

def atomic_json(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    pending=path.with_suffix('.pending')
    fd=os.open(pending,os.O_WRONLY|os.O_CREAT|os.O_TRUNC,0o600)
    with os.fdopen(fd,'w') as f:
        json.dump(value,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
    pending.replace(path)
    fd=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
    try:os.fsync(fd)
    finally:os.close(fd)

class SerialLink:
    def __init__(self,path,callback):
        self.fd=os.open(path,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
        attr=termios.tcgetattr(self.fd);attr[0]=0;attr[1]=0
        attr[2]=termios.CS8|termios.CREAD|termios.CLOCAL;attr[3]=0
        attr[4]=termios.B115200;attr[5]=termios.B115200
        attr[6][termios.VMIN]=0;attr[6][termios.VTIME]=0
        termios.tcsetattr(self.fd,termios.TCSANOW,attr)
        self.callback=callback;self.parser=Parser();self.lock=threading.Lock();self.closed=False
        threading.Thread(target=self.read,daemon=True).start()
    def send(self,kind,seq,data=b''):
        remaining=memoryview(encode(kind,seq,data))
        with self.lock:
            until=time.monotonic()+1
            while remaining:
                if time.monotonic()>until:raise TimeoutError('Controller transmit timed out')
                if select.select([],[self.fd],[],0.1)[1]:
                    try:remaining=remaining[os.write(self.fd,remaining):]
                    except BlockingIOError:pass
    def read(self):
        while not self.closed:
            try:
                if select.select([self.fd],[],[],0.25)[0]:
                    for packet in self.parser.feed(os.read(self.fd,4096)):self.callback(packet)
            except (OSError,ValueError):self.closed=True

class Controller:
    def __init__(self,directory,simulate=True,serial='/dev/serial0'):
        self.directory=Path(directory);self.simulate=simulate;self.seq=0;self.token=secrets.token_urlsafe(32)
        self.lock=threading.RLock();self.cond=threading.Condition(self.lock);self.cancel=threading.Event();self.cancel_epoch=0
        self.jobs=queue.Queue(8);self.acks={};self.events=[];self.event_id=0;self.ready_generation=None
        self.encoder_pressed=False;self.encoder_since=0;self.host_epoch=0;self.received_power=0;self.ir_parts=None;self.ir_version=0;self.tested=None
        self.state={'simulation':simulate,'host':0,'host_flags':0,'bank':0,'mode':0,'battery':None,
                    'power_flags':0,'loads':0,'requested_loads':60,'generation':0,'connected':simulate,
                    'busy':False,'message':'Ready','ir_state':0,'candidate':None,'display_brightness':100}
        self.config=self.load('config.json',DEFAULT);validate_config(self.config)
        self.remotes=self.load('remotes.json',{})
        for code in self.remotes.values():infrared.validate(code)
        self.home=self.load('home.json',{'url':'','token':''})
        self.audio=Audio(simulate)
        self.link=None if simulate else SerialLink(serial,self.packet)
        threading.Thread(target=self.worker,daemon=True).start()
        if not simulate:threading.Thread(target=self.monitor,daemon=True).start()
    def load(self,name,default):
        p=self.directory/name
        return json.loads(p.read_text()) if p.exists() else copy.deepcopy(default)
    def event(self,kind,**data):
        with self.lock:
            self.event_id+=1;self.events.append({'id':self.event_id,'kind':kind,**data});self.events=self.events[-80:]
    def send(self,kind,data=b'',ack=False):
        with self.lock:
            seq=self.seq;self.seq=(self.seq+1)&255;self.acks.pop(seq,None)
            if self.simulate:
                self.event('simulated',command=int(kind),data=list(data))
                if kind==Kind.HOST_SELECT:self.packet(Packet(Kind.HOST_STATUS,seq,bytes([data[0],3,0])))
                if kind==Kind.BANK:self.packet(Packet(Kind.BANK,seq,data))
                if kind==Kind.MODE:self.packet(Packet(Kind.MODE,seq,data))
                if kind==Kind.TAP:self.packet(Packet(Kind.ACK,seq,bytes([kind,0])))
            else:self.link.send(kind,seq,data)
            if ack:
                deadline=time.monotonic()+1.5
                while seq not in self.acks:
                    remaining=deadline-time.monotonic()
                    if remaining<=0:raise TimeoutError('Keyboard did not acknowledge the shortcut; macro stopped')
                    self.cond.wait(remaining)
                command,result=self.acks.pop(seq)
                if command!=kind:raise ValueError('Unexpected keyboard acknowledgement; macro stopped')
                if result==3:raise ValueError('Release the physical keys before running this macro')
                if result==4:raise ValueError('Selected computer is not connected; macro stopped')
                if result:raise ValueError('Keyboard refused the shortcut; check power and connection')
            return seq
    def packet(self,p):
        with self.cond:
            d=p.data
            if p.kind==Kind.ACK and len(d)==2:self.acks[p.seq]=tuple(d);self.cond.notify_all()
            elif p.kind==Kind.HOST_STATUS and len(d)==3:
                if self.state['host']!=d[0]:self.host_epoch+=1
                self.state.update(host=d[0],host_flags=d[1]);self.event('host',slot=d[0])
            elif p.kind==Kind.POWER and len(d)==14:
                soc,flags,bmv,ma,vmv,loads,reason,generation,ts=struct.unpack('<BBHhHBBHH',d)
                self.received_power=time.monotonic()
                self.state.update(battery=soc if soc<=100 else None,power_flags=flags,battery_mv=bmv,battery_ma=ma,
                                  input_mv=vmv,loads=loads,reason=reason,generation=generation,connected=True)
                if generation!=self.ready_generation and flags&8 and not flags&16:
                    self.ready_generation=generation;self.send(Kind.READY,struct.pack('<H',generation))
                    self.send(Kind.LOADS,bytes([self.state['requested_loads']]))
            elif p.kind==Kind.SHUTDOWN and len(d)==3:
                generation=struct.unpack_from('<H',d)[0]
                if generation==self.state['generation'] and self.state['power_flags']&16:
                    self.cancel_epoch+=1;self.cancel.set();self.state['message']='Shutting down safely'
                    if not self.simulate and not getattr(self,'shutdown_started',False):
                        self.shutdown_started=True
                        # A separate, restricted system service owns target power-off.
                        Path('/run/keyboard/poweroff-request').touch(mode=0o600)
            elif p.kind==Kind.BANK and len(d)==1:self.state['bank']=d[0];self.event('bank',value=d[0])
            elif p.kind==Kind.MODE and len(d)==1:self.state['mode']=d[0];self.event('mode',value=d[0])
            elif p.kind==Kind.ENCODER and len(d)==3:
                self.event('encoder',delta=struct.unpack('b',d[:1])[0],pressed=bool(d[1]),mode=d[2])
                if d[1] and not self.encoder_pressed:self.encoder_since=time.monotonic()
                if not d[1] and self.encoder_pressed and d[2]==1 and time.monotonic()-self.encoder_since<0.8:self.event('activate',mode=1)
                self.encoder_pressed=bool(d[1])
            elif p.kind==Kind.KEY_EVENT and len(d)==3 and d[2] and self.config.get('key_sounds'):
                if not self.simulate:
                    try:self.jobs.put_nowait(('sound',None,self.host_epoch,self.cancel_epoch))
                    except queue.Full:pass
            elif p.kind==Kind.PASSKEY:self.state['message']='Type the six-digit Bluetooth code on the keys, then press Enter'
            elif p.kind==Kind.IR_META and len(d)==4:
                hz,count=struct.unpack('<HH',d)
                self.ir_parts={'frequency':hz,'pulses':[None]*count} if 2<=count<=1024 and 25000<=hz<=60000 else None
            elif p.kind==Kind.IR_DATA and len(d)>=4 and len(d)%2==0 and self.ir_parts:
                offset=struct.unpack_from('<H',d)[0];values=struct.unpack('<'+'H'*((len(d)-2)//2),d[2:])
                if offset+len(values)<=len(self.ir_parts['pulses']):
                    self.ir_parts['pulses'][offset:offset+len(values)]=values
                    if all(x is not None for x in self.ir_parts['pulses']):
                        try:self.state['candidate']=infrared.validate(self.ir_parts);self.tested=None;self.event('learned')
                        except ValueError as e:self.state['message']=str(e)
                        self.ir_parts=None
            elif p.kind==Kind.IR_STATE and len(d)==1:
                self.state['ir_state']=d[0];self.ir_version+=1;self.cond.notify_all()
                if d[0]==4:self.state['message']='IR command failed or learning timed out'
    def monitor(self):
        while True:
            time.sleep(1)
            with self.lock:
                if self.received_power and time.monotonic()-self.received_power>4:
                    self.state.update(connected=False,battery=None);self.cancel_epoch+=1;self.cancel.set()
    def snapshot(self):
        with self.lock:
            return {'state':copy.deepcopy(self.state),'config':copy.deepcopy(self.config),'events':self.events[:],
                    'remotes':list(self.remotes),'home_url':self.home.get('url',''),'token':self.token}
    def submit(self,job,value=None):
        with self.lock:
            try:self.jobs.put_nowait((job,value,self.host_epoch,self.cancel_epoch))
            except queue.Full:raise ValueError('Too many actions queued')
        return {'queued':True}
    def worker(self):
        while True:
            job,value,epoch,cancel_epoch=self.jobs.get()
            try:
                with self.lock:
                    if cancel_epoch!=self.cancel_epoch:raise ValueError('Queued action cancelled')
                    if epoch!=self.host_epoch:raise ValueError('Host changed; queued action cancelled')
                    if self.state['power_flags']&16:raise ValueError('Keyboard is shutting down')
                    self.cancel.clear();self.state['busy']=True
                if job=='sound':
                    if not self.simulate and (not getattr(self,'sound_process',None) or self.sound_process.poll() is not None):
                        self.sound_process=subprocess.Popen(['pw-play',str(Path(__file__).parent/'key.wav')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    continue
                if job=='macro':self.run_steps(value,epoch)
                elif job=='ir':self.transmit(value)
                elif job=='candidate':
                    self.transmit(value)
                    with self.lock:self.tested=copy.deepcopy(value);self.state['message']='Did the device respond? Save only if it worked.'
                self.state['message']='Done' if job!='candidate' else self.state['message']
            except Exception as e:self.state['message']=str(e)
            finally:self.state['busy']=False;self.jobs.task_done()
    def pause(self,seconds):
        if self.cancel.wait(seconds):raise ValueError('Macro cancelled')
    def run_steps(self,steps,epoch=None):
        validate_steps(steps)
        if epoch is None:epoch=self.host_epoch
        for step in steps:
            if self.cancel.is_set():raise ValueError('Macro cancelled')
            if epoch!=self.host_epoch:raise ValueError('Host changed; macro stopped')
            kind=step['type']
            if kind in ['shortcut','text']:
                if self.state['host'] and not self.state['host_flags']&2:raise ValueError('Selected Bluetooth device is not connected')
                codes=[chord(step['value'])] if kind=='shortcut' else text_codes(step['value'])
                for code in codes:
                    if epoch!=self.host_epoch:raise ValueError('Host changed; macro stopped')
                    self.send(Kind.TAP,struct.pack('<H',code),ack=True);self.pause(.035)
            elif kind=='delay':self.pause(step['ms']/1000)
            elif kind=='bank':self.send(Kind.BANK,bytes([step['bank']]))
            elif kind=='home':self.home_action(step)
            elif kind=='ir':self.transmit(self.remotes[step['name']])
    def home_request(self,path,data=None):
        if not self.home.get('url') or not self.home.get('token'):raise ValueError('Set the Home Assistant address and token in Devices')
        if self.simulate:raise ValueError('Network actions are disabled in simulation')
        req=Request(self.home['url'].rstrip('/')+'/api/'+path,
                    data=None if data is None else json.dumps(data).encode(),
                    headers={'Authorization':'Bearer '+self.home['token'],'Content-Type':'application/json'})
        with urlopen(req,timeout=8) as response:return json.load(response)
    def home_action(self,step):
        if self.simulate:self.event('simulated-device',entity=step['entity'],service=step['service']);return
        domain=step['entity'].split('.')[0]
        self.home_request(f'services/{domain}/{step["service"]}',{'entity_id':step['entity']})
    def transmit(self,code):
        code=infrared.validate(code)
        if self.simulate:self.event('simulated-ir',durations=len(code['pulses']));return
        with self.lock:version=self.ir_version
        self.send(Kind.IR_BEGIN,struct.pack('<HH',code['frequency'],len(code['pulses'])));self.pause(.03)
        for offset in range(0,len(code['pulses']),120):
            chunk=code['pulses'][offset:offset+120]
            self.send(Kind.IR_CHUNK,struct.pack('<H',offset)+struct.pack('<'+'H'*len(chunk),*chunk));self.pause(.03)
        self.send(Kind.IR_COMMIT)
        with self.cond:
            deadline=time.monotonic()+2
            while self.ir_version<version+3 or self.state['ir_state']!=0:
                if self.state['ir_state']==4:raise ValueError('IR transmitter rejected the command')
                remaining=deadline-time.monotonic()
                if remaining<=0:raise TimeoutError('IR transmission did not complete')
                self.cond.wait(remaining)
    def action(self,path,value):
        if path.startswith('audio-'):return self.audio.action(path[6:],value)
        if path=='config':
            config=validate_config(value);atomic_json(self.directory/'config.json',config);self.config=copy.deepcopy(config);return {'saved':True}
        if path=='macro':
            for page in self.config['pages']:
                for button in page['buttons']:
                    if button['id']==value.get('id'):return self.submit('macro',copy.deepcopy(button['steps']))
            raise ValueError('Button not found')
        if path=='run':return self.submit('macro',copy.deepcopy(validate_steps(value['steps'])))
        if path=='cancel':
            with self.lock:
                self.cancel_epoch+=1;self.cancel.set()
                while True:
                    try:self.jobs.get_nowait();self.jobs.task_done()
                    except queue.Empty:break
            return {'cancelled':True}
        if path=='host':
            slot=value.get('slot');pair=value.get('pair',False)
            if type(slot) is not int or slot not in range(4) or type(pair) is not bool:raise ValueError('Invalid host')
            self.action('cancel',{})
            with self.lock:
                self.host_epoch+=1;self.state.update(host=slot,host_flags=0)
                self.send(Kind.HOST_SELECT,bytes([slot,pair]))
            return {'selected':slot}
        if path in ['bank','mode']:
            v=value.get('value');limit=4 if path=='bank' else 2
            if type(v) is not int or not 0<=v<limit:raise ValueError('Invalid selection')
            self.send(Kind.BANK if path=='bank' else Kind.MODE,bytes([v]));return {'selected':v}
        if path=='rgb':
            color=value.get('color','');brightness=value.get('brightness',100)
            if not re.fullmatch('#[0-9a-fA-F]{6}',color) or type(brightness) is not int or not 0<=brightness<=100:raise ValueError('Invalid RGB settings')
            import colorsys
            h,s,v=colorsys.rgb_to_hsv(*[int(color[i:i+2],16)/255 for i in [1,3,5]])
            self.send(Kind.RGB,bytes([round(h*255),round(s*255),round(v*32*brightness/100),bool(value.get('enabled',True))]));return {'applied':True}
        if path=='loads':
            mask=value.get('mask')
            if type(mask) is not int or mask&~60:raise ValueError('Invalid power selection')
            self.state['requested_loads']=mask;self.send(Kind.LOADS,bytes([mask]));return {'requested':mask}
        if path=='brightness':
            percent=value.get('value')
            if type(percent) is not int or not 5<=percent<=100:raise ValueError('Brightness must be 5–100%')
            if not self.simulate:
                paths=list(Path('/sys/class/backlight').glob('*/brightness'))
                if len(paths)!=1:raise ValueError('Display backlight not found')
                maximum=int((paths[0].parent/'max_brightness').read_text());paths[0].write_text(str(round(maximum*percent/100)))
            self.state['display_brightness']=percent;return {'applied':True}
        if path=='home-config':
            url=value.get('url','');token=value.get('token','') or self.home.get('token','');u=urlsplit(url)
            if u.scheme not in ['http','https'] or not u.hostname or u.username or u.password or u.query or u.fragment:raise ValueError('Enter the Home Assistant http(s) address')
            if len(token)>4096:raise ValueError('Invalid token')
            self.home={'url':url.rstrip('/'),'token':token};atomic_json(self.directory/'home.json',self.home);return {'saved':True}
        if path=='home-list':
            entities=self.home_request('states')
            return [{'entity':e['entity_id'],'name':e.get('attributes',{}).get('friendly_name',e['entity_id']),'state':e['state']}
                    for e in entities if e['entity_id'].split('.')[0] in ['light','switch','scene','script','media_player']]
        if path=='ir-learn':
            self.ir_parts=None;self.tested=None;self.state['candidate']=None;self.send(Kind.IR_LEARN);return {'learning':True}
        if path=='ir-import':
            if 'pronto' in value:code=infrared.pronto(value['pronto'])
            elif 'nec' in value:code=infrared.nec(**value['nec'])
            else:code=infrared.validate(value)
            self.state['candidate']=code;self.tested=None;return {'loaded':True}
        if path=='ir-test':
            if not self.state['candidate']:raise ValueError('Learn or import a command first')
            return self.submit('candidate',copy.deepcopy(self.state['candidate']))
        if path=='ir-save':
            name=value.get('name','').strip()
            if not 1<=len(name)<=60:raise ValueError('Give the command a short name')
            if value.get('worked') is not True or self.tested!=self.state['candidate'] or not self.tested:raise ValueError('Test this command and confirm the device responded before saving')
            self.remotes[name]=copy.deepcopy(self.tested);atomic_json(self.directory/'remotes.json',self.remotes);return {'saved':name}
        if path=='ir-send':return self.submit('ir',copy.deepcopy(self.remotes[value['name']]))
        raise ValueError('Unknown action')

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass # Keep tokens and text macros out of access logs.
    def local(self):
        return self.headers.get('Host') in [f'127.0.0.1:{self.server.server_port}',f'localhost:{self.server.server_port}']
    def reply(self,status,value,mime='application/json'):
        body=json.dumps(value).encode() if mime=='application/json' else value
        self.send_response(status);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(body)))
        self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff')
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; frame-src http://127.0.0.1:32500; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        self.end_headers();self.wfile.write(body)
    def do_GET(self):
        if not self.local():return self.reply(403,{'error':'Local interface only'})
        path=urlsplit(self.path).path
        if path=='/api/state':return self.reply(200,self.server.controller.snapshot())
        if path=='/api/export':return self.reply(200,self.server.controller.config)
        files={'/':('index.html','text/html; charset=utf-8'),'/app.js':('app.js','text/javascript; charset=utf-8'),'/style.css':('style.css','text/css; charset=utf-8')}
        if path not in files:return self.reply(404,{'error':'Not found'})
        name,mime=files[path];self.reply(200,(STATIC/name).read_bytes(),mime)
    def do_POST(self):
        if not self.local() or self.headers.get('X-Keyboard-Token')!=self.server.controller.token:return self.reply(403,{'error':'Reload the local screen'})
        if self.headers.get('Content-Type')!='application/json':return self.reply(415,{'error':'Use JSON'})
        try:
            n=int(self.headers.get('Content-Length','0'))
            if not 0<n<=200000:raise ValueError('Request size invalid')
            value=json.loads(self.rfile.read(n));path=urlsplit(self.path).path
            if not path.startswith('/api/'):raise ValueError('Unknown action')
            self.reply(200,self.server.controller.action(path[5:],value))
        except (ValueError,KeyError,TypeError,TimeoutError,OSError) as e:self.reply(400,{'error':str(e)})

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--simulate',action='store_true');parser.add_argument('--hardware',action='store_true')
    parser.add_argument('--data',type=Path,required=True);parser.add_argument('--serial',default='/dev/serial0');parser.add_argument('--port',type=int,default=8765)
    args=parser.parse_args()
    if args.simulate==args.hardware:parser.error('Choose --simulate or --hardware')
    if args.hardware:
        model=Path('/proc/device-tree/model')
        if not model.exists() or b'Compute Module 4' not in model.read_bytes():parser.error('Hardware mode is restricted to the keyboard CM4')
        if not Path('/etc/keyboard/rev-g-target').exists():parser.error('Target installation marker missing')
    controller=Controller(args.data,args.simulate,args.serial)
    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler);server.controller=controller
    print(f'Keyboard screen: http://127.0.0.1:{args.port} ({"simulation" if args.simulate else "CM4 hardware"})',flush=True)
    server.serve_forever()
if __name__=='__main__':main()
