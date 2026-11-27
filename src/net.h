// SPDX-License-Identifier: GPL-2.0-or-later
// Wi-Fi client setup: credentials in NVS, set from the serial console.
#pragma once
#include <Arduino.h>

namespace net {

// defaultHostname is used until a name is saved with setHostname().
void begin(const char *defaultHostname, void (*onFirstConnect)());
void poll();  // reports connection changes; call from loop()

void setSsid(const String &s);
void setPass(const String &p);
void saveAndConnect();
void forget();
void scan();
void printStatus();

// Hostname: sent to the router over DHCP and announced over mDNS
// (<name>.local). hostname() is the one in use since boot; a saved change
// takes effect at the next boot, the only time it reaches the router.
String hostname();
String savedHostname();
// Checks and saves a new name (bridge::normalizeHostname); empty = back to
// the default. Returns false, saving nothing, if the name isn't valid.
bool setHostname(const String &name);

// Web page password (HTTP Basic auth, user "admin"); empty = no password.
String webPassword();
void setWebPassword(const String &pw);

}  // namespace net
