// SPDX-License-Identifier: GPL-2.0-or-later
#include "net.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

namespace net {

static Preferences prefs;
static String ssid_, pass_, webpw_;
static const char *host_ = "esp32";
static void (*onUp_)() = nullptr;
static bool upOnce_ = false;
static wl_status_t last_ = WL_IDLE_STATUS;

// isKey() check avoids noisy NOT_FOUND log lines on first boot.
static String load(const char *k) { return prefs.isKey(k) ? prefs.getString(k) : String(); }

static void connect() {
  if (ssid_.isEmpty()) {
    Serial.println("No SSID set. Use: wifi ssid <name>");
    return;
  }
  Serial.printf("Connecting to \"%s\"...\n", ssid_.c_str());
  WiFi.disconnect();
  WiFi.begin(ssid_.c_str(), pass_.c_str());
}

void begin(const char *hostname, void (*onFirstConnect)()) {
  host_ = hostname;
  onUp_ = onFirstConnect;
  WiFi.setHostname(hostname);  // before mode(), or it's ignored
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  prefs.begin("wifi", false);
  ssid_ = load("ssid");
  pass_ = load("pass");
  webpw_ = load("webpw");
  if (ssid_.isEmpty())
    Serial.println("No saved network. Use 'wifi scan', 'wifi ssid <name>', 'wifi pass <pw>', 'wifi connect'.");
  else
    connect();
}

void poll() {
  wl_status_t s = WiFi.status();
  if (s == last_) return;
  bool was = (last_ == WL_CONNECTED);
  last_ = s;
  if (s == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    if (!upOnce_) {
      upOnce_ = true;
      if (MDNS.begin(host_)) MDNS.addService("http", "tcp", 80);
      if (onUp_) onUp_();
    }
    Serial.printf("\r\nWi-Fi connected. Open http://%s/ or http://%s.local/\n> ", ip.c_str(), host_);
  } else if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) {
    Serial.printf("\r\nWi-Fi connection failed (%s). Check 'wifi ssid' / 'wifi pass'.\n> ",
                  s == WL_NO_SSID_AVAIL ? "network not found" : "auth failed");
  } else if (was) {
    Serial.print("\r\nWi-Fi disconnected; retrying.\n> ");
  }
}

void setSsid(const String &s) { ssid_ = s; }
void setPass(const String &p) { pass_ = p; }

void saveAndConnect() {
  prefs.putString("ssid", ssid_);
  prefs.putString("pass", pass_);
  Serial.println("Credentials saved.");
  connect();
}

void forget() {
  prefs.remove("ssid");
  prefs.remove("pass");
  ssid_ = pass_ = "";
  WiFi.disconnect();
  Serial.println("Saved Wi-Fi credentials erased.");
}

void scan() {
  Serial.println("Scanning...");
  int n = WiFi.scanNetworks();
  if (n <= 0) Serial.println("No networks found.");
  for (int i = 0; i < n; i++)
    Serial.printf("%2d  %4d dBm  ch%2d  %s  %s\n", i + 1, WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open  " : "secure", WiFi.SSID(i).c_str());
  WiFi.scanDelete();
}

void printStatus() {
  Serial.printf("Saved SSID : %s\n", ssid_.isEmpty() ? "(none)" : ssid_.c_str());
  Serial.printf("Password   : %s\n", pass_.isEmpty() ? "(none)" : "********");
  Serial.printf("Web login  : %s\n", webpw_.isEmpty() ? "none (page is open to the LAN)" : "admin / ********");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected  : yes, ch %d, RSSI %d dBm\n", WiFi.channel(), WiFi.RSSI());
    Serial.printf("Web page   : http://%s/  or  http://%s.local/\n", WiFi.localIP().toString().c_str(), host_);
  } else {
    Serial.println("Connected  : no");
  }
}

String webPassword() { return webpw_; }

void setWebPassword(const String &pw) {
  webpw_ = pw;
  if (pw.isEmpty()) prefs.remove("webpw");
  else prefs.putString("webpw", pw);
}

}  // namespace net
