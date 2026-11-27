// SPDX-License-Identifier: GPL-2.0-or-later
// Shade bridge prototype: Wi-Fi setup over serial, a web page for positions
// and presets, and a non-blocking send queue. Frames go out on an nRF24 when
// one is fitted and HD_Remote_Clone has stored a remote ID on the board;
// otherwise they're printed instead (dry run).
//
// Serial console, 115200 baud. Type 'help'.

#include <Arduino.h>
#include "net.h"
#include "radio.h"
#include "txqueue.h"
#include "version.h"
#include "web.h"

static const char *HOSTNAME = "shade-bridge";

static String lineBuf;

static void printHelp() {
  Serial.printf(
      "Shade bridge %s (radio: %s)\n"
      "Wi-Fi:\n"
      "  wifi scan             list nearby networks\n"
      "  wifi ssid <name>      set network name\n"
      "  wifi pass [password]  set password (blank = open network)\n"
      "  wifi connect          save and connect\n"
      "  wifi status           connection info and web address\n"
      "  wifi forget           erase saved network\n"
      "  web pass [password]   web page login (user admin); blank = no login\n"
      "  name [hostname]       show or set the network name (default shade-bridge); after 'reboot'\n"
      "Shades (same queue as the web page):\n"
      "  grp <1-4|all> <up|down>\n"
      "  pos <mask> <0-255>    mask in hex, group bits 01-08, e.g. 05 = groups 1+3\n"
      "  presets               list presets (saved and deleted on the web page)\n"
      "  preset <name>         run a preset; a name alone works too, e.g. bedtime\n"
      "  queue                 what's sending and queued\n"
      "  clear                 drop queued jobs\n"
      "Radio:\n"
      "  radio [on|off]        show the radio, or turn transmitting on/off (off = dry run)\n"
      "Other: help, reboot\n",
      FW_VERSION, radio::state());
}

static void report(txq::Result r) {
  if (r == txq::FULL) Serial.println("Queue full; try again shortly.");
  else if (r == txq::BAD) Serial.println("Bad arguments. Type 'help'.");
}

static String nextWord(String &rest) {
  rest.trim();
  int sp = rest.indexOf(' ');
  String w = sp < 0 ? rest : rest.substring(0, sp);
  rest = sp < 0 ? String() : rest.substring(sp + 1);
  return w;
}

static void wifiCommand(String rest) {
  String sub = nextWord(rest);
  sub.toLowerCase();
  if (sub == "scan") net::scan();
  else if (sub == "ssid") { net::setSsid(rest); Serial.printf("SSID set to \"%s\". Now 'wifi pass', then 'wifi connect'.\n", rest.c_str()); }
  else if (sub == "pass") { net::setPass(rest); Serial.println(rest.isEmpty() ? "Password cleared (open network)." : "Password set."); }
  else if (sub == "connect") net::saveAndConnect();
  else if (sub == "status" || sub.isEmpty()) net::printStatus();
  else if (sub == "forget") net::forget();
  else Serial.println("Unknown wifi command. Type 'help'.");
}

static void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;
  String rest = line;
  String cmd = nextWord(rest);
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") printHelp();
  else if (cmd == "wifi") wifiCommand(rest);
  else if (cmd == "web") {
    String sub = nextWord(rest);
    if (!sub.equalsIgnoreCase("pass")) { Serial.println("Usage: web pass [password]"); return; }
    net::setWebPassword(rest);
    Serial.println(rest.isEmpty() ? "Web login removed: the page is open to anyone on the LAN."
                                  : "Web login set (user: admin).");
  } else if (cmd == "grp") {
    String g = nextWord(rest), d = nextWord(rest);
    d.toLowerCase();
    int gi = g.equalsIgnoreCase("all") ? 0 : g.toInt();
    if ((gi == 0 && !g.equalsIgnoreCase("all")) || (d != "up" && d != "down")) return report(txq::BAD);
    report(txq::group(gi, d == "up"));
  } else if (cmd == "pos") {
    String m = nextWord(rest), p = nextWord(rest);
    long mask = strtol(m.c_str(), nullptr, 16);
    if (m.isEmpty() || p.isEmpty() || (p.toInt() == 0 && p != "0")) return report(txq::BAD);
    report(txq::position((uint8_t)mask, p.toInt()));
  } else if (cmd == "presets") {
    if (!txq::presetCount()) Serial.println("No presets. Save one from the web page.");
    for (int i = 0; i < txq::presetCount(); i++)
      Serial.printf("  %-24s %s\n", txq::presetName(i), txq::presetSummary(i).c_str());
  } else if (cmd == "preset") {
    rest.trim();
    if (!txq::presetExists(rest)) Serial.println("No such preset. Type 'presets' to list them.");
    else report(txq::preset(rest));
  } else if (cmd == "queue") {
    if (!txq::busy()) Serial.println("Idle.");
    else {
      Serial.printf("Sending: %s (%lu ms left); %d queued; %lu ms in all\n", txq::currentLabel().c_str(),
                    (unsigned long)txq::currentLeftMs(), txq::pending(), (unsigned long)txq::remainingMs());
      for (int i = 0; i < txq::pending(); i++) Serial.printf("  %d. %s\n", i + 1, txq::pendingLabel(i).c_str());
    }
  } else if (cmd == "clear") {
    Serial.printf("Dropped %d queued job(s).\n", txq::clearPending());
  } else if (cmd == "name") {
    rest.trim();
    if (rest.isEmpty()) {
      Serial.printf("Hostname: %s", net::hostname().c_str());
      if (net::savedHostname() != net::hostname())
        Serial.printf(" (saved: %s, from the next 'reboot')", net::savedHostname().c_str());
      Serial.println();
    } else if (!net::setHostname(rest.equalsIgnoreCase("default") ? String() : rest)) {
      Serial.println("Not a valid name: letters, digits and '-', not first or last, up to 32 characters.");
    } else {
      Serial.printf("Hostname saved: %s. It takes effect after 'reboot'.\n", net::savedHostname().c_str());
    }
  } else if (cmd == "radio") {
    String sub = nextWord(rest);
    sub.toLowerCase();
    if (sub == "on" || sub == "off") radio::setEnabled(sub == "on");
    else if (!sub.isEmpty()) return report(txq::BAD);
    Serial.printf("Radio: %s\n", radio::state());
  } else if (cmd == "reboot") {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  } else if (txq::presetExists(line)) {
    report(txq::preset(line));  // a preset's name on its own
  } else {
    Serial.printf("Unknown command '%s'. Type 'help'.\n", cmd.c_str());
  }
}

// Line editor with echo and backspace, for a raw serial terminal.
static void pollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (c == '\n' && lineBuf.isEmpty()) continue;  // LF after CR
      Serial.println();
      handleCommand(lineBuf);
      lineBuf = "";
      Serial.print("> ");
    } else if (c == '\b' || c == 0x7f) {
      if (!lineBuf.isEmpty()) {
        lineBuf.remove(lineBuf.length() - 1);
        Serial.print("\b \b");
      }
    } else if (isPrintable(c) && lineBuf.length() < 128) {
      lineBuf += c;
      String lower = lineBuf;
      lower.toLowerCase();
      bool secret = (lower.startsWith("wifi pass ") && lineBuf.length() > 10) ||
                    (lower.startsWith("web pass ") && lineBuf.length() > 9);
      Serial.print(secret ? '*' : c);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n\nShade bridge %s  -  type 'help'\n", FW_VERSION);
  txq::beginPresets();
  radio::begin();
  net::begin(HOSTNAME, web::begin);
  Serial.print("> ");
}

void loop() {
  pollSerial();
  net::poll();
  txq::poll();
  web::poll();
}
