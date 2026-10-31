// SPDX-License-Identifier: GPL-2.0-or-later
#include "bluetooth.h"
#include "connection.h"
#include "project.h"
#include "timer.h"
#include "uart.h"
#include "usb_device_state.h"
static bool started = false, linked = false;
static bool authenticated = false;
static uint8_t leds = 0, slot = 0, sequence = 0;
static uint32_t last_status = 0, last_refresh = 0;
static wire_parser parser;
static uint8_t key_report[8] = {}, mouse_report[5] = {}, system_usage = 0;
static uint16_t consumer_usage = 0;
void bridge_send(uint8_t type, const uint8_t *p, uint16_t n) {
  if (!started)
    return;
  uint8_t frame[WIRE_FRAME];
  size_t length = wire_encode(type, sequence++, p, n, frame);
  uart_transmit(frame, length);
}
void bridge_reply(uint8_t seq,uint8_t command,uint8_t result) {
  if(!started)return;
  uint8_t data[]={command,result},frame[WIRE_FRAME];
  size_t n=wire_encode(W_ACK,seq,data,2,frame);uart_transmit(frame,n);
}
void bluetooth_init(void) {
  uart_init(115200);
  started = true;
  uint8_t v = 1;
  bridge_send(W_HELLO, &v, 1);
}
bool bluetooth_is_connected(void) {
  return linked && timer_elapsed32(last_status) < 2500;
}
bool bridge_can_tap(void) {
  if (connection_get_host() == CONNECTION_HOST_USB)
    return usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED;
  return connection_get_host() == CONNECTION_HOST_BLUETOOTH &&
         bluetooth_is_connected() && authenticated;
}
bool bluetooth_can_send_nkro(void) { return false; }
uint8_t bluetooth_keyboard_leds(void) { return leds; }
void bluetooth_send_keyboard(report_keyboard_t *r) {
  key_report[0] = r->mods;
  key_report[1] = 0;
  memcpy(key_report + 2, r->keys, 6);
  bridge_send(W_KEYS, key_report, 8);
}
void bluetooth_send_nkro(report_nkro_t *r) { (void)r; }
void bluetooth_send_consumer(uint16_t usage) {
  consumer_usage = usage;
  uint8_t b[2];
  wire_put16(b, usage);
  bridge_send(W_CONSUMER, b, 2);
}
void bluetooth_send_system(uint16_t usage) {
  system_usage = usage >= 0x81 && usage <= 0x83 ? usage - 0x80 : 0;
  bridge_send(W_SYSTEM, &system_usage, 1);
}
void bluetooth_send_mouse(report_mouse_t *r) {
  mouse_report[0] = r->buttons;
  mouse_report[1] = r->x;
  mouse_report[2] = r->y;
  mouse_report[3] = r->v;
  mouse_report[4] = r->h;
  bridge_send(W_MOUSE, mouse_report, 5);
}
void bluetooth_send_raw_hid(uint8_t *data, uint8_t length) {
  (void)data;
  (void)length; /* VIA uses USB Raw HID. */
}
void host_select(uint8_t next, bool pair) {
  if (next > 3)
    return;
  clear_keyboard();
  memset(key_report, 0, 8);
  consumer_usage = 0;
  system_usage = 0;
  memset(mouse_report, 0, 5);
  bridge_send(W_KEYS, key_report, 8);
  uint8_t b[] = {next, (uint8_t)pair};
  bridge_send(W_HOST_SELECT, b, 2);
  slot = next;
  linked = false;
  authenticated = false;
  connection_set_host(next ? CONNECTION_HOST_BLUETOOTH : CONNECTION_HOST_USB);
}
void bluetooth_task(void) {
  wire_packet p;
  for (int i = 0; i < 512 && uart_available(); i++) {
    uint8_t b = uart_read();
    if (!wire_feed(&parser, b, &p))
      continue;
    if (p.type == W_HOST_STATUS && p.len == 3) {
      last_status = timer_read32();
      linked = p.data[1] & 1;
      authenticated = p.data[1] & 2;
      leds = p.data[2];
      if (p.data[0] != slot) {
        slot = p.data[0];
        connection_set_host_noeeprom(slot ? CONNECTION_HOST_BLUETOOTH
                                          : CONNECTION_HOST_USB);
      }
    } else if (p.type == W_HOST_SELECT && p.len == 2)
      host_select(p.data[0], p.data[1]);
    else {
      power_packet(&p);
      project_packet(&p);
    }
  }
  if (slot && timer_elapsed32(last_refresh) >= 400) {
    last_refresh = timer_read32();
    bridge_send(W_KEYS, key_report, 8);
    uint8_t b[2];
    wire_put16(b, consumer_usage);
    bridge_send(W_CONSUMER, b, 2);
    bridge_send(W_SYSTEM, &system_usage, 1);
  }
}
