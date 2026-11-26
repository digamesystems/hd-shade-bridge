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
// Merged with waiting positions where it can be: groups going to the same
// position share a frame, and a newer target replaces an older one.
Result position(uint8_t mask, int pos);
// Run a named preset (name matched ignoring case). Its steps are queued as
// positions, so they merge too.
Result preset(const String &name);
// Drop jobs that haven't started. The running job finishes (keeps its release tail).
int clearPending();
void poll();

// Status for the web page and console.
bool busy();
int pending();
String pendingLabel(int i);  // i-th waiting job, 0 = next to send
String currentLabel();
uint32_t currentTotalMs();
uint32_t currentLeftMs();
uint32_t remainingMs();
int commanded(int g);  // last commanded position for group 1..4, -1 if unknown

// Presets, kept in flash. The first boot seeds bedtime, waketime and sunset
// (HD_Remote_Clone's); after that they're ordinary presets, and can be deleted.
static const int MAX_PRESETS = 12;
static const int PRESET_NAME_MAX = 24;
void beginPresets();  // load from flash; call once from setup()
int presetCount();
const char *presetName(int i);
int presetSteps(int i);
uint8_t presetMask(int i, int k);
uint8_t presetPos(int i, int k);
String presetSummary(int i);  // e.g. "G4 255, G2 255"
// Save the last-sent positions of `groups` (bits 01..08) as preset `name`.
// Groups at the same position share one step (one frame on air). Refused
// while the queue is busy, so the saved positions are the ones sent. Returns
// "" on success, else a message for the user.
String savePreset(const String &name, uint8_t groups, bool replace);
bool presetExists(const String &name);
bool deletePreset(const String &name);

// Activity log (oldest first), also echoed to Serial.
void log(const char *fmt, ...);
int logCount();
const String &logEntry(int i);

}  // namespace txq
