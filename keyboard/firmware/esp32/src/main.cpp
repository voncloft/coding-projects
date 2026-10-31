// SPDX-License-Identifier: GPL-2.0-or-later
#include "wire.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <Preferences.h>
#include <atomic>
#include <driver/rmt.h>
#include <freertos/queue.h>
static HardwareSerial rp(1);
static Preferences prefs;
static wire_parser rpParser{}, piParser{};
static NimBLEServer *server;
static NimBLEHIDDevice *hid;
static NimBLECharacteristic *keys, *consumer, *systemReport, *mouseReport,
    *bootKeys;
static uint8_t slot = 1, leds = 0, seqNo = 0, mode = 0;
static bool linked = false, secured = false, advertisePending = false,
            passkeyPending = false;
static uint16_t connHandle = 0xffff;
static std::atomic<uint32_t> selectionEpoch{0};
static uint32_t connectionEpoch = 0;
static uint8_t connectionSlot = 0;
struct BleEvent {
  uint8_t kind, addressType, value;
  uint16_t handle;
  uint32_t epoch;
  char address[18];
};
static QueueHandle_t bleEvents;
static std::atomic<bool> eventOverflow{false};
static void enqueueBle(uint8_t kind, NimBLEConnInfo &info, uint8_t value = 0) {
  BleEvent event{};
  event.kind = kind;
  event.handle = info.getConnHandle();
  event.epoch = selectionEpoch.load();
  event.value = value;
  auto address = info.getIdAddress();
  event.addressType = address.getType();
  auto text = address.toString();
  strncpy(event.address, text.c_str(), sizeof(event.address) - 1);
  if (xQueueSend(bleEvents, &event, 0) != pdTRUE)
    eventOverflow.store(true);
}
static uint32_t pinValue = 0, pairUntil = 0, lastPower = 0, lastReport = 0,
                lastStatus = 0;
static uint8_t pinDigits = 0, oldKeys[8] = {};
static bool pairing = false;
static bool powerReceived = false;
static String peers[3];
static uint8_t peerTypes[3];
static const uint8_t reportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xE0,
    0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08,
    0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x05, 0x07, 0x19,
    0x00, 0x2A, 0xFF, 0x00, 0x81, 0x00, 0xC0, 0x05, 0x0C, 0x09, 0x01, 0xA1,
    0x01, 0x85, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF,
    0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00, 0xC0, 0x05, 0x01, 0x09, 0x80,
    0xA1, 0x01, 0x85, 0x03, 0x15, 0x01, 0x25, 0x03, 0x19, 0x81, 0x29, 0x83,
    0x75, 0x08, 0x95, 0x01, 0x81, 0x00, 0xC0, 0x05, 0x01, 0x09, 0x02, 0xA1,
    0x01, 0x85, 0x04, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29,
    0x08, 0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x05,
    0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75,
    0x08, 0x95, 0x03, 0x81, 0x06, 0x05, 0x0C, 0x0A, 0x38, 0x02, 0x95, 0x01,
    0x81, 0x06, 0xC0, 0xC0};
static void send(HardwareSerial &port, uint8_t type, const uint8_t *data,
                 uint16_t len, int16_t seq = -1) {
  uint8_t frame[WIRE_FRAME];
  size_t n = wire_encode(type, seq < 0 ? seqNo++ : (uint8_t)seq, data, len, frame);
  port.write(frame, n);
}
static void both(uint8_t type, const uint8_t *d, uint16_t n) {
  send(rp, type, d, n);
  send(Serial, type, d, n);
}
static void status() {
  uint8_t p[] = {slot,
                 (uint8_t)(linked | (secured << 1) | (pairing << 2) |
                           (passkeyPending << 3)),
                 leds};
  both(W_HOST_STATUS, p, sizeof p);
  lastStatus = millis();
}
static void release() {
  uint8_t z[8] = {};
  if (secured) {
    keys->setValue(z, 8);
    keys->notify();
    bootKeys->setValue(z, 8);
    bootKeys->notify();
    consumer->setValue(z, 2);
    consumer->notify();
    systemReport->setValue(z, 1);
    systemReport->notify();
    mouseReport->setValue(z, 5);
    mouseReport->notify();
  }
  memset(oldKeys, 0, 8);
}
static void advertise() {
  auto adv = NimBLEDevice::getAdvertising();
  adv->stop();
  for (int i = 0; i < 3; i++)
    if (peers[i].length())
      NimBLEDevice::whiteListRemove(
          NimBLEAddress(peers[i].c_str(), peerTypes[i]));
  if (!slot)
    return;
  if (!pairing && !peers[slot - 1].length())
    return;
  if (!pairing)
    NimBLEDevice::whiteListAdd(
        NimBLEAddress(peers[slot - 1].c_str(), peerTypes[slot - 1]));
  adv->setScanFilter(false, !pairing);
  adv->start(pairing ? 120000 : 0);
  status();
}
static void selectHost(uint8_t next, bool pair) {
  if (next > 3)
    return;
  release();
  selectionEpoch.fetch_add(1);
  NimBLEDevice::getAdvertising()->stop();
  if (linked)
    server->disconnect(connHandle);
  slot = next;
  prefs.putUChar("slot", slot);
  pairing = pair && slot;
  pairUntil = millis() + 120000;
  secured = false;
  passkeyPending = false;
  advertisePending = true;
  if (pairing && peers[slot - 1].length()) {
    NimBLEDevice::deleteBond(
        NimBLEAddress(peers[slot - 1].c_str(), peerTypes[slot - 1]));
    peers[slot - 1] = "";
    prefs.putString(("p" + String(slot)).c_str(), "");
  }
  status();
}
// Callbacks only queue immutable events. UART, preferences and host selection
// are owned by loop(), so a late authentication cannot overwrite another slot.
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &i) override {
    enqueueBle(1, i);
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &i, int) override {
    enqueueBle(2, i);
  }
  void onPassKeyEntry(NimBLEConnInfo &i) override { enqueueBle(3, i); }
  void onAuthenticationComplete(NimBLEConnInfo &i) override {
    enqueueBle(4, i, i.isEncrypted() && i.isAuthenticated() && i.isBonded());
  }
};
class LedCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &i) override {
    auto v = c->getValue();
    if (v.size())
      enqueueBle(5, i, v[0]);
  }
};
static void bluetoothEvents() {
  if (eventOverflow.exchange(false)) {
    release();
    secured = false;
    pairing = false;
    selectionEpoch.fetch_add(1);
    server->disconnect(connHandle);
    advertisePending = true;
  }
  BleEvent e;
  while (xQueueReceive(bleEvents, &e, 0) == pdTRUE) {
    if (e.kind == 1) {
      NimBLEAddress address(e.address, e.addressType);
      if (e.epoch != selectionEpoch.load() || linked || !slot ||
          (!pairing && address != NimBLEAddress(peers[slot - 1].c_str(),
                                                peerTypes[slot - 1]))) {
        server->disconnect(e.handle);
        continue;
      }
      connHandle = e.handle;
      connectionEpoch = e.epoch;
      connectionSlot = slot;
      linked = true;
      secured = false;
      lastReport = millis();
      NimBLEDevice::startSecurity(connHandle);
    } else if (e.kind == 2) {
      if (e.handle != connHandle)
        continue;
      linked = false;
      secured = false;
      passkeyPending = false;
      connHandle = 0xffff;
      advertisePending = true;
    } else {
      if (!linked || e.handle != connHandle ||
          e.epoch != connectionEpoch ||
          connectionEpoch != selectionEpoch.load() || connectionSlot != slot)
        continue;
      if (e.kind == 3) {
        pinValue = 0;
        pinDigits = 0;
        passkeyPending = true;
        uint8_t v = 1;
        both(W_PASSKEY, &v, 1);
      } else if (e.kind == 4) {
        if (!e.value) {
          server->disconnect(e.handle);
          continue;
        }
        if (pairing) {
          peers[slot - 1] = e.address;
          peerTypes[slot - 1] = e.addressType;
          prefs.putString(("p" + String(slot)).c_str(), peers[slot - 1]);
          prefs.putUChar(("t" + String(slot)).c_str(), peerTypes[slot - 1]);
        }
        secured = true;
        pairing = false;
        passkeyPending = false;
        release();
      } else if (e.kind == 5 && secured)
        leds = e.value;
    }
    status();
  }
}
static void keyboard(const uint8_t *p) {
  if (passkeyPending) {
    for (int i = 2; i < 8; i++) {
      uint8_t k = p[i];
      if (!k || memchr(oldKeys + 2, k, 6))
        continue;
      if (k >= 0x1e && k <= 0x27 && pinDigits < 6) {
        pinValue = pinValue * 10 + (k == 0x27 ? 0 : k - 0x1d);
        pinDigits++;
      } else if (k == 0x2a && pinDigits) {
        pinValue /= 10;
        pinDigits--;
      } else if (k == 0x28 && pinDigits == 6) {
        NimBLEDevice::injectPassKey(server->getPeerInfoByHandle(connHandle),
                                    pinValue);
        passkeyPending = false;
      }
    }
  } else if (secured) {
    keys->setValue(p, 8);
    keys->notify();
    bootKeys->setValue(p, 8);
    bootKeys->notify();
  }
  memcpy(oldKeys, p, 8);
  lastReport = millis();
}
static constexpr int IRMAX = 1024;
static uint16_t ir[IRMAX];
static uint16_t irCount = 0, irExpected = 0, irFrequency = 38000;
static bool learning = false, transmitting = false, irOverflow = false;
static uint32_t irStarted = 0, irLast = 0;
static RingbufHandle_t irRing = nullptr;
static rmt_item32_t txItems[IRMAX];
static uint8_t irSeen[IRMAX / 8];
static void irState(uint8_t state) { send(Serial, W_IR_STATE, &state, 1); }
static void infraredInit() {
  rmt_config_t tx = RMT_DEFAULT_CONFIG_TX((gpio_num_t)0, RMT_CHANNEL_0);
  tx.clk_div = 80;
  tx.tx_config.carrier_en = true;
  tx.tx_config.carrier_freq_hz = 38000;
  tx.tx_config.carrier_duty_percent = 33;
  tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  ESP_ERROR_CHECK(rmt_config(&tx));
  ESP_ERROR_CHECK(rmt_driver_install(RMT_CHANNEL_0, 0, 0));
  rmt_config_t rx = RMT_DEFAULT_CONFIG_RX((gpio_num_t)1, RMT_CHANNEL_2);
  rx.clk_div = 80;
  rx.rx_config.filter_ticks_thresh = 80;
  rx.rx_config.idle_threshold = 32767;
  rx.mem_block_num = 2;
  ESP_ERROR_CHECK(rmt_config(&rx));
  ESP_ERROR_CHECK(rmt_driver_install(RMT_CHANNEL_2, 4096, 0));
  ESP_ERROR_CHECK(rmt_get_ringbuf_handle(RMT_CHANNEL_2, &irRing));
}
static void irCommand(const wire_packet &p) {
  // Empty commands must be exactly empty. Reject malformed uploads rather
  // than leaving a previously complete frame armed for a later commit.
  if (((p.type == W_IR_LEARN || p.type == W_IR_COMMIT) && p.len != 0) ||
      (p.type == W_IR_BEGIN && p.len != 4) ||
      (p.type == W_IR_CHUNK && (p.len < 4 || (p.len % 2) != 0))) {
    irExpected = 0;
    irState(4);
    return;
  }
  if (p.type == W_IR_LEARN) {
    if (transmitting)
      return;
    rmt_rx_stop(RMT_CHANNEL_2);
    size_t staleBytes = 0;
    void *stale;
    while ((stale = xRingbufferReceive(irRing, &staleBytes, 0)))
      vRingbufferReturnItem(irRing, stale);
    learning = true;
    irExpected = 0;
    memset(irSeen, 0, sizeof irSeen);
    irCount = 0;
    irOverflow = false;
    irStarted = irLast = millis();
    rmt_rx_start(RMT_CHANNEL_2, true);
    irState(1);
  } else if (p.type == W_IR_BEGIN && p.len == 4) {
    if (transmitting || learning)
      return;
    irFrequency = wire_u16(p.data);
    irExpected = wire_u16(p.data + 2);
    irCount = 0;
    memset(irSeen, 0, sizeof irSeen);
    if (irFrequency < 25000 || irFrequency > 60000 || irExpected < 2 ||
        irExpected > IRMAX) {
      irExpected = 0;
      irState(4);
    } else
      irState(2);
  } else if (p.type == W_IR_CHUNK && p.len >= 4 && (p.len % 2) == 0 &&
             irExpected && !transmitting && !learning) {
    uint16_t off = wire_u16(p.data);
    for (int i = 2; i < p.len; i += 2, off++) {
      if (off >= irExpected) {
        irExpected = 0;
        irState(4);
        return;
      }
      ir[off] = wire_u16(p.data + i);
      irSeen[off / 8] |= 1 << (off % 8);
    }
  } else if (p.type == W_IR_COMMIT && irExpected && !transmitting &&
             !learning) {
    uint32_t us = 0;
    for (int i = 0; i < irExpected; i++) {
      if (!(irSeen[i / 8] & (1 << (i % 8))) || ir[i] < 80 || ir[i] > 32767) {
        irState(4);
        irExpected = 0;
        return;
      }
      us += ir[i];
    }
    if (us > 500000) {
      irExpected = 0;
      irState(4);
      return;
    }
    int count = (irExpected + 1) / 2;
    memset(txItems, 0, sizeof txItems);
    for (int i = 0; i < irExpected; i++) {
      if (i % 2) {
        txItems[i / 2].level1 = 0;
        txItems[i / 2].duration1 = ir[i];
      } else {
        txItems[i / 2].level0 = 1;
        txItems[i / 2].duration0 = ir[i];
      }
    }
    // Carrier high/low uses the undivided 80 MHz APB clock, unlike the
    // 1 us pulse duration fields. Round the period before splitting duty.
    uint32_t period = (80000000u + irFrequency / 2) / irFrequency;
    if (rmt_set_tx_carrier(RMT_CHANNEL_0, true, period / 3, period - period / 3,
                           RMT_CARRIER_LEVEL_HIGH) != ESP_OK) {
      irExpected = 0;
      irState(4);
      return;
    }
    rmt_rx_stop(RMT_CHANNEL_2);
    transmitting =
        rmt_write_items(RMT_CHANNEL_0, txItems, count, false) == ESP_OK;
    irExpected = 0;
    irState(transmitting ? 3 : 4);
  }
}
static void infraredTask() {
  if (transmitting && rmt_wait_tx_done(RMT_CHANNEL_0, 0) == ESP_OK) {
    transmitting = false;
    irState(0);
  }
  size_t bytes = 0;
  auto items = (rmt_item32_t *)xRingbufferReceive(irRing, &bytes, 0);
  if (items) {
    if (learning) {
      // Save the first complete frame, not a guessed gap between remote
      // repeats. TSOP38238 outputs LOW for a carrier mark; skip the leading
      // idle HIGH.
      bool lastLevel = true;
      for (size_t i = 0; i < bytes / sizeof(rmt_item32_t); i++) {
        uint16_t d[2] = {(uint16_t)items[i].duration0,
                         (uint16_t)items[i].duration1};
        bool level[2] = {(bool)items[i].level0, (bool)items[i].level1};
        for (int j = 0; j < 2; j++) {
          if (!d[j] || (!irCount && level[j]))
            continue;
          if (irCount && level[j] == lastLevel) {
            uint32_t total = ir[irCount - 1] + d[j];
            if (total > 32767)
              irOverflow = true;
            else
              ir[irCount - 1] = total;
          } else if (irCount < IRMAX)
            ir[irCount++] = d[j];
          else
            irOverflow = true;
          lastLevel = level[j];
        }
      }
      if (irCount && (irCount % 2) == 0)
        irCount--; // Discard idle space after final mark.
      learning = false;
      rmt_rx_stop(RMT_CHANNEL_2);
      // Apply the same pulse bounds to learned frames and imported frames.
      // A frame that cannot be replayed must not be offered as a saved code.
      uint32_t totalUs = 0;
      for (uint16_t i = 0; i < irCount; i++) {
        totalUs += ir[i];
        if (ir[i] < 80 || ir[i] > 32767)
          irOverflow = true;
      }
      if (totalUs > 500000)
        irOverflow = true;
      if (irOverflow || irCount < 5)
        irState(4);
      else {
        uint8_t p[WIRE_MAX];
        wire_put16(p, 38000);
        wire_put16(p + 2, irCount);
        send(Serial, W_IR_META, p, 4);
        for (int off = 0; off < irCount; off += 120) {
          int count = min(120, (int)irCount - off);
          wire_put16(p, off);
          for (int i = 0; i < count; i++)
            wire_put16(p + 2 + 2 * i, ir[off + i]);
          send(Serial, W_IR_DATA, p, 2 + 2 * count);
        }
        irState(0);
      }
    }
    vRingbufferReturnItem(irRing, items);
  }
  if (learning && millis() - irStarted > 10000) {
    learning = false;
    rmt_rx_stop(RMT_CHANNEL_2);
    irState(4);
  }
}
static void battery(uint8_t soc) {
  lastPower = millis();
  powerReceived = true;
  if (soc > 100) {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    return;
  }
  uint8_t r = soc < 80 ? 110 : 0, g = soc >= 30 ? 180 : 0;
  ledcWrite(0, r);
  ledcWrite(1, g);
  hid->setBatteryLevel(soc);
}
static void packet(const wire_packet &p, bool fromRp) {
  if (fromRp) {
    if (p.type == W_HOST_SELECT && p.len == 2)
      selectHost(p.data[0], p.data[1]);
    else if (p.type == W_KEYS && p.len == 8)
      keyboard(p.data);
    else if (secured && p.type == W_CONSUMER && p.len == 2) {
      consumer->setValue(p.data, 2);
      consumer->notify();
      lastReport = millis();
    } else if (secured && p.type == W_SYSTEM && p.len == 1) {
      systemReport->setValue(p.data, 1);
      systemReport->notify();
      lastReport = millis();
    } else if (secured && p.type == W_MOUSE && p.len == 5) {
      mouseReport->setValue(p.data, 5);
      mouseReport->notify();
      lastReport = millis();
    } else {
      if (p.type == W_POWER && p.len >= 1)
        battery(p.data[0]);
      if (p.type == W_MODE && p.len == 1)
        mode = p.data[0];
      send(Serial, p.type, p.data, p.len, p.seq);
    }
  } else {
    if (p.type >= W_IR_LEARN && p.type <= W_IR_COMMIT)
      irCommand(p);
    else if (p.type == W_PING)
      send(Serial, W_PONG, p.data, p.len, p.seq);
    else if (p.type == W_UNPAIR && p.len == 1 && p.data[0] >= 1 &&
             p.data[0] <= 3)
      selectHost(p.data[0], true);
    else if (p.type == W_HOST_SELECT && p.len == 2) {
      send(rp, p.type, p.data, p.len, p.seq);
    } else if (p.type == W_RGB || p.type == W_BANK || p.type == W_MODE ||
               p.type == W_LOADS || p.type == W_TAP || p.type == W_READY ||
               p.type == W_SHUTDOWN)
      send(rp, p.type, p.data, p.len, p.seq);
  }
}
static uint32_t lastEncoder = 0, lastButtonEdge = 0;
static int8_t encoderSum = 0;
static uint8_t previousAB = 3;
static bool rawButton = true, button = true, halted = false;
static void controls() {
  const int8_t table[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t ab = (digitalRead(6) << 1) | digitalRead(7);
  encoderSum += table[(previousAB << 2) | ab];
  previousAB = ab;
  if (encoderSum >= 2 || encoderSum <= -2) {
    int8_t delta = encoderSum > 0 ? 1 : -1;
    encoderSum = 0;
    uint8_t p[] = {(uint8_t)delta, (uint8_t)!button, mode};
    both(W_ENCODER, p, 3);
    lastEncoder = millis();
  }
  bool b = digitalRead(10);
  if (b != rawButton) {
    rawButton = b;
    lastButtonEdge = millis();
  }
  if (b != button && millis() - lastButtonEdge >= 15) {
    button = b;
    uint8_t p[] = {0, (uint8_t)!button, mode};
    both(W_ENCODER, p, 3);
  }
  bool h = !digitalRead(3);
  if (h != halted) {
    halted = h;
    uint8_t p = (uint8_t)h;
    both(W_HALTED, &p, 1);
  }
}
void setup() {
  bleEvents = xQueueCreate(24, sizeof(BleEvent));
  configASSERT(bleEvents);
  Serial.begin(115200, SERIAL_8N1, 20, 21);
  rp.begin(115200, SERIAL_8N1, 5, 4);
  prefs.begin("keyboard", false);
  slot = prefs.getUChar("slot", 1);
  if (slot > 3)
    slot = 1;
  for (int i = 0; i < 3; i++) {
    peers[i] = prefs.getString(("p" + String(i + 1)).c_str(), "");
    peerTypes[i] = prefs.getUChar(("t" + String(i + 1)).c_str(), 0);
  }
  for (int pin : {6, 7, 10, 3})
    pinMode(pin, INPUT);
  ledcSetup(0, 1000, 8);
  ledcSetup(1, 1000, 8);
  ledcAttachPin(18, 0);
  ledcAttachPin(19, 1);
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  NimBLEDevice::init("Full Keyboard");
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
  NimBLEDevice::setMTU(64);
  server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  hid = new NimBLEHIDDevice(server);
  hid->setManufacturer("Personal keyboard");
  hid->setPnp(0x02, 0xFEED, 0x0108, 0x0100);
  hid->setHidInfo(0, 1);
  hid->setReportMap((uint8_t *)reportMap, sizeof reportMap);
  keys = hid->getInputReport(1);
  consumer = hid->getInputReport(2);
  systemReport = hid->getInputReport(3);
  mouseReport = hid->getInputReport(4);
  bootKeys = hid->getBootInput();
  hid->getOutputReport(1)->setCallbacks(new LedCallbacks());
  hid->getBootOutput()->setCallbacks(new LedCallbacks());
  hid->startServices();
  auto adv = NimBLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);
  adv->addServiceUUID(hid->getHidService()->getUUID());
  adv->enableScanResponse(true);
  adv->setName("Full Keyboard");
  infraredInit();
  advertisePending = true;
  uint8_t v = 1;
  both(W_HELLO, &v, 1);
}
void loop() {
  bluetoothEvents();
  wire_packet p;
  for (int i = 0; i < 512 && rp.available(); i++)
    if (wire_feed(&rpParser, rp.read(), &p))
      packet(p, true);
  for (int i = 0; i < 512 && Serial.available(); i++)
    if (wire_feed(&piParser, Serial.read(), &p))
      packet(p, false);
  if (advertisePending && !linked) {
    advertisePending = false;
    advertise();
  }
  if (pairing && (int32_t)(millis() - pairUntil) >= 0) {
    pairing = false;
    if (linked && !secured)
      server->disconnect(connHandle);
    advertisePending = true;
  }
  if (secured && millis() - lastReport > 1500) {
    release();
    lastReport = millis();
  }
  if (powerReceived && millis() - lastPower > 5000) {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    powerReceived = false;
  }
  if (millis() - lastStatus > 1000)
    status();
  controls();
  infraredTask();
  delay(1);
}
