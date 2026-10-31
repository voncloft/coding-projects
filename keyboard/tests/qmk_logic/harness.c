// Execute the delivered QMK application; only its hardware/framework API is mocked.
#include <assert.h>
#include <stdio.h>
#include "../../firmware/qmk/full_keyboard.c"
static uint32_t now;static bool enabled=true,connected=true,held=false,fn=false;
static uint8_t mods,weak,reply=255,last_host,last_brightness;static bool last_pair;
static unsigned taps,host_calls;static uint16_t maps[4][8][14];
uint32_t timer_read32(void){return now;}uint32_t timer_elapsed32(uint32_t t){return now-t;}
void bridge_send(uint8_t t,const uint8_t*p,uint16_t n){(void)t;(void)p;(void)n;}
void bridge_reply(uint8_t s,uint8_t c,uint8_t r){(void)s;(void)c;reply=r;}
bool bridge_can_tap(void){return connected;}
void host_select(uint8_t s,bool p){last_host=s;last_pair=p;host_calls++;}
void power_init(void){}void power_task(void){}bool power_keys_enabled(void){return enabled;}
void power_begin_programming(void){}bool power_ready_for_programming(void){return true;}
void bluetooth_task(void){}void watchdog_update(void){}void watchdog_enable(uint32_t t,bool b){(void)t;(void)b;}
void gpio_write_pin_low(int p){(void)p;}void gpio_set_pin_output(int p){(void)p;}
void keyboard_pre_init_user(void){}void keyboard_post_init_user(void){}void housekeeping_task_user(void){}
bool process_record_user(uint16_t k,keyrecord_t*r){(void)k;(void)r;return true;}
bool layer_state_is(uint8_t l){return l==1&&fn;}bool has_anykey(void){return held;}bool has_anymod(void){return mods!=0;}
uint8_t get_mods(void){return mods;}uint8_t get_weak_mods(void){return weak;}
void set_mods(uint8_t m){mods=m;}void set_weak_mods(uint8_t m){weak=m;}
void tap_code16(uint16_t k){(void)k;taps++;}void send_keyboard_report(void){}
void rgb_matrix_sethsv_noeeprom(uint8_t h,uint8_t s,uint8_t v){(void)h;(void)s;last_brightness=v;}
void rgb_matrix_enable_noeeprom(void){}void rgb_matrix_disable_noeeprom(void){}
bool shutdown_user(bool b){return b;}void wait_ms(uint32_t ms){now+=ms;}
uint16_t dynamic_keymap_get_keycode(uint8_t l,uint8_t r,uint8_t c){assert(l<4&&r<8&&c<14);return maps[l][r][c];}
uint16_t keycode_at_keymap_location(uint8_t l,uint8_t r,uint8_t c){(void)l;(void)r;(void)c;return 4;}
static void event(uint16_t code,uint8_t row,uint8_t col,bool down){keyrecord_t k={.event={.key={.row=row,.col=col},.pressed=down}};process_record_kb(code,&k);}
int main(int argc,char**argv){
 assert(argc==2);const char*n=argv[1];
 if(!strcmp(n,"release-during-off")){
   event(4,5,8,true);request_bank(2);assert(macro_bank==0);
   enabled=false;event(4,5,8,false);enabled=true;housekeeping_task_kb();
   assert(macros_down==0&&macro_bank==2);
 }else if(!strcmp(n,"host-at-timer-zero")){
   event(QK_USER_0,7,0,true);now=1800;housekeeping_task_kb();
   assert(host_calls==1&&last_pair&&last_host==1);
   event(QK_USER_0,7,0,false);assert(host_calls==1);
 }else if(!strcmp(n,"queued-encoder-steps")){
   event(4,5,8,true);wire_packet p={.type=W_ENCODER,.len=3,.data={1,0,0}};
   project_packet(&p);project_packet(&p);project_packet(&p);
   event(4,5,8,false);housekeeping_task_kb();assert(macro_bank==3);
 }else if(!strcmp(n,"banks-and-keymap")){
   maps[0][5][8]=9;maps[2][5][8]=KC_TRNS;request_bank(2);
   assert(keymap_key_to_keycode(0,(keypos_t){.row=5,.col=8})==9);
   for(int i=0;i<9;i++){uint8_t r=i<6?5:6,c=i<6?8+i:i-6;event(4,r,c,true);}
   assert(macros_down==511);request_bank(3);assert(macro_bank==2);
   for(int i=0;i<9;i++)event(4,i<6?5:6,i<6?8+i:i-6,false);
   housekeeping_task_kb();assert(macro_bank==3&&!macros_down);
   assert(keymap_key_to_keycode(0,(keypos_t){.row=8,.col=0})==KC_NO);
 }else if(!strcmp(n,"tap-gates")){
   wire_packet p={.type=W_TAP,.len=2,.data={4,0}};
   project_packet(&p);assert(reply==0&&taps==1);
   held=true;project_packet(&p);assert(reply==3&&taps==1);held=false;
   mods=2;project_packet(&p);assert(reply==3&&mods==2);mods=0;
   weak=2;project_packet(&p);assert(reply==3&&weak==2);weak=0;
   connected=false;project_packet(&p);assert(reply==4);connected=true;
   enabled=false;project_packet(&p);assert(reply==1);enabled=true;
   p.data[1]=0x20;project_packet(&p);assert(reply==2);
   p.len=1;project_packet(&p);assert(reply==1&&taps==1);
 }else if(!strcmp(n,"encoder-hold-wrap")){
   now=UINT32_MAX-500;wire_packet p={.type=W_ENCODER,.len=3,.data={0,1,0}};project_packet(&p);
   now+=999;housekeeping_task_kb();assert(rotary_mode==0);
   now++;housekeeping_task_kb();assert(rotary_mode==1);
   now+=2000;housekeeping_task_kb();assert(rotary_mode==1);
   p.data[1]=0;project_packet(&p);p.data[1]=1;project_packet(&p);now+=1000;housekeeping_task_kb();assert(rotary_mode==0);
 }else if(!strcmp(n,"host-short-and-wrap")){
   now=UINT32_MAX-100;event(QK_USER_0+2,7,2,true);now+=1800;housekeeping_task_kb();assert(last_pair&&last_host==3);
   event(QK_USER_0+2,7,2,false);assert(host_calls==1);
   fn=true;event(QK_USER_0,7,0,true);event(QK_USER_0,7,0,false);assert(last_host==0&&!last_pair);
 }else if(!strcmp(n,"rgb-cap")){
   wire_packet p={.type=W_RGB,.len=4,.data={1,2,255,1}};project_packet(&p);assert(last_brightness==32);
 }else assert(!"unknown scenario");
 printf("PASS %s\n",n);return 0;
}
