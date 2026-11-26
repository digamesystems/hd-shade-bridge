// SPDX-License-Identifier: GPL-2.0-or-later
// nRF24 transmitter for Platinum frames (ID ID MODE GROUP ACTION).
//
// RF behaviour is HD_Remote_Clone's, unchanged (CLAUDE.md, "RF protocol"):
// air settings from src/rf_tools.cpp prepareWake(), sending from
// rotateBurst(), pins from include/pins.h. Frames go out only with an nRF24
// present and a remote ID stored by HD_Remote_Clone's firmware on this board;
// otherwise the bridge is a dry run.
#pragma once
#include <Arduino.h>

namespace radio {

// Probe the nRF24 and load the remote ID. Call once from setup().
void begin();
// True when frames are transmitted; false = dry run (frames printed only).
bool live();
// "live", "off" (turned off on serial), "no radio" or "no remote ID".
const char *state();
// Transmit on/off (RAM only; on at boot when the radio and ID are there).
void setEnabled(bool on);
// Apply the Platinum air settings, as prepareWake() does before every command.
void prepare();
// Send one frame back to back for `ms`, ch 33/52/71 in rotation starting at
// 33, as rotateBurst() does. Blocks for `ms`. Returns the packets sent.
uint32_t sendPhase(uint8_t mode, uint8_t group, uint8_t action, uint32_t ms);

}  // namespace radio
