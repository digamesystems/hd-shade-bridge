// SPDX-License-Identifier: GPL-2.0-or-later
// Send queue for shade commands (dry run: frames are printed, not transmitted).
//
// Web and serial requests enqueue jobs and return at once; poll() steps
// through them from loop() so nothing blocks the web server or console.
// A job is a short list of steps, each repeating one 5-byte Platinum
// payload (ID ID MODE GROUP ACTION) for a set time on ch 33/52/71.
#pragma once
#include <Arduino.h>

namespace txq {

enum Result { OK, FULL, BAD };

// Group command, as `grp` in HD_Remote_Clone: g = 1..4, or 0 for All.
Result group(int g, bool up);
// Absolute position, as `pos`: mask = group bits 01..08 (combinable), pos 0..255.
Result position(uint8_t mask, int pos);
// Named preset (bedtime, waketime, sunset).
Result preset(const String &name);
// Drop jobs that haven't started. The running job finishes (keeps its release tail).
int clearPending();
void poll();

// Status for the web page and console.
bool busy();
int pending();
String currentLabel();
uint32_t currentTotalMs();
uint32_t currentLeftMs();
uint32_t remainingMs();
int commanded(int g);  // last commanded position for group 1..4, -1 if unknown

// Presets, for listing.
int presetCount();
const char *presetName(int i);
String presetSummary(int i);

// Activity log (oldest first), also echoed to Serial.
void log(const char *fmt, ...);
int logCount();
const String &logEntry(int i);

}  // namespace txq
