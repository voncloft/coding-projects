// Execute the delivered ESP32 application with hardware boundary fixtures.
#include <cstdio>
#include "../../firmware/esp32/src/main.cpp"
static wire_packet command(uint8_t kind,std::initializer_list<uint8_t>d={}){wire_packet p{};p.type=kind;p.len=d.size();std::copy(d.begin(),d.end(),p.data);return p;}
static std::vector<wire_packet> frames(HardwareSerial&p){std::vector<wire_packet> result;wire_parser parser{};wire_packet f;for(auto b:p.out)if(wire_feed(&parser,b,&f))result.push_back(f);p.out.clear();return result;}
static void event(uint8_t kind,uint16_t h,uint32_t epoch,const char*address,uint8_t value=0){BleEvent e{};e.kind=kind;e.handle=h;e.epoch=epoch;e.value=value;strncpy(e.address,address,17);assert(xQueueSend(bleEvents,&e,0));bluetoothEvents();}
static void upload(unsigned n=6,unsigned hz=38000,unsigned us=560){auto p=command(W_IR_BEGIN);p.len=4;wire_put16(p.data,hz);wire_put16(p.data+2,n);irCommand(p);if(!irExpected)return;p=command(W_IR_CHUNK);p.len=2+2*n;assert(p.len<=WIRE_MAX);for(unsigned i=0;i<n;i++)wire_put16(p.data+2+2*i,us);irCommand(p);}
static void key(uint8_t k){uint8_t p[8]={};p[2]=k;keyboard(p);}
int main(int argc,char**argv){assert(argc==2);const char*n=argv[1];for(auto&p:pins)p=1;setup();rp.out.clear();Serial.out.clear();
 if(!strcmp(n,"sequence-255")){
   auto p=command(W_PING,{1,2,3});p.seq=255;packet(p,false);auto v=frames(Serial);assert(v.size()==1&&v[0].seq==255&&v[0].type==W_PONG);
 }else if(!strcmp(n,"soc-all-values")){
   for(unsigned s=0;s<256;s++){battery(s);assert(pwm[0]==(s<80?110:0));assert(pwm[1]==(s>=30&&s<=100?180:0));}
 }else if(!strcmp(n,"soc-stale-at-zero")){
   battery(100);clock_ms=5001;loop();assert(pwm[0]==0&&pwm[1]==0);
 }else if(!strcmp(n,"soc-wrap")){
   clock_ms=UINT32_MAX-100;battery(50);clock_ms+=5000;loop();assert(pwm[1]==180);clock_ms+=1;loop();assert(!pwm[0]&&!pwm[1]);
 }else if(!strcmp(n,"ir-cancel-upload-on-learn")){
   upload();irCommand(command(W_IR_LEARN));clock_ms+=10001;infraredTask();irCommand(command(W_IR_COMMIT));assert(!transmitting&&last_tx.empty());
 }else if(!strcmp(n,"ir-malformed-learn")){
   irCommand(command(W_IR_LEARN,{1}));assert(!learning);
 }else if(!strcmp(n,"ir-malformed-commit")){
   upload();irCommand(command(W_IR_COMMIT,{1}));assert(!transmitting);
 }else if(!strcmp(n,"ir-upload-boundaries")){
   for(unsigned hz:{24999u,60001u}){upload(6,hz);assert(irExpected==0);}upload(1);assert(!irExpected);upload(6,38000,79);irCommand(command(W_IR_COMMIT));assert(!transmitting);
   upload(6,38000,560);irCommand(command(W_IR_COMMIT));assert(transmitting&&last_tx.size()==3);assert(high_ticks+low_ticks==2105);assert(last_tx[0].level0==1&&last_tx[0].level1==0&&last_tx[0].duration0==560);tx_done=true;infraredTask();assert(!transmitting);
 }else if(!strcmp(n,"ir-incomplete-and-offset")){
   auto p=command(W_IR_BEGIN,{0x70,0x94,6,0});irCommand(p);irCommand(command(W_IR_COMMIT));assert(!transmitting&&!irExpected);
   upload();irCommand(command(W_IR_CHUNK,{6,0,1,1}));assert(!irExpected);
 }else if(!strcmp(n,"ir-duration-and-driver-faults")){
   upload(16,38000,32767);irCommand(command(W_IR_COMMIT));assert(!transmitting&&!irExpected);
   upload();carrier_error=1;irCommand(command(W_IR_COMMIT));assert(!transmitting);carrier_error=0;
   upload();transmit_error=1;irCommand(command(W_IR_COMMIT));assert(!transmitting);
 }else if(!strcmp(n,"ir-learn-valid")){
   irCommand(command(W_IR_LEARN));ring.push_back({{560,0,560,1},{560,0,1690,1},{560,0,32767,1}});infraredTask();assert(!learning&&irCount==5);auto v=frames(Serial);assert(std::any_of(v.begin(),v.end(),[](auto p){return p.type==W_IR_META&&wire_u16(p.data+2)==5;}));
 }else if(!strcmp(n,"ir-learn-short-pulse")){
   irCommand(command(W_IR_LEARN));ring.push_back({{79,0,560,1},{560,0,560,1},{560,0,32767,1}});infraredTask();auto v=frames(Serial);assert(std::none_of(v.begin(),v.end(),[](auto p){return p.type==W_IR_META;}));
 }else if(!strcmp(n,"ble-host-isolation")){
   peers[0]="01:02:03:04:05:06";event(1,1,0,"01:02:03:04:05:07");assert(!linked);
   event(1,1,0,"01:02:03:04:05:06");assert(linked&&!secured);event(4,1,0,"01:02:03:04:05:06",1);assert(secured);
   key(4);assert(keys->value[2]==4);selectHost(2,false);assert(!secured&&keys->value[2]==0);event(4,1,0,"01:02:03:04:05:06",1);assert(!secured);event(2,1,1,"01:02:03:04:05:06");assert(!linked);advertise();assert(!NimBLEDevice::adv.active);
 }else if(!strcmp(n,"ble-stale-auth-epoch")){
   selectHost(2,true);auto epoch=selectionEpoch.load();event(1,7,epoch,"01:02:03:04:05:06");event(4,7,epoch-1,"01:02:03:04:05:06",1);assert(!secured&&peers[1].empty());
 }else if(!strcmp(n,"ble-passkey")){
   selectHost(1,true);auto e=selectionEpoch.load();event(1,2,e,"01:02:03:04:05:06");event(3,2,e,"01:02:03:04:05:06");
   for(auto k:{0x1e,0x1f,0x20,0x21,0x22,0x23}){key(k);key(k);key(0);}assert(pinDigits==6&&pinValue==123456);key(0x28);assert(NimBLEDevice::passkey==123456&&!passkeyPending);assert(keys->notifications==0);
   event(4,2,e,"01:02:03:04:05:06",1);assert(secured&&peers[0]=="01:02:03:04:05:06");
 }else if(!strcmp(n,"ble-overflow-and-timeout")){
   clock_ms=UINT32_MAX-500;selectHost(1,true);clock_ms+=120000;loop();assert(!pairing);
   selectHost(1,true);event(1,2,selectionEpoch.load(),"01:02:03:04:05:06");eventOverflow=true;bluetoothEvents();assert(!secured&&!pairing&&!server->disconnected.empty());
 }else if(!strcmp(n,"ble-report-timeout")){
   secured=true;clock_ms=10;key(4);clock_ms+=1501;loop();assert(keys->value[2]==0);
 }else if(!strcmp(n,"encoder-and-debounce")){
   controls();frames(rp);pins[6]=0;controls();pins[7]=0;controls();auto v=frames(rp);assert(v.size()==1&&v[0].type==W_ENCODER&&v[0].data[0]==1);
   pins[10]=0;controls();clock_ms=14;controls();assert(frames(rp).empty());clock_ms=15;controls();v=frames(rp);assert(v.size()==1&&v[0].data[1]==1);
 }else assert(!"unknown scenario");printf("PASS %s\n",n);
}
