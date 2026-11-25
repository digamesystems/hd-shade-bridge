// Shade bridge prototype (dry run): Wi-Fi setup over serial, a web page for
// group commands, positions and presets, and a non-blocking send queue.
// Frames are printed instead of transmitted until an nRF24 is attached.
//
// Serial console, 115200 baud. Type 'help'.

#include <Arduino.h>
#include "net.h"
#include "txqueue.h"
#include "web.h"

static const char *HOSTNAME = "shade-bridge";
static const char *FW_VERSION = "0.2.0-dryrun";

static String lineBuf;

static void printHelp() {
  Serial.printf(
      "Shade bridge %s (dry run: frames are printed, not sent)\n"
      "Wi-Fi:\n"
      "  wifi scan             list nearby networks\n"
      "  wifi ssid <name>      set network name\n"
      "  wifi pass [password]  set password (blank = open network)\n"
      "  wifi connect          save and connect\n"
      "  wifi status           connection info and web address\n"
      "  wifi forget           erase saved network\n"
      "  web pass [password]   web page login (user admin); blank = no login\n"
      "Shades (same queue as the web page):\n"
      "  grp <1-4|all> <up|down>\n"
      "  pos <mask> <0-255>    mask in hex, group bits 01-08, e.g. 05 = groups 1+3\n"
      "  bedtime | waketime | sunset\n"
      "  queue                 what's sending and queued\n"
      "  clear                 drop queued jobs\n"
      "Other: help, reboot\n",
      FW_VERSION);
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
  } else if (cmd == "bedtime" || cmd == "waketime" || cmd == "sunset") {
    report(txq::preset(cmd));
  } else if (cmd == "queue") {
    if (!txq::busy()) Serial.println("Idle.");
    else Serial.printf("Sending: %s (%lu ms left); %d queued; %lu ms in all\n", txq::currentLabel().c_str(),
                       (unsigned long)txq::currentLeftMs(), txq::pending(), (unsigned long)txq::remainingMs());
  } else if (cmd == "clear") {
    Serial.printf("Dropped %d queued job(s).\n", txq::clearPending());
  } else if (cmd == "reboot") {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
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
  net::begin(HOSTNAME, web::begin);
  Serial.print("> ");
}

void loop() {
  pollSerial();
  net::poll();
  txq::poll();
  web::poll();
}
