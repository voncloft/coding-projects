#pragma once
#include <stdbool.h>
#include <stdint.h>
#define GP22 22
#define GP23 23
#define GP27 27
void gpio_set_pin_input(int p);
void gpio_set_pin_output(int p);
void gpio_write_pin_low(int p);
void gpio_write_pin_high(int p);
bool gpio_read_pin(int p);
void clear_keyboard(void);
