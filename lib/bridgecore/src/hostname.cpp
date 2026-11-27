// SPDX-License-Identifier: GPL-2.0-or-later
#include "hostname.h"

namespace bridge {

bool normalizeHostname(const char *in, char *out) {
  out[0] = 0;
  if (!in) return false;
  size_t n = 0;
  for (; in[n]; n++) {
    if (n >= MAX_HOSTNAME) {
      out[0] = 0;
      return false;
    }
    char c = in[n];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    if (!ok) {
      out[0] = 0;
      return false;
    }
    out[n] = c;
  }
  out[n] = 0;
  if (n == 0 || out[0] == '-' || out[n - 1] == '-') {
    out[0] = 0;
    return false;
  }
  return true;
}

}  // namespace bridge
