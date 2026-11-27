// SPDX-License-Identifier: GPL-2.0-or-later
#include "schedule.h"
#include <string.h>

namespace bridge {

int parseClock(const char *s) {
  if (!s) return -1;
  int h = 0, m = 0, i = 0, digits = 0;
  while (s[i] >= '0' && s[i] <= '9' && digits < 2) h = h * 10 + (s[i++] - '0'), digits++;
  if (digits == 0 || s[i] != ':') return -1;
  i++;
  if (!(s[i] >= '0' && s[i] <= '9' && s[i + 1] >= '0' && s[i + 1] <= '9') || s[i + 2] != 0) return -1;
  m = (s[i] - '0') * 10 + (s[i + 1] - '0');
  if (h > 23 || m > 59) return -1;
  return h * 60 + m;
}

void formatClock(int minute, char *out) {
  if (minute < 0 || minute > 1439) {
    out[0] = 0;
    return;
  }
  int h = minute / 60, m = minute % 60;
  out[0] = (char)('0' + h / 10);
  out[1] = (char)('0' + h % 10);
  out[2] = ':';
  out[3] = (char)('0' + m / 10);
  out[4] = (char)('0' + m % 10);
  out[5] = 0;
}

// US and Canadian zones (DST: second Sunday of March to first Sunday of
// November, at 2:00), plus UTC. Add rows as needed.
struct Zone {
  const char *iana, *posix;
};
static const Zone ZONES[] = {
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Detroit", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Indianapolis", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Winnipeg", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Boise", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Edmonton", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Halifax", "AST4ADT,M3.2.0,M11.1.0"},
    {"Pacific/Honolulu", "HST10"},
    {"UTC", "UTC0"},
    {"Etc/UTC", "UTC0"},
};

const char *posixForZone(const char *iana) {
  if (!iana) return nullptr;
  for (const Zone &z : ZONES)
    if (strcmp(z.iana, iana) == 0) return z.posix;
  return nullptr;
}

bool shouldFire(int prevMinute, int nowMinute, int targetMinute, long today, long lastFiredDay) {
  if (prevMinute < 0) return false;         // time just became known: don't replay
  if (nowMinute != targetMinute) return false;
  if (prevMinute == nowMinute) return false;  // already in this minute at the last check
  return lastFiredDay != today;               // once a day
}

}  // namespace bridge
