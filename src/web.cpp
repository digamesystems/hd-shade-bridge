// SPDX-License-Identifier: GPL-2.0-or-later
#include "web.h"
#include <WebServer.h>
#include <WiFi.h>
#include "net.h"
#include "page.h"
#include "txqueue.h"

namespace web {

static WebServer server(80);
static bool started = false;

// Which way each group's fabric hangs, for this house. Position values are
// the moving rail's height (0 = bottom, 255 = top), so for a top-down shade
// 255 is open, and for a bottom-up shade 0 is open.
static const char *GROUP_TYPE[4] = {"top-down", "bottom-up", "top-down", "bottom-up"};

static String esc(const String &s) {
  String o;
  o.reserve(s.length() + 2);
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
    else o += c;
  }
  return o;
}

// HTTP Basic auth when a web password is set ('web pass <pw>' on serial).
static bool authed() {
  String pw = net::webPassword();
  if (pw.isEmpty() || server.authenticate("admin", pw.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "Shade bridge");
  return false;
}

static void reply(txq::Result r) {
  if (r == txq::OK)
    server.send(202, "application/json", "{\"ok\":true,\"queued\":" + String(txq::pending()) + "}");
  else if (r == txq::FULL)
    server.send(429, "application/json", "{\"ok\":false,\"error\":\"Queue full, try again shortly\"}");
  else
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Bad request\"}");
}

static int parseGroup(const String &g) {  // 1..4, 0 = all, -1 = bad
  if (g.equalsIgnoreCase("all")) return 0;
  int v = g.toInt();
  return (v >= 1 && v <= 4 && g.length() == 1) ? v : -1;
}

static void handleStatus() {
  if (!authed()) return;
  String j = "{\"dry_run\":true,\"uptime\":" + String(millis() / 1000);
  j += ",\"wifi\":{\"ssid\":\"" + esc(WiFi.SSID()) + "\",\"rssi\":" + String(WiFi.RSSI()) +
       ",\"ch\":" + String(WiFi.channel()) + ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  j += ",\"busy\":" + String(txq::busy() ? "true" : "false");
  j += ",\"current\":\"" + esc(txq::currentLabel()) + "\"";
  j += ",\"cur_total_ms\":" + String(txq::currentTotalMs());
  j += ",\"cur_left_ms\":" + String(txq::currentLeftMs());
  j += ",\"queued\":" + String(txq::pending());
  j += ",\"remaining_ms\":" + String(txq::remainingMs());
  j += ",\"pos\":[";
  for (int g = 1; g <= 4; g++) j += (g > 1 ? "," : "") + String(txq::commanded(g));
  j += "],\"log\":[";
  for (int i = 0; i < txq::logCount(); i++) j += (i ? ",\"" : "\"") + esc(txq::logEntry(i)) + "\"";
  j += "]}";
  server.send(200, "application/json", j);
}

static void fail(int code, const String &msg) {
  server.send(code, "application/json", "{\"ok\":false,\"error\":\"" + esc(msg) + "\"}");
}

// [{"name":"Bedtime","steps":[[mask,pos],...]}, ...]; the page shows positions as %.
static void handlePresets() {
  if (!authed()) return;
  String j = "[";
  for (int i = 0; i < txq::presetCount(); i++) {
    j += String(i ? "," : "") + "{\"name\":\"" + esc(txq::presetName(i)) + "\",\"steps\":[";
    for (int k = 0; k < txq::presetSteps(i); k++)
      j += String(k ? "," : "") + "[" + String(txq::presetMask(i, k)) + "," + String(txq::presetPos(i, k)) + "]";
    j += "]}";
  }
  server.send(200, "application/json", j + "]");
}

static void handlePresetSave() {
  if (!authed()) return;
  String g = server.arg("groups");  // group bits, decimal 1..15
  bool digits = g.length() >= 1 && g.length() <= 2;
  for (char c : g) digits = digits && isDigit(c);
  int mask = digits ? g.toInt() : 0;
  if (mask < 1 || mask > 15) return fail(400, "Choose at least one group");
  String err = txq::savePreset(server.arg("name"), mask, server.arg("replace") == "1");
  if (!err.isEmpty()) return fail(400, err);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handlePresetDelete() {
  if (!authed()) return;
  if (!txq::deletePreset(server.arg("name"))) return fail(404, "No such preset, or the flash write failed");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleGrp() {
  if (!authed()) return;
  int g = parseGroup(server.arg("group"));
  String d = server.arg("dir");
  if (g < 0 || (d != "up" && d != "down")) return reply(txq::BAD);
  reply(txq::group(g, d == "up"));
}

static void handlePos() {
  if (!authed()) return;
  int g = parseGroup(server.arg("group"));
  String p = server.arg("pos");
  int v = p.toInt();
  if (g < 1 || p.isEmpty() || v < 0 || v > 255 || (v == 0 && p != "0")) return reply(txq::BAD);
  reply(txq::position(1 << (g - 1), v));
}

void begin() {
  if (started) return;
  server.on("/", HTTP_GET, [] {
    if (!authed()) return;
    server.send_P(200, "text/html", PAGE_HTML);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/presets", HTTP_GET, handlePresets);
  server.on("/api/groups", HTTP_GET, [] {
    if (!authed()) return;
    String j = "[";
    for (int g = 1; g <= 4; g++)
      j += String(g > 1 ? "," : "") + "{\"group\":" + String(g) + ",\"type\":\"" + GROUP_TYPE[g - 1] + "\"}";
    server.send(200, "application/json", j + "]");
  });
  server.on("/api/grp", HTTP_POST, handleGrp);
  server.on("/api/pos", HTTP_POST, handlePos);
  server.on("/api/preset", HTTP_POST, [] {
    if (!authed()) return;
    reply(txq::preset(server.arg("name")));
  });
  server.on("/api/preset/save", HTTP_POST, handlePresetSave);
  server.on("/api/preset/delete", HTTP_POST, handlePresetDelete);
  server.on("/api/clear", HTTP_POST, [] {
    if (!authed()) return;
    int n = txq::clearPending();
    server.send(200, "application/json", "{\"ok\":true,\"cleared\":" + String(n) + "}");
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found\n"); });
  server.begin();
  started = true;
}

void poll() {
  if (started) server.handleClient();
}

}  // namespace web
