/* Executes the shipped power.c against a register bus and GPIO fixture. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/qmk/power.c"
static uint8_t regs[128][256], telemetry[14];
static bool pins[30], fail_bus;
static int fail_read_reg=-1,fail_write_reg=-1,corrupt_read_reg=-1,fail_address=-1;
static uint32_t clock_ms;
static unsigned writes, shutdowns, clears;
void i2c_init(void) {}
void gpio_set_pin_input(int p) {(void)p;}
void gpio_set_pin_output(int p) {(void)p;}
void gpio_write_pin_low(int p) {pins[p]=false;}
void gpio_write_pin_high(int p) {pins[p]=true;}
bool gpio_read_pin(int p) {return pins[p];}
void clear_keyboard(void) {clears++;}
uint32_t timer_read32(void) {return clock_ms;}
uint32_t timer_elapsed32(uint32_t t) {return clock_ms-t;}
int i2c_read_register(uint8_t a,uint8_t r,uint8_t *p,uint16_t n,uint16_t timeout) {
 (void)timeout;if(fail_bus||(r==fail_read_reg && (fail_address<0 || (a>>1)==fail_address)))return -1;memcpy(p,&regs[a>>1][r],n);if(r==corrupt_read_reg)p[0]^=0x20;return 0;
}
int i2c_write_register(uint8_t a,uint8_t r,const uint8_t *p,uint16_t n,uint16_t timeout) {
 (void)timeout;if(fail_bus||(r==fail_write_reg && (fail_address<0 || (a>>1)==fail_address)))return -1;
 if(writes==0)assert((a>>1)==GPIOX&&r==1&&p[0]==0);
 if(writes==1)assert((a>>1)==GPIOX&&r==3&&p[0]==0x80);
 if((a>>1)==BQ && r==0x0f && (p[0]&4) && !(regs[BQ][0x0f]&4))regs[BQ][0x2e]&=0x7f;
 writes++;memcpy(&regs[a>>1][r],p,n);return 0;
}
void bridge_send(uint8_t type,const uint8_t *p,uint16_t n) {
 if(type==W_POWER){assert(n==14);memcpy(telemetry,p,n);}
 if(type==W_SHUTDOWN){assert(n==3);shutdowns++;}
}
static void value16(int a,int reg,uint16_t v) {regs[a][reg]=v>>8;regs[a][reg+1]=v;}
static void tick(unsigned dt) {clock_ms+=dt;power_task();}
static void halted_packet(bool state) {wire_packet p={.type=W_HALTED,.len=1,.data={state}};power_packet(&p);}
int main(int argc,char **argv) {
 assert(argc==2);const char *test=argv[1];
 regs[BQ][0x48]=3<<3;regs[BQ][0x0a]=1<<6;regs[BQ][0x1d]=1;regs[GPIOX][0]=0x80;
 value16(BQ,0x3b,7600);value16(BQ,0x35,0);value16(BQ,0x33,(uint16_t)-2000);value16(BQ,0x3f,600);value16(GAUGE,4,80<<8);
 pins[GP22]=true;
 if(!strcmp(test,"wrong-part"))regs[BQ][0x48]=0;
 if(!strcmp(test,"wrong-cell-count"))regs[BQ][0x0a]=0;
 if(!strcmp(test,"open-ntc"))value16(BQ,0x3f,1023);
 if(!strcmp(test,"no-battery"))regs[BQ][0x1d]=0;
 if(!strcmp(test,"charge-while-off"))pins[GP22]=false;
 if(!strcmp(test,"low-start"))value16(GAUGE,4,5<<8);
 if(!strcmp(test,"pd")||!strcmp(test,"charge-while-off")||!strcmp(test,"hot-ntc")){
  value16(BQ,0x35,20000);regs[PD][0]=0x6a;regs[PD][1]=0x48;
 }
 if(!strcmp(test,"hot-ntc"))value16(BQ,0x3f,400);
 if(!strcmp(test,"default-usb")||!strcmp(test,"typec-1a5")||!strcmp(test,"false-pd-voltage")) {
  value16(BQ,0x35,5000);regs[PD][1]=!strcmp(test,"typec-1a5")?0x45:0x44;
  if(!strcmp(test,"false-pd-voltage")){regs[PD][0]=0x6a;regs[PD][1]=0x48;}
 }
 power_init();tick(500);
 if(!strcmp(test,"wrong-part")||!strcmp(test,"wrong-cell-count")||!strcmp(test,"no-battery")||!strcmp(test,"low-start")){
  assert(!pins[GP23]);assert(!(regs[GPIOX][1]&CHARGE_EN));
 } else if(!strcmp(test,"charge-while-off")){
  assert(!pins[GP23]);assert(regs[GPIOX][1]&CHARGE_EN);assert(regs[BQ][0x01]==3&&regs[BQ][0x02]==0x48);assert(regs[BQ][0x03]==0&&regs[BQ][0x04]==100);
 } else {
  assert(pins[GP23]);
  if(!strcmp(test,"startup")){assert(telemetry[0]==80);assert(telemetry[1]&1);assert(regs[BQ][0x06]==0&&regs[BQ][0x07]==10);assert(regs[GPIOX][1]&(AUDIO_EN|RGB_EN));}
  else if(!strcmp(test,"default-usb")||!strcmp(test,"false-pd-voltage")) {
   assert(regs[BQ][0x0f]&4);assert(!(regs[GPIOX][1]&CHARGE_EN));assert(regs[BQ][0x07]==10);assert(regs[BQ][0x2e]&0x80);
  } else if(!strcmp(test,"typec-1a5")) {
   assert(!(regs[BQ][0x0f]&4));assert(regs[GPIOX][1]&CHARGE_EN);assert(regs[BQ][0x07]==50);
  }
  else if(!strcmp(test,"pd")){assert(regs[BQ][0x07]==230);assert(regs[GPIOX][1]&CHARGE_EN);assert(regs[GPIOX][1]&USB_EN);assert(!(regs[GPIOX][1]&WATCH_EN));wire_packet p={.type=W_LOADS,.len=1,.data={WATCH_EN}};power_packet(&p);tick(500);assert(regs[GPIOX][1]&WATCH_EN);}
  else if(!strcmp(test,"open-ntc")||!strcmp(test,"hot-ntc"))assert(!(regs[GPIOX][1]&CHARGE_EN));
  else if(!strcmp(test,"low-shutdown")){value16(GAUGE,4,5<<8);tick(500);assert(stopping&&reason==2);assert(shutdowns);tick(30001);assert(!pins[GP23]);}
  else if(!strcmp(test,"overcurrent")){value16(BQ,0x33,(uint16_t)-5600);tick(500);assert(stopping&&reason==3);}
  else if(!strcmp(test,"fault")){regs[BQ][0x20]=0x08;tick(500);assert(stopping&&reason==3);assert(!(regs[GPIOX][1]&CHARGE_EN));}
  else if(!strcmp(test,"bus-loss")){fail_bus=true;tick(3000);assert(stopping&&reason==4);tick(120001);assert(!pins[GP23]);}
  else if(!strcmp(test,"input-limit-write-failure")){
   fail_write_reg=0x06;for(int i=0;i<7;i++)tick(500);
   assert(stopping&&reason==4);assert(telemetry[0]==255);
  }else if(!strcmp(test,"charge-register-readback-failure")){
   corrupt_read_reg=0x0f;for(int i=0;i<7;i++)tick(500);
   assert(stopping&&reason==4);assert(telemetry[0]==255);
  }else if(!strcmp(test,"adc-enable-write-failure")){
   regs[BQ][0x2e]=0;fail_write_reg=0x2e;for(int i=0;i<7;i++)tick(500);
   assert(stopping&&reason==4);assert(telemetry[0]==255);
  }else if(!strcmp(test,"invalid-telemetry-light")){
   fail_write_reg=0x06;tick(500);assert(!(telemetry[1]&1)&&telemetry[0]==255);
  }else if(!strcmp(test,"power-timer-wrap")){
   clock_ms=UINT32_MAX-1000;last_sample=clock_ms;valid_time=clock_ms;
   tick(500);assert(valid&&!stopping);fail_bus=true;tick(3000);assert(stopping&&reason==4);tick(120001);assert(!pins[GP23]);
  }else if(!strcmp(test,"watch-fault")){
   value16(BQ,0x35,20000);regs[PD][0]=0x6a;regs[PD][1]=0x48;
   wire_packet p={.type=W_LOADS,.len=1,.data={WATCH_EN}};power_packet(&p);tick(500);assert(granted&WATCH_EN);
   regs[GPIOX][0]=0;tick(500);assert(!(granted&WATCH_EN)&&!stopping);
  }else if(!strcmp(test,"all-load-combinations")){
   value16(BQ,0x35,20000);regs[PD][0]=0x6a;regs[PD][1]=0x48;
   const uint8_t bits[]={AUDIO_EN,RGB_EN,USB_EN,WATCH_EN};const int costs[]={3000,3500,7650,7650};
   for(int mask=0;mask<16;mask++){
    uint8_t request=0;for(int i=0;i<4;i++)if(mask&(1<<i))request|=bits[i];
    wire_packet p={.type=W_LOADS,.len=1,.data={request}};power_packet(&p);tick(500);
    int draw=17000;for(int i=0;i<4;i++)if(granted&bits[i]){assert(request&bits[i]);draw+=costs[i];}
    assert(draw<=36000);assert((bool)(granted&IO_EN)==(bool)(granted&(USB_EN|WATCH_EN)));
   }
  }
  else if(!strcmp(test,"halt-sequence")){
   halted_packet(true);pins[GP22]=false;tick(500);tick(1500);assert(pins[GP23]);
   halted_packet(false);halted_packet(true);tick(1);assert(!pins[GP23]);
  } else if(!strcmp(test,"ready-generation")){
   wire_packet p={.type=W_READY,.len=2};wire_put16(p.data,generation+1);power_packet(&p);assert(!ready);
   wire_put16(p.data,generation);power_packet(&p);assert(ready);
  } else if(!strcmp(test,"shed-loads")){value16(BQ,0x33,(uint16_t)-5100);tick(500);assert(!granted);assert(!stopping);}
  else if(!strcmp(test,"programming-halt")){
   halted_packet(false);power_begin_programming();assert(!power_ready_for_programming());
   tick(1500);assert(pins[GP23]&&shutdowns);halted_packet(true);tick(1);
   assert(power_ready_for_programming()&&!pins[GP23]);
   pins[GP22]=false;tick(500);pins[GP22]=true;tick(500);assert(!pins[GP23]);
  } else if(!strcmp(test,"programming-timeout")){
   power_begin_programming();tick(120001);assert(power_ready_for_programming()&&!pins[GP23]);
   tick(500);assert(!pins[GP23]);
  } else if(!strcmp(test,"no-restart-after-low")){value16(GAUGE,4,5<<8);tick(500);tick(30001);value16(GAUGE,4,90<<8);tick(500);assert(!pins[GP23]);pins[GP22]=false;tick(500);pins[GP22]=true;tick(500);assert(pins[GP23]);}
 }
 printf("PASS %s\n",test);return 0;
}
