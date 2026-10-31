/* Selective I2C failures against the actual power supervisor, one fresh process per case. */
#define main baseline_main
#include "power_harness.c"
#undef main
int main(int argc,char **argv) {
 assert(argc==5);bool initial=!strcmp(argv[1],"initial");bool read=!strcmp(argv[2],"read");
 int addr=(int)strtol(argv[3],NULL,0),reg=(int)strtol(argv[4],NULL,0);
 if(initial) {
  regs[BQ][0x48]=3<<3;regs[BQ][0x0a]=1<<6;regs[BQ][0x1d]=1;regs[GPIOX][0]=0x80;
  value16(BQ,0x3b,7600);value16(BQ,0x35,20000);value16(BQ,0x33,(uint16_t)-2000);value16(BQ,0x3f,600);value16(GAUGE,4,80<<8);
  regs[PD][0]=0x6a;regs[PD][1]=0x48;pins[GP22]=true;
 } else {
  char *base[]={argv[0],"startup"};assert(baseline_main(2,base)==0);
  value16(BQ,0x35,20000);regs[PD][0]=0x6a;regs[PD][1]=0x48;tick(500);
  assert(valid&&core&&(regs[GPIOX][1]&CHARGE_EN));
 }
 fail_address=addr;if(read)fail_read_reg=reg;else fail_write_reg=reg;
 if(initial) {
  power_init();tick(500);
  if(addr==PD) {assert(configured&&valid&&core);assert(!(regs[GPIOX][1]&CHARGE_EN));assert(regs[BQ][0x0f]&4);}
  else {assert(!configured&&!pins[GP23]);assert(!(regs[GPIOX][1]&CHARGE_EN));}
 } else {
  if(addr==BQ && reg==0x2e && !read)regs[BQ][0x2e]&=0x7f;
  if(addr==GPIOX && !read) {wire_packet p={.type=W_LOADS,.len=1,.data={0}};power_packet(&p);}
  for(int i=0;i<7;i++)tick(500);
  if(addr==PD) {assert(valid&&core&&!stopping);assert(!(regs[GPIOX][1]&CHARGE_EN));assert(!(granted&WATCH_EN));assert(regs[BQ][0x0f]&4);}
  else {assert(stopping&&reason==4&&!valid&&telemetry[0]==255);tick(120001);assert(!pins[GP23]);}
 }
 printf("PASS %s %s address=%02x register=%02x\n",argv[1],argv[2],addr,reg);return 0;
}
