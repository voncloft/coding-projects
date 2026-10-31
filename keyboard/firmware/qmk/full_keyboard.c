// SPDX-License-Identifier: GPL-2.0-or-later
#include "dynamic_keymap.h"
#include "hardware/watchdog.h"
#include "keymap_introspection.h"
#include "project.h"
#include "bluetooth.h"
static uint8_t macro_bank = 0, pending_bank = 0, rotary_mode = 0;
static uint16_t macros_down = 0;
static uint32_t host_down[3] = {};
static bool host_pressed[3] = {};
static bool paired[3] = {};
static bool encoder_button = false, encoder_long = false;
static uint32_t encoder_down = 0;
static int macro_index(keypos_t k) {
  if (k.row == 5 && k.col >= 8)
    return k.col - 8;
  if (k.row == 6 && k.col < 3)
    return k.col + 6;
  return -1;
}
void request_bank(uint8_t bank) {
  pending_bank = bank % 4;
  if (!macros_down) {
    macro_bank = pending_bank;
    bridge_send(W_BANK, &macro_bank, 1);
  }
}
uint16_t keymap_key_to_keycode(uint8_t layer, keypos_t key) {
  if (key.row >= MATRIX_ROWS || key.col >= MATRIX_COLS)
    return KC_NO;
  if (macro_index(key) >= 0) {
    uint16_t code = dynamic_keymap_get_keycode(macro_bank, key.row, key.col);
    return code == KC_TRNS ? dynamic_keymap_get_keycode(0, key.row, key.col)
                           : code;
  }
  return keycode_at_keymap_location(layer, key.row, key.col);
}
void keyboard_pre_init_kb(void) {
  gpio_write_pin_low(GP23);
  gpio_set_pin_output(GP23);
  keyboard_pre_init_user();
}
void keyboard_post_init_kb(void) {
  power_init();
  watchdog_enable(8000, true);
  keyboard_post_init_user();
}
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
  if (!process_record_user(keycode, record))
    return false;
  if (keycode >= QK_USER_0 && keycode <= QK_USER_2) {
    uint8_t host = keycode - QK_USER_0;
    if (record->event.pressed) {
      host_down[host] = timer_read32();
      host_pressed[host] = true;
      paired[host] = false;
    } else {
      if (!paired[host])
        host_select((host == 0 && layer_state_is(1)) ? 0 : host + 1, false);
      host_down[host] = 0;
      host_pressed[host] = false;
    }
    return false;
  }
  int index = macro_index(record->event.key);
  // Release bookkeeping must survive a supervisor stop. A release while
  // CORE is off must not permanently lock the next requested macro bank.
  if (index >= 0 && !record->event.pressed)
    macros_down &= ~(1 << index);
  if (!power_keys_enabled())
    return false;
  if (index >= 0) {
    if (record->event.pressed)
      macros_down |= 1 << index;
    else
      macros_down &= ~(1 << index);
    uint8_t p[] = {macro_bank, (uint8_t)index, (uint8_t)record->event.pressed};
    bridge_send(W_MACRO, p, 3);
  }
  uint8_t p[] = {record->event.key.row, record->event.key.col,
                 (uint8_t)record->event.pressed};
  bridge_send(W_KEY_EVENT, p, 3);
  return true;
}
void project_packet(const wire_packet *p) {
  if (p->type == W_BANK && p->len == 1)
    request_bank(p->data[0]);
  else if (p->type == W_MODE && p->len == 1) {
    rotary_mode = p->data[0] ? 1 : 0;
    bridge_send(W_MODE, &rotary_mode, 1);
  } else if (p->type == W_ENCODER && p->len == 3) {
    int8_t delta = (int8_t)p->data[0];
    bool button = p->data[1] != 0;
    if (!rotary_mode && delta)
      request_bank((pending_bank + (delta > 0 ? 1 : 3)) % 4);
    if (button && !encoder_button) {encoder_down=timer_read32();encoder_long=false;}
    encoder_button = button;
  } else if (p->type == W_TAP) {
    if(p->len!=2||!power_keys_enabled()){bridge_reply(p->seq,W_TAP,1);return;}
    uint16_t code = wire_u16(p->data);
    if (code > 0 && code < 0x2000) {
      if (!bridge_can_tap()) {bridge_reply(p->seq,W_TAP,4);return;}
      // Do not release a physically held key or apply its modifiers to a macro.
      if (has_anykey() || has_anymod() || get_weak_mods()) {bridge_reply(p->seq,W_TAP,3);return;}
      uint8_t saved_mods=get_mods(),saved_weak=get_weak_mods();
      tap_code16(code);set_mods(saved_mods);set_weak_mods(saved_weak);send_keyboard_report();
      bridge_reply(p->seq,W_TAP,0);
    } else bridge_reply(p->seq,W_TAP,2);
  } else if (p->type == W_RGB && p->len == 4) {
    rgb_matrix_sethsv_noeeprom(p->data[0], p->data[1], MIN(p->data[2], 32));
    if (p->data[3])
      rgb_matrix_enable_noeeprom();
    else
      rgb_matrix_disable_noeeprom();
  }
}
void housekeeping_task_kb(void) {
  watchdog_update();
  power_task();
  if(encoder_button&&!encoder_long&&timer_elapsed32(encoder_down)>=1000){
    encoder_long=true;rotary_mode^=1;bridge_send(W_MODE,&rotary_mode,1);
  }
  for (uint8_t i = 0; i < 3; i++)
    if (host_pressed[i] && !paired[i] && timer_elapsed32(host_down[i]) >= 1800) {
      paired[i] = true;
      host_select(i + 1, true);
    }
  if (!macros_down && pending_bank != macro_bank)
    request_bank(pending_bank);
  housekeeping_task_user();
}

// A software bootloader request first lets the CM4 finish its disk writes.
// Rev G powers the USB hub independently, so removing CORE does not remove UF2.
bool shutdown_kb(bool jump_to_bootloader) {
  bool result = shutdown_user(jump_to_bootloader);
  if (jump_to_bootloader) {
    power_begin_programming();
    while (!power_ready_for_programming()) {
      watchdog_update();
      bluetooth_task();
      power_task();
      wait_ms(1);
    }
  }
  return result;
}
