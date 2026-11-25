// Wi-Fi client setup: credentials in NVS, set from the serial console.
#pragma once
#include <Arduino.h>

namespace net {

void begin(const char *hostname, void (*onFirstConnect)());
void poll();  // reports connection changes; call from loop()

void setSsid(const String &s);
void setPass(const String &p);
void saveAndConnect();
void forget();
void scan();
void printStatus();

// Web page password (HTTP Basic auth, user "admin"); empty = no password.
String webPassword();
void setWebPassword(const String &pw);

}  // namespace net
