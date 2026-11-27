// SPDX-License-Identifier: GPL-2.0-or-later
// Checking a hostname set over serial (`name <hostname>`). The name goes to
// the router over DHCP and onto the network over mDNS (<name>.local), so it
// follows the usual rule for a single DNS label, kept short: letters, digits
// and hyphens, not starting or ending with a hyphen, at most MAX_HOSTNAME
// characters. Letters are lowercased.
#pragma once
#include <stddef.h>

namespace bridge {

static const size_t MAX_HOSTNAME = 32;

// Writes the lowercased name to out (at least MAX_HOSTNAME + 1 bytes) and
// returns true if `in` is a valid hostname; returns false (out untouched
// beyond its first byte) if not.
bool normalizeHostname(const char *in, char *out);

}  // namespace bridge
