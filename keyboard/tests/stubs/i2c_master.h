#pragma once
#include <stdint.h>
#define I2C_STATUS_SUCCESS 0
void i2c_init(void);
int i2c_read_register(uint8_t a,uint8_t r,uint8_t *p,uint16_t n,uint16_t timeout);
int i2c_write_register(uint8_t a,uint8_t r,const uint8_t *p,uint16_t n,uint16_t timeout);
