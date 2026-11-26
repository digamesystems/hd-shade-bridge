// SPDX-License-Identifier: GPL-2.0-or-later
// Checked against HD_Remote_Clone 30e0c3f (2026-11-26): src/rf_tools.cpp
// prepareWake(), rotateBurst(), sendGroup(), sendPosition(); include/pins.h.
#include "radio.h"
#include <Preferences.h>
#include <RF24.h>
#include <SPI.h>

namespace radio {

// Same on HD_Remote_Clone's breadboard and handheld PCB rev 1 (include/pins.h):
// nRF24 on VSPI (SCK 18, MISO 19, MOSI 23), CE 17, CSN 5.
static const uint8_t PIN_CE = 17, PIN_CSN = 5;
// Both of those boards drive their IR LEDs from GPIO 4 (HIGH = on). The bridge
// never sends IR, so hold it low rather than leave it floating.
static const uint8_t PIN_IR_TX = 4;

// hd_rf.h: kChannels, kAddrWidth, kAddress, kPayloadSize.
static const uint8_t CHANNELS[3] = {33, 52, 71};
static const uint8_t ADDR_WIDTH = 3;
// E7 E7 E7 on air; hd::onAirToRegister() reverses it, which leaves it unchanged.
static uint8_t ADDR_REG[5] = {0xE7, 0xE7, 0xE7, 0, 0};
static const uint8_t PAYLOAD = 5;

static RF24 rf(PIN_CE, PIN_CSN);
static bool found = false, haveId = false, enabled = true;
static uint8_t remoteId[2];  // private: never printed or served

void begin() {
  pinMode(PIN_IR_TX, OUTPUT);
  digitalWrite(PIN_IR_TX, LOW);

  // HD_Remote_Clone's firmware keeps the remote ID in its own namespace; read
  // it without changing anything. Absent on a board that never ran it.
  Preferences prefs;
  if (prefs.begin("hd", true)) {
    haveId = prefs.isKey("rid") && prefs.getBytes("rid", remoteId, 2) == 2;
    prefs.end();
  }

  found = rf.begin() && rf.isChipConnected();  // as ensureRadio()
  Serial.printf("Radio: %s%s\n", state(),
                live() ? " (remote ID from HD_Remote_Clone's flash)" : ": frames are printed, not sent");
}

bool live() { return found && haveId && enabled; }

const char *state() {
  if (!found) return "no radio";
  if (!haveId) return "no remote ID";
  return enabled ? "live" : "off";
}

void setEnabled(bool on) { enabled = on; }

// prepareWake(), less the ID check (live() covers it) and the wake payload.
void prepare() {
  if (!found) return;
  rf.stopListening();
  rf.setAutoAck(false);
  rf.setRetries(0, 0);
  rf.disableDynamicPayloads();
  rf.setDataRate(RF24_2MBPS);
  rf.setAddressWidth(ADDR_WIDTH);
  rf.setCRCLength(RF24_CRC_16);
  rf.setPayloadSize(PAYLOAD);
  rf.setPALevel(RF24_PA_HIGH);  // txPa at boot
  rf.openWritingPipe(ADDR_REG);
}

// rotateBurst(), with the payload built from the remote ID.
uint32_t sendPhase(uint8_t mode, uint8_t group, uint8_t action, uint32_t ms) {
  if (!live()) return 0;
  const uint8_t p[PAYLOAD] = {remoteId[0], remoteId[1], mode, group, action};
  uint32_t sent = 0, t0 = millis();
  while (millis() - t0 < ms) {
    rf.setChannel(CHANNELS[sent % 3]);
    rf.write(p, PAYLOAD);
    sent++;
  }
  return sent;
}

}  // namespace radio
