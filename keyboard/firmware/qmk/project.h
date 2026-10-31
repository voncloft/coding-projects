#pragma once
#include "quantum.h"
#include "wire.h"
void bridge_send(uint8_t type, const uint8_t *p, uint16_t n);
void host_select(uint8_t slot, bool pair);
void project_packet(const wire_packet *p);
void power_init(void);
void power_task(void);
void power_packet(const wire_packet *p);
bool power_keys_enabled(void);
void request_bank(uint8_t bank);

void bridge_reply(uint8_t seq,uint8_t command,uint8_t result);
bool bridge_can_tap(void);

void power_begin_programming(void);
bool power_ready_for_programming(void);
