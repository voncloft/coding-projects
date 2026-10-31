#pragma once
#include "Arduino.h"
using gpio_num_t=int;using RingbufHandle_t=int*;
#define RMT_CHANNEL_0 0
#define RMT_CHANNEL_2 2
#define RMT_IDLE_LEVEL_LOW 0
#define RMT_CARRIER_LEVEL_HIGH 1
#define ESP_OK 0
#define ESP_ERROR_CHECK(x) assert((x)==0)
struct rmt_item32_t {unsigned duration0:15;unsigned level0:1;unsigned duration1:15;unsigned level1:1;};
struct rmt_config_t {int clk_div=80,mem_block_num=1;struct {bool carrier_en;int carrier_freq_hz,carrier_duty_percent,idle_level;} tx_config{};struct {int filter_ticks_thresh,idle_threshold;} rx_config{};};
inline rmt_config_t RMT_DEFAULT_CONFIG_TX(int,int){return {};}
inline rmt_config_t RMT_DEFAULT_CONFIG_RX(int,int){return {};}
static std::deque<std::vector<rmt_item32_t>> ring;static std::vector<rmt_item32_t> active_ring,last_tx;
static int carrier_error=0,transmit_error=0;static bool tx_done=false;static unsigned high_ticks,low_ticks;
inline int rmt_config(rmt_config_t*){return 0;}inline int rmt_driver_install(int,int,int){return 0;}
inline int rmt_get_ringbuf_handle(int,RingbufHandle_t*h){static int token;*h=&token;return 0;}
inline int rmt_rx_stop(int){return 0;}inline int rmt_rx_start(int,bool){return 0;}
inline void*xRingbufferReceive(RingbufHandle_t,size_t*z,int){if(ring.empty())return nullptr;active_ring=ring.front();ring.pop_front();*z=active_ring.size()*sizeof(rmt_item32_t);return active_ring.data();}
inline void vRingbufferReturnItem(RingbufHandle_t,void*){active_ring.clear();}
inline int rmt_set_tx_carrier(int,bool,unsigned h,unsigned l,int){high_ticks=h;low_ticks=l;return carrier_error;}
inline int rmt_write_items(int,rmt_item32_t*p,int n,bool){last_tx.assign(p,p+n);return transmit_error;}
inline int rmt_wait_tx_done(int,int){return tx_done?0:1;}
