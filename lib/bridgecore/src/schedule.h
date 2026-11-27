// SPDX-License-Identifier: GPL-2.0-or-later
// Clock-time schedules for presets (docs/FEATURE_EXPLORATION.md, "Schedules
// and a sun trigger"; John, 2026-11-27: every preset can be scheduled, clock
// time first, one time a day). The parts that decide things, host-tested;
// src/sched.cpp keeps the schedules and runs them.
#pragma once
#include <stdint.h>

namespace bridge {

// "HH:MM", 24-hour, 00:00..23:59 (one-digit hour allowed) -> minute of the
// day 0..1439; -1 if it isn't one.
int parseClock(const char *s);

// Minute of the day -> "HH:MM" into out (6 bytes). Out of range -> "".
void formatClock(int minute, char *out);

// POSIX TZ string (daylight-saving rules included) for an IANA time-zone
// name the bridge knows, e.g. "America/Los_Angeles" -> "PST8PDT,M3.2.0,
// M11.1.0"; nullptr for an unknown name. The phone's browser supplies the
// name; the ESP32 needs the POSIX form.
const char *posixForZone(const char *iana);

// Whether a schedule at targetMinute fires now. It fires when the clock
// moves into its minute (prevMinute is the minute seen at the last check, -1
// if the time wasn't known then, as after a boot), and at most once on each
// day (dates as YYYYMMDD; lastFiredDay 0 = never). So nothing fires in the
// first minute after a boot (no replay of a missed time), a time repeated when
// the clocks go back fires once, and a time skipped when they go forward
// doesn't fire that day.
bool shouldFire(int prevMinute, int nowMinute, int targetMinute, long today, long lastFiredDay);

}  // namespace bridge
