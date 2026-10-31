#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "timer.h"
#define GP23 23
#define MATRIX_ROWS 8
#define MATRIX_COLS 14
#define KC_NO 0
#define KC_TRNS 1
#define QK_USER_0 0x7e40
#define QK_USER_2 (QK_USER_0+2)
#define MIN(a,b) ((a)<(b)?(a):(b))
typedef struct {uint8_t col,row;} keypos_t;
typedef struct {struct {keypos_t key;bool pressed;} event;} keyrecord_t;
void gpio_write_pin_low(int p);void gpio_set_pin_output(int p);
void keyboard_pre_init_user(void);void keyboard_post_init_user(void);
bool process_record_user(uint16_t k,keyrecord_t*r);bool layer_state_is(uint8_t l);
bool has_anykey(void);bool has_anymod(void);uint8_t get_mods(void);uint8_t get_weak_mods(void);
void set_mods(uint8_t m);void set_weak_mods(uint8_t m);void tap_code16(uint16_t k);void send_keyboard_report(void);
void rgb_matrix_sethsv_noeeprom(uint8_t h,uint8_t s,uint8_t v);
void rgb_matrix_enable_noeeprom(void);void rgb_matrix_disable_noeeprom(void);
void housekeeping_task_user(void);bool shutdown_user(bool b);void wait_ms(uint32_t ms);
