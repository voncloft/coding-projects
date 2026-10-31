/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KEYBOARD_WIRE_H
#define KEYBOARD_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define WIRE_MAX 256
#define WIRE_FRAME 272
#define WIRE_VERSION 1
/* COBS-delimited packets, version/type/sequence/lengthLE/payload/CRC16-CCITT.
 */
enum wire_type {
  W_HELLO = 1,
  W_HOST_SELECT,
  W_HOST_STATUS,
  W_KEYS,
  W_CONSUMER,
  W_SYSTEM,
  W_MOUSE,
  W_POWER,
  W_KEY_EVENT,
  W_ENCODER,
  W_MACRO,
  W_BANK,
  W_MODE,
  W_LOADS,
  W_SHUTDOWN,
  W_HALTED,
  W_TAP,
  W_PING,
  W_PONG,
  W_IR_LEARN,
  W_IR_META,
  W_IR_DATA,
  W_IR_BEGIN,
  W_IR_CHUNK,
  W_IR_COMMIT,
  W_IR_STATE,
  W_SOC,
  W_READY,
  W_RGB,
  W_CONFIG,
  W_PASSKEY,
  W_CONFIRM,
  W_UNPAIR,
  W_ACK
};
typedef struct {
  uint8_t type, seq;
  uint16_t len;
  uint8_t data[WIRE_MAX];
} wire_packet;
typedef struct {
  uint8_t bytes[WIRE_FRAME];
  uint16_t used;
  bool dropping;
  uint32_t bad;
} wire_parser;
static inline uint16_t wire_u16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline void wire_put16(uint8_t *p, uint16_t v) {
  p[0] = v;
  p[1] = v >> 8;
}
static inline uint16_t wire_crc(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) {
    c ^= (uint16_t)*p++ << 8;
    for (int i = 0; i < 8; i++)
      c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}
static inline size_t wire_encode(uint8_t type, uint8_t seq, const uint8_t *p,
                                 uint16_t n, uint8_t *out) {
  if (n > WIRE_MAX)
    return 0;
  uint8_t raw[WIRE_MAX + 7];
  raw[0] = WIRE_VERSION;
  raw[1] = type;
  raw[2] = seq;
  wire_put16(raw + 3, n);
  if (n)
    memcpy(raw + 5, p, n);
  wire_put16(raw + 5 + n, wire_crc(raw, n + 5));
  size_t at = 1, code_at = 0;
  uint8_t code = 1;
  for (size_t i = 0; i < n + 7u; i++) {
    if (raw[i]) {
      out[at++] = raw[i];
      if (++code == 255) {
        out[code_at] = code;
        code_at = at++;
        code = 1;
      }
    } else {
      out[code_at] = code;
      code_at = at++;
      code = 1;
    }
  }
  out[code_at] = code;
  out[at++] = 0;
  return at;
}
static inline bool wire_feed(wire_parser *s, uint8_t b, wire_packet *out) {
  if (b) {
    if (s->dropping)
      return false;
    if (s->used == WIRE_FRAME) {
      s->dropping = true;
      s->bad++;
      return false;
    }
    s->bytes[s->used++] = b;
    return false;
  }
  if (s->dropping) {
    s->dropping = false;
    s->used = 0;
    return false;
  }
  if (!s->used)
    return false;
  uint8_t raw[WIRE_MAX + 7];
  size_t in = 0, n = 0;
  bool ok = true;
  while (in < s->used) {
    uint8_t code = s->bytes[in++];
    if (!code || in + code - 1 > s->used) {
      ok = false;
      break;
    }
    for (int i = 1; i < code; i++) {
      if (n >= sizeof(raw)) {
        ok = false;
        break;
      }
      raw[n++] = s->bytes[in++];
    }
    if (!ok)
      break;
    if (code != 255 && in < s->used) {
      if (n >= sizeof(raw)) {
        ok = false;
        break;
      }
      raw[n++] = 0;
    }
  }
  s->used = 0;
  if (!ok || n < 7 || raw[0] != WIRE_VERSION || wire_u16(raw + 3) > WIRE_MAX ||
      n != (size_t)wire_u16(raw + 3) + 7 ||
      wire_u16(raw + n - 2) != wire_crc(raw, n - 2)) {
    s->bad++;
    return false;
  }
  out->type = raw[1];
  out->seq = raw[2];
  out->len = wire_u16(raw + 3);
  if (out->len)
    memcpy(out->data, raw + 5, out->len);
  return true;
}
#endif
