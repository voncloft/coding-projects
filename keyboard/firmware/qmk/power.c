// SPDX-License-Identifier: GPL-2.0-or-later
#include "i2c_master.h"
#include "project.h"
#include "timer.h"
/* Register values follow BQ25798 SLUSDV2C, MAX17049 rev7, and HUSB238 status
   map. Charge-enable stays low until identification, cell count, configuration
   and ADC agree. */
#define BQ 0x6b
#define GAUGE 0x36
#define GPIOX 0x20
#define PD 0x08
#define INPUT_BOOST 1
#define CHARGE_EN 2
#define WATCH_EN 4
#define USB_EN 8
#define RGB_EN 16
#define AUDIO_EN 32
#define IO_EN 64
static bool configured = false, valid = false, core = false, stopping = false,
            ready = false, halted = false, halt_cleared = false,
            request_previous = false, rearm = true, programming = false;
static uint32_t last_sample = 0, last_retry = 0, boot_time = 0, stop_time = 0,
                last_shutdown = 0, valid_time = 0;
static uint16_t generation = 0, battery_mv = 0, vbus_mv = 0, ts = 0;
static int16_t battery_ma = 0;
static uint8_t percent = 255, requested = RGB_EN | AUDIO_EN | USB_EN | WATCH_EN,
               granted = 0, latch = 0, reason = 0;
static bool rd(uint8_t a, uint8_t reg, uint8_t *b, uint8_t n) {
  return i2c_read_register(a << 1, reg, b, n, 5) == I2C_STATUS_SUCCESS;
}
static bool wr(uint8_t a, uint8_t reg, uint8_t v) {
  return i2c_write_register(a << 1, reg, &v, 1, 5) == I2C_STATUS_SUCCESS;
}
static bool rd16(uint8_t a, uint8_t reg, uint16_t *v) {
  uint8_t b[2];
  if (!rd(a, reg, b, 2))
    return false;
  *v = ((uint16_t)b[0] << 8) | b[1];
  return true;
}
static bool wr16(uint8_t a, uint8_t reg, uint16_t v) {
  uint8_t b[] = {v >> 8, v};
  return i2c_write_register(a << 1, reg, b, 2, 5) == I2C_STATUS_SUCCESS;
}
static bool verify16(uint8_t reg, uint16_t v) {
  uint16_t r = 0;
  return wr16(BQ, reg, v) && rd16(BQ, reg, &r) && r == v;
}
static bool modify(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t b = 0, r = 0;
  if (!rd(BQ, reg, &b, 1))
    return false;
  b = (b & ~mask) | (value & mask);
  return wr(BQ, reg, b) && rd(BQ, reg, &r, 1) && (r & mask) == (b & mask);
}
static bool output(uint8_t bits) {
  if (bits == latch)
    return true;
  if (!wr(GPIOX, 1, bits))
    return false;
  latch = bits;
  return true;
}
/* The service hub has its own input draw. Charge only after the USB-C
 * controller reports at least 1.5 A, and subsequently verify actual VBUS.
 * Legacy/default USB sources are data-only with an installed battery.
 * Adapter attach can reset EN_HIZ: source/inrush qualification still requires
 * measurements on the assembled prototype. */
static uint16_t input_contract_mv(uint8_t pd0, uint8_t pd1) {
  if (!(pd1 & 0x40)) return 0;
  uint8_t voltage = pd0 >> 4;
  if (((pd1 >> 3) & 7) == 1 && voltage >= 1 && voltage <= 6 && (pd0 & 15) >= 4) {
    static const uint16_t nominal[] = {0, 5000, 9000, 12000, 15000, 18000, 20000};
    return nominal[voltage];
  }
  if (voltage == 0 && (pd1 & 4) && (pd1 & 3)) return 5000;
  return 0;
}
static bool configure(void) {
  uint8_t ident = 0, cell = 0;
  /* TCA9534 powers up as inputs with output latches high. Set latches FIRST. */
  if (!wr(GPIOX, 1, 0) || !wr(GPIOX, 3, 0x80))
    return false;
  latch = 0;
  if (!rd(BQ, 0x48, &ident, 1) || ((ident >> 3) & 7) != 3 ||
      !rd(BQ, 0x0a, &cell, 1) || ((cell >> 6) & 3) != 1)
    return false;
  uint8_t pd0 = 0, pd1 = 0;
  bool input = rd(PD, 0, &pd0, 1) && rd(PD, 1, &pd1, 1) && input_contract_mv(pd0, pd1);
  if (!modify(0x0f, 0x3d, input ? 0 : 4) || !verify16(0x01, 840) || !verify16(0x03, 100) ||
      !verify16(0x06, 10))
    return false;
  if (!modify(0x09, 0x20, 0x20) || !modify(0x10, 0x3f, 0x04) ||
      !modify(0x11, 0x78, 0) || !modify(0x14, 0x20, 0x20))
    return false;
  if (!wr(BQ, 0x2f, 0) || !wr(BQ, 0x30, 0) || !wr(BQ, 0x2e, 0x80))
    return false;
  return true;
}
void power_init(void) {
  gpio_set_pin_input(GP22);
  gpio_write_pin_low(GP23);
  gpio_set_pin_output(GP23);
  gpio_set_pin_input(GP27);
  i2c_init();
  last_sample = timer_read32();
  configured = configure();
  last_retry = timer_read32();
}
bool power_keys_enabled(void) { return core && !stopping; }
static void stop(uint8_t why) {
  if (!core || stopping)
    return;
  stopping = true;
  ready = false;
  reason = why;
  stop_time = timer_read32();
  last_shutdown = 0;
  granted = 0;
  clear_keyboard();
}
void power_begin_programming(void) {
  programming = true;
  stop(1);
}
bool power_ready_for_programming(void) { return !core && !stopping; }
void power_packet(const wire_packet *p) {
  if (p->type == W_LOADS && p->len == 1)
    requested = p->data[0] & (WATCH_EN | USB_EN | RGB_EN | AUDIO_EN);
  if (p->type == W_READY && p->len == 2 && wire_u16(p->data) == generation &&
      core)
    ready = true;
  if (p->type == W_HALTED && p->len == 1) {
    halted = p->data[0] != 0;
    if (!halted && core)
      halt_cleared = true;
  }
  if (p->type == W_SHUTDOWN)
    stop(1);
}
void power_task(void) {
  uint32_t now = timer_read32();
  bool want = gpio_read_pin(GP22);
  if (!want)
    rearm = true;
  if (want && !request_previous)
    rearm = true;
  request_previous = want;
  if (core && !want)
    stop(1);
  if (!configured) {
    if (timer_elapsed32(last_retry) >= 3000) {
      last_retry = now;
      configured = configure();
    }
    if (core)
      stop(4);
  }
  if (configured && timer_elapsed32(last_sample) >= 500) {
    last_sample = now;
    uint16_t soc = 0, ib = 0;
    uint8_t f0 = 0, f1 = 0, st = 0, gpio = 0, pd0 = 0, pd1 = 0, wd = 0;
    valid = rd16(BQ, 0x3b, &battery_mv) && rd16(BQ, 0x35, &vbus_mv) &&
            rd16(BQ, 0x33, &ib) && rd16(BQ, 0x3f, &ts) &&
            rd16(GAUGE, 0x04, &soc) && rd(BQ, 0x20, &f0, 1) &&
            rd(BQ, 0x21, &f1, 1) && rd(BQ, 0x1d, &st, 1) &&
            rd(GPIOX, 0, &gpio, 1);
    battery_ma = (int16_t)ib;
    valid = valid && (st & 1) && battery_mv >= 5000 && battery_mv <= 8600 &&
            ts > 0 && ts < 1024;
    percent = valid ? (soc >> 8) > 100 ? 100 : (soc >> 8) : 255;
    bool pd_ok = rd(PD, 0, &pd0, 1) && rd(PD, 1, &pd1, 1);
    uint16_t contract_mv = pd_ok ? input_contract_mv(pd0, pd1) : 0;
    bool input_ok = contract_mv && vbus_mv >= (uint32_t)contract_mv * 85 / 100 &&
                    vbus_mv <= (uint32_t)contract_mv * 110 / 100 && vbus_mv <= 21000;
    bool high_pd = input_ok && (pd1 & 0x40) && ((pd1 >> 3) & 7) == 1 &&
                   (pd0 >> 4) == 6 && (pd0 & 15) >= 10 && vbus_mv >= 18000 &&
                   vbus_mv <= 21000;
    bool charger_fault = (f0 & 0x7f) || (f1 & 0xc4);
    bool temp_ok = ts >= 458 && ts <= 750;
    if (!rd(BQ, 0x10, &wd, 1) || !wr(BQ, 0x10, wd | 8)) {
      valid = false;
      percent = 255;
    }
    uint8_t out = high_pd ? INPUT_BOOST : 0;
    if (!verify16(0x06, high_pd ? 230 : input_ok ? 50 : 10)) {
      valid = false;
      out = 0;
    }
    bool charge = valid && input_ok && temp_ok && !charger_fault && vbus_mv >= 4300 &&
                  vbus_mv <= 21000 && battery_mv < 8500;
    if (!modify(0x0f, 0x24, (charge ? 0x20 : 0) | (input_ok ? 0 : 4))) {
      valid = false;
      charge = false;
    }
    /* Entering HIZ disables ADC; restore sampling for battery supervision. */
    uint8_t adc = 0;
    if (!rd(BQ, 0x2e, &adc, 1) || (!(adc & 0x80) && !wr(BQ, 0x2e, adc | 0x80))) {
      valid = false;
      charge = false;
    }
    if (charge)
      out |= CHARGE_EN;
    // Only a fully successful supervision cycle refreshes the fault timer.
    // Readable telemetry alone must not hide failing configuration writes.
    if (valid)
      valid_time = now;
    if (!valid) {
      out = 0;
      if (core && timer_elapsed32(valid_time) > 2500)
        stop(4);
    }
    if (valid && core && (percent <= 5 || battery_mv < 6200))
      stop(2);
    if (charger_fault && core)
      stop(3);
    if (valid && core && battery_ma < -5500)
      stop(3);
    if (!programming && !core && want && rearm && valid && !charger_fault &&
        (high_pd || percent > 8) && battery_mv >= 6400) {
      core = true;
      stopping = false;
      ready = false;
      halt_cleared = false;
      rearm = false;
      reason = 0;
      boot_time = now;
      generation++;
      if (!generation)
        generation++;
      gpio_write_pin_high(GP23);
    }
    granted = 0;
    if (core && !stopping && valid && !charger_fault && battery_ma > -5000) {
      /* Reserve 17 W for CM4, display and hub. Discretionary ports are shed
       * first. */
      int32_t budget =
          high_pd ? 36000 : ((int32_t)battery_mv * 5 * 85 / 100 - 800);
      int32_t left = budget - 17000;
      const uint8_t bits[] = {AUDIO_EN, RGB_EN, USB_EN, WATCH_EN};
      const uint16_t cost[] = {3000, 3500, 7650, 7650};
      for (int i = 0; i < 4; i++)
        if ((requested & bits[i]) && left >= cost[i]) {
          granted |= bits[i];
          left -= cost[i];
        }
      if (!(gpio & 0x80))
        granted &= ~WATCH_EN;
      if (granted & (USB_EN | WATCH_EN))
        granted |= IO_EN;
      out |= granted;
    }
    if (!output(out)) {
      configured = false;
      valid = false;
      if (core)
        stop(4);
    }
    if (!valid)
      percent = 255;
    uint8_t t[14] = {percent,
                     (uint8_t)((valid ? 1 : 0) | (charge ? 2 : 0) |
                               (high_pd ? 4 : 0) | (core ? 8 : 0) |
                               (stopping ? 16 : 0) | (ready ? 32 : 0))};
    wire_put16(t + 2, battery_mv);
    wire_put16(t + 4, (uint16_t)battery_ma);
    wire_put16(t + 6, vbus_mv);
    t[8] = granted;
    t[9] = reason;
    wire_put16(t + 10, generation);
    wire_put16(t + 12, ts);
    bridge_send(W_POWER, t, sizeof t);
  }
  if (core && stopping) {
    if (!last_shutdown || timer_elapsed32(last_shutdown) > 1000) {
      last_shutdown = now;
      uint8_t p[3];
      wire_put16(p, generation);
      p[2] = reason;
      bridge_send(W_SHUTDOWN, p, 3);
    }
    /* Hardware final-halt is accepted only after the line was high in this
     * boot. */
    if ((halted && halt_cleared && timer_elapsed32(stop_time) > 1000) ||
        timer_elapsed32(stop_time) > (reason == 2 ? 30000 : 120000)) {
      output(latch & (INPUT_BOOST | CHARGE_EN));
      gpio_write_pin_low(GP23);
      core = false;
      stopping = false;
      ready = false;
      halt_cleared = false;
    }
  }
  (void)boot_time;
}
