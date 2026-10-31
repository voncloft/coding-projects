"""CM4 headphone pairing and PipeWire output selection; simulation never opens a bus."""
import json,re,subprocess,threading,time

A2DP_SINK='0000110b-0000-1000-8000-00805f9b34fb'
MAC=re.compile(r'(?:[0-9A-F]{2}:){5}[0-9A-F]{2}')

def pipewire_nodes(data,media_class):
    devices={x['id']:x.get('info',{}).get('props',{}) for x in data if x.get('type')=='PipeWire:Interface:Device'}
    result=[]
    for obj in data:
        props=obj.get('info',{}).get('props',{})
        if obj.get('type')!='PipeWire:Interface:Node' or props.get('media.class')!=media_class:continue
        parent=devices.get(int(props.get('device.id',-1)),{})
        result.append({'id':obj['id'],'name':str(props.get('node.name','')),
                       'label':str(props.get('node.description',props.get('node.nick',props.get('node.name','Audio')))),
                       'properties':{**parent,**props}})
    return result

class Audio:
    def __init__(self,simulate=False,run=None):
        self.simulate=simulate;self.run=run or subprocess.run;self.lock=threading.Lock()
        self.busy=False;self.message='';self.fake_connected=False;self.fake_paired=False;self.fake_output=1;self.volume=50
    def command(self,args,timeout=15):
        if self.simulate:raise AssertionError('Simulation must not execute audio commands')
        try:r=self.run(args,capture_output=True,text=True,timeout=timeout,check=False)
        except subprocess.TimeoutExpired:raise ValueError('Audio action timed out; check the headphones and try again')
        if r.returncode:raise ValueError('Audio service could not complete this action: '+args[0])
        return r.stdout
    def objects(self):
        import dbus
        bus=dbus.SystemBus()
        return dbus.Interface(bus.get_object('org.bluez','/'),'org.freedesktop.DBus.ObjectManager').GetManagedObjects()
    def devices(self):
        if self.simulate:return [{'address':'02:00:00:00:00:01','name':'Preview headphones','paired':self.fake_paired,'connected':self.fake_connected}]
        result=[]
        for path,interfaces in self.objects().items():
            p=interfaces.get('org.bluez.Device1')
            if not p:continue
            # Devices with unresolved services may be offered for explicit pairing.
            uuids=[str(u).lower() for u in p.get('UUIDs',[])]
            if uuids and A2DP_SINK not in uuids:continue
            result.append({'address':str(p['Address']).upper(),'name':str(p.get('Alias',p.get('Name',p['Address']))),
                           'paired':bool(p.get('Paired',False)),'connected':bool(p.get('Connected',False))})
        return sorted(result,key=lambda x:(not x['paired'],x['name'].casefold()))
    def outputs(self):
        if self.simulate:return [{'id':1,'name':'speakers','label':'Keyboard speakers'}]+([{'id':2,'name':'headphones','label':'Preview headphones'}] if self.fake_connected else [])
        return [{k:p[k] for k in ['id','name','label']} for p in pipewire_nodes(json.loads(self.command(['pw-dump'])),'Audio/Sink')]
    def snapshot(self):
        try:return {'devices':self.devices(),'outputs':self.outputs(),'busy':self.busy,'message':self.message,'volume':self.volume,'simulation':self.simulate}
        except Exception as e:return {'devices':[],'outputs':[],'busy':self.busy,'message':'Audio service is not ready: '+str(e),'volume':self.volume,'simulation':self.simulate}
    def action(self,operation,value):
        if operation=='status':return self.snapshot()
        if operation=='output':
            ident=value.get('id')
            if type(ident) is not int or ident not in [x['id'] for x in self.outputs()]:raise ValueError('Select an available audio output')
            if self.simulate:self.fake_output=ident
            else:self.command(['wpctl','set-default',str(ident)])
            self.message='Audio output selected';return self.snapshot()
        if operation=='volume':
            level=value.get('value')
            if type(level) is not int or not 0<=level<=100:raise ValueError('Volume must be 0–100%')
            if not self.simulate:self.command(['wpctl','set-volume','@DEFAULT_AUDIO_SINK@',str(level/100)])
            self.volume=level;return {'applied':True}
        if operation not in ['scan','connect','disconnect','forget']:raise ValueError('Unknown audio action')
        address=value.get('address','')
        if not isinstance(address,str):raise ValueError('Invalid headphone address')
        address=address.upper()
        if operation!='scan':
            if not MAC.fullmatch(address) or address not in [d['address'] for d in self.devices()]:raise ValueError('Select headphones from the discovered list')
        with self.lock:
            if self.busy:raise ValueError('Wait for the current headphone action to finish')
            self.busy=True;self.message='Searching for headphones' if operation=='scan' else 'Updating headphones'
        def work():
            try:
                if self.simulate:
                    if operation in ['connect','disconnect','forget']:self.fake_connected=operation=='connect'
                    if operation=='connect':self.fake_paired=True
                    if operation=='forget':self.fake_paired=False
                    if not self.fake_connected:self.fake_output=1
                elif operation=='scan':
                    self.command(['bluetoothctl','power','on'])
                    # The client's discovery session ends when its 12-second timeout exits.
                    self.command(['bluetoothctl','--timeout','12','scan','on'],timeout=17)
                else:
                    found=next(d for d in self.devices() if d['address']==address)
                    if operation=='connect':
                        self.command(['bluetoothctl','power','on'])
                        if not found['paired']:
                            self.command(['bluetoothctl','--agent','NoInputNoOutput','--timeout','35','pair',address],timeout=40)
                        self.command(['bluetoothctl','--timeout','20','connect',address,A2DP_SINK],timeout=25)
                        # Verify state rather than interpreting a successful command exit as pairing success.
                        found=next(d for d in self.devices() if d['address']==address)
                        if not found['paired'] or not found['connected']:raise ValueError('Headphones did not connect; put them in pairing mode and try again')
                        self.command(['bluetoothctl','trust',address])
                    else:self.command(['bluetoothctl','disconnect' if operation=='disconnect' else 'remove',address])
                self.message='Headphones connected. Choose their audio output below.' if operation=='connect' else 'Headphone action completed'
            except Exception as e:self.message=str(e)
            finally:
                with self.lock:self.busy=False
        threading.Thread(target=work,daemon=True).start()
        return {'queued':True}
