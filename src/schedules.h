// SPDX-License-Identifier: GPL-2.0-or-later
// (Not "sched.h": that name is the system's POSIX header.) Clock-time schedules for presets (John, 2026-11-27): any preset can run at
// one time a day, on or off. Kept in flash beside the presets, keyed by the
// preset's name, so the preset store's layout doesn't change. The time comes
// from NTP in the time zone set from the page (or `tz` on serial). The rules
// for when a schedule fires are bridge::shouldFire (lib/bridgecore).
#pragma once
#include <Arduino.h>

namespace sched {

void begin();  // load from flash, start NTP if a time zone is set; after net::begin()
void poll();   // checks the clock about once a second; call from loop()

// Time zone, as an IANA name ("America/Los_Angeles"); "" = not set.
String zone();
// Sets and saves it; false if the bridge doesn't know the name.
bool setZone(const String &iana);

// Local time now, minute of the day, or -1 until the time is known.
int nowMinute();

// A preset's schedule: minute of the day (-1 = none) and whether it's on.
int minuteFor(const String &preset);
bool isOn(const String &preset);
// Sets a preset's daily time (minute 0..1439) and on/off; false if the preset
// doesn't exist or there's no room. minute -1 clears it.
bool set(const String &preset, int minute, bool on);
// Removes a preset's schedule (when the preset is deleted).
void forget(const String &preset);

}  // namespace sched
