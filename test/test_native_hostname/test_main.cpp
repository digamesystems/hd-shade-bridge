// SPDX-License-Identifier: GPL-2.0-or-later
// Host tests (`pio test -e native`): hostname checking, lib/bridgecore/hostname.*.
#include <unity.h>

#include "hostname.h"

using namespace bridge;

void setUp() {}
void tearDown() {}

static char out[MAX_HOSTNAME + 1];

static void good(const char *in, const char *expect) {
  TEST_ASSERT_TRUE_MESSAGE(normalizeHostname(in, out), in);
  TEST_ASSERT_EQUAL_STRING(expect, out);
}

static void bad(const char *in) {
  TEST_ASSERT_FALSE_MESSAGE(normalizeHostname(in, out), in);
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_accepts_plain_names() {
  good("shade-bridge", "shade-bridge");
  good("blinds", "blinds");
  good("a", "a");
  good("den2", "den2");
  good("2nd-floor", "2nd-floor");
}

void test_lowercases() {
  good("Living-Room", "living-room");
  good("SHADES", "shades");
}

void test_rejects_empty_and_null() {
  bad("");
  TEST_ASSERT_FALSE(normalizeHostname(nullptr, out));
}

void test_rejects_hyphen_at_either_end() {
  bad("-shades");
  bad("shades-");
  bad("-");
}

void test_rejects_other_characters() {
  bad("living room");      // space
  bad("shades.local");     // dot: the name is one label, .local is added
  bad("shade_bridge");     // underscore
  bad("shades!");
  bad("caf\xc3\xa9");      // non-ASCII
}

void test_length_limit() {
  char ok[MAX_HOSTNAME + 1], tooLong[MAX_HOSTNAME + 2];
  for (size_t i = 0; i < MAX_HOSTNAME; i++) ok[i] = tooLong[i] = 'a';
  ok[MAX_HOSTNAME] = 0;
  tooLong[MAX_HOSTNAME] = 'a';
  tooLong[MAX_HOSTNAME + 1] = 0;
  good(ok, ok);
  bad(tooLong);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_accepts_plain_names);
  RUN_TEST(test_lowercases);
  RUN_TEST(test_rejects_empty_and_null);
  RUN_TEST(test_rejects_hyphen_at_either_end);
  RUN_TEST(test_rejects_other_characters);
  RUN_TEST(test_length_limit);
  return UNITY_END();
}
