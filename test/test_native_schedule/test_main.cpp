// SPDX-License-Identifier: GPL-2.0-or-later
// Host tests (`pio test -e native`): clock-time schedules, lib/bridgecore/schedule.*.
#include <unity.h>

#include "schedule.h"

using namespace bridge;

void setUp() {}
void tearDown() {}

void test_parse_clock() {
  TEST_ASSERT_EQUAL(0, parseClock("00:00"));
  TEST_ASSERT_EQUAL(22 * 60 + 30, parseClock("22:30"));
  TEST_ASSERT_EQUAL(23 * 60 + 59, parseClock("23:59"));
  TEST_ASSERT_EQUAL(7 * 60 + 5, parseClock("7:05"));  // one-digit hour
  TEST_ASSERT_EQUAL(-1, parseClock("24:00"));
  TEST_ASSERT_EQUAL(-1, parseClock("12:60"));
  TEST_ASSERT_EQUAL(-1, parseClock("12:5"));
  TEST_ASSERT_EQUAL(-1, parseClock("12:300"));
  TEST_ASSERT_EQUAL(-1, parseClock("1230"));
  TEST_ASSERT_EQUAL(-1, parseClock(":30"));
  TEST_ASSERT_EQUAL(-1, parseClock("ab:cd"));
  TEST_ASSERT_EQUAL(-1, parseClock(""));
  TEST_ASSERT_EQUAL(-1, parseClock(nullptr));
}

void test_format_clock() {
  char out[6];
  formatClock(0, out);
  TEST_ASSERT_EQUAL_STRING("00:00", out);
  formatClock(22 * 60 + 30, out);
  TEST_ASSERT_EQUAL_STRING("22:30", out);
  formatClock(7 * 60 + 5, out);
  TEST_ASSERT_EQUAL_STRING("07:05", out);
  formatClock(1440, out);
  TEST_ASSERT_EQUAL_STRING("", out);
  for (int m = 0; m < 1440; m++) {  // round trip
    formatClock(m, out);
    TEST_ASSERT_EQUAL(m, parseClock(out));
  }
}

void test_zones() {
  TEST_ASSERT_EQUAL_STRING("PST8PDT,M3.2.0,M11.1.0", posixForZone("America/Los_Angeles"));
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", posixForZone("America/New_York"));
  TEST_ASSERT_EQUAL_STRING("MST7", posixForZone("America/Phoenix"));  // no DST
  TEST_ASSERT_EQUAL_STRING("UTC0", posixForZone("UTC"));
  TEST_ASSERT_NULL(posixForZone("Europe/Atlantis"));
  TEST_ASSERT_NULL(posixForZone("america/los_angeles"));  // names are case-sensitive
  TEST_ASSERT_NULL(posixForZone(nullptr));
}

static const int T = 22 * 60 + 30;  // a 22:30 schedule
static const long DAY1 = 20261127, DAY2 = 20261128;

void test_fires_when_the_clock_enters_its_minute() {
  TEST_ASSERT_TRUE(shouldFire(T - 1, T, T, DAY1, 0));
  TEST_ASSERT_TRUE(shouldFire(T - 1, T, T, DAY2, DAY1));  // next day again
}

void test_not_outside_its_minute() {
  TEST_ASSERT_FALSE(shouldFire(T - 2, T - 1, T, DAY1, 0));
  TEST_ASSERT_FALSE(shouldFire(T, T + 1, T, DAY1, 0));
}

void test_once_while_in_the_minute() {
  // Checked every second: only the first check inside the minute fires.
  TEST_ASSERT_FALSE(shouldFire(T, T, T, DAY1, 0));
}

void test_no_replay_after_boot() {
  // Time unknown at the previous check (just booted, or before NTP): no fire,
  // even inside the minute, and not for a time already passed.
  TEST_ASSERT_FALSE(shouldFire(-1, T, T, DAY1, 0));
  TEST_ASSERT_FALSE(shouldFire(-1, T + 5, T, DAY1, 0));
}

void test_once_a_day_when_clocks_go_back() {
  // 01:30 happens twice on the night the clocks go back: fires the first time.
  const int t = 90;
  TEST_ASSERT_TRUE(shouldFire(t - 1, t, t, DAY1, 0));
  // An hour later the clock reads 01:29 -> 01:30 again, same date: no fire.
  TEST_ASSERT_FALSE(shouldFire(t - 1, t, t, DAY1, DAY1));
}

void test_skipped_when_clocks_go_forward() {
  // 02:30 doesn't exist the night the clocks go forward: 01:59 -> 03:00.
  const int t = 150;
  TEST_ASSERT_FALSE(shouldFire(119, 180, t, DAY1, 0));
}

void test_midnight_schedule() {
  TEST_ASSERT_TRUE(shouldFire(1439, 0, 0, DAY2, DAY1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_clock);
  RUN_TEST(test_format_clock);
  RUN_TEST(test_zones);
  RUN_TEST(test_fires_when_the_clock_enters_its_minute);
  RUN_TEST(test_not_outside_its_minute);
  RUN_TEST(test_once_while_in_the_minute);
  RUN_TEST(test_no_replay_after_boot);
  RUN_TEST(test_once_a_day_when_clocks_go_back);
  RUN_TEST(test_skipped_when_clocks_go_forward);
  RUN_TEST(test_midnight_schedule);
  return UNITY_END();
}
