// SPDX-License-Identifier: GPL-2.0-or-later
#include "schedules.h"
#include <Preferences.h>
#include <schedule.h>
#include <time.h>
#include "txqueue.h"

namespace sched {

struct Entry {
  char preset[txq::PRESET_NAME_MAX + 1];
  int16_t minute;  // 0..1439
  uint8_t on;
};
// One fixed-size blob, like the presets: a save is one flash write, and an
// unreadable blob (layout changed) is dropped without touching the presets.
struct Store {
  uint8_t n;
  Entry e[txq::MAX_PRESETS];
};
static Store store;
static long lastFired[txq::MAX_PRESETS];  // YYYYMMDD, RAM only: a reboot never replays
static Preferences prefs;
static const char *KEY = "v1";
static String zone_;
static int prevMinute = -1;
static uint32_t lastCheck = 0;

static int find(const String &preset) {
  for (int i = 0; i < store.n; i++)
    if (preset.equalsIgnoreCase(store.e[i].preset)) return i;
  return -1;
}

static bool save() { return prefs.putBytes(KEY, &store, sizeof(store)) == sizeof(store); }

static void startClock() {
  const char *posix = bridge::posixForZone(zone_.c_str());
  if (posix) configTzTime(posix, "pool.ntp.org", "time.google.com");
}

void begin() {
  prefs.begin("sched", false);
  bool ok = prefs.isKey(KEY) && prefs.getBytesLength(KEY) == sizeof(store) &&
            prefs.getBytes(KEY, &store, sizeof(store)) == sizeof(store) && store.n <= txq::MAX_PRESETS;
  if (!ok) memset(&store, 0, sizeof(store));
  for (int i = 0; i < store.n; i++) store.e[i].preset[txq::PRESET_NAME_MAX] = 0;
  zone_ = prefs.isKey("tz") ? prefs.getString("tz") : String();
  if (!bridge::posixForZone(zone_.c_str())) zone_ = "";
  if (zone_.isEmpty()) Serial.println("Schedules: no time zone set (page, or 'tz <zone>').");
  else startClock();
}

String zone() { return zone_; }

bool setZone(const String &iana) {
  if (!bridge::posixForZone(iana.c_str())) return false;
  zone_ = iana;
  prefs.putString("tz", iana);
  startClock();
  prevMinute = -1;  // the clock may jump: don't fire on the change
  txq::log("time zone: %s", iana.c_str());
  return true;
}

// Local time, or false until NTP has set the clock (an unset clock reads 1970).
static bool localNow(struct tm &t) {
  if (zone_.isEmpty()) return false;
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  localtime_r(&now, &t);
  return true;
}

int nowMinute() {
  struct tm t;
  return localNow(t) ? t.tm_hour * 60 + t.tm_min : -1;
}

int minuteFor(const String &preset) {
  int i = find(preset);
  return i < 0 ? -1 : store.e[i].minute;
}

bool isOn(const String &preset) {
  int i = find(preset);
  return i >= 0 && store.e[i].on;
}

bool set(const String &preset, int minute, bool on) {
  if (!txq::presetExists(preset)) return false;
  int i = find(preset);
  if (minute < 0) {
    if (i >= 0) forget(preset);
    return true;
  }
  if (minute > 1439) return false;
  if (i < 0) {
    if (store.n >= txq::MAX_PRESETS) return false;
    i = store.n++;
    lastFired[i] = 0;
  }
  strlcpy(store.e[i].preset, preset.c_str(), sizeof(store.e[i].preset));
  store.e[i].minute = minute;
  store.e[i].on = on ? 1 : 0;
  char hhmm[6];
  bridge::formatClock(minute, hhmm);
  txq::log("schedule: %s at %s, %s", store.e[i].preset, hhmm, on ? "on" : "off");
  return save();
}

void forget(const String &preset) {
  int i = find(preset);
  if (i < 0) return;
  for (int k = i; k + 1 < store.n; k++) {
    store.e[k] = store.e[k + 1];
    lastFired[k] = lastFired[k + 1];
  }
  store.n--;
  memset(&store.e[store.n], 0, sizeof(Entry));
  save();
}

void poll() {
  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();
  struct tm t;
  if (!localNow(t)) {
    prevMinute = -1;
    return;
  }
  int now = t.tm_hour * 60 + t.tm_min;
  long today = (long)(t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
  for (int i = 0; i < store.n; i++) {
    const Entry &e = store.e[i];
    if (!e.on || !bridge::shouldFire(prevMinute, now, e.minute, today, lastFired[i])) continue;
    lastFired[i] = today;
    char hhmm[6];
    bridge::formatClock(e.minute, hhmm);
    txq::Result r = txq::preset(e.preset);
    txq::log("scheduled %s (%s)%s", e.preset, hhmm,
             r == txq::OK ? "" : r == txq::FULL ? ": not sent, queue full" : ": preset not found");
  }
  prevMinute = now;
}

}  // namespace sched
