// SPDX-License-Identifier: GPL-2.0-or-later
#include "txqueue.h"
#include <Preferences.h>
#include <stdarg.h>

namespace txq {

// Timings from HD_Remote_Clone docs/FIRMWARE.md.
static const uint16_t GRP_MS = 300;       // A1/A2 while "held"
static const uint16_t GRP_TAIL_MS = 330;  // 21/22 release tail
static const uint16_t POS50_MS = 1490;    // ID 50 mask pos
static const uint16_t POS32_MS = 300;     // ID 32 mask pos

struct Step {
  uint8_t mode, group, action;
  uint16_t ms;
};
static const int MAX_STEPS = 8;
static const int MAX_JOBS = 8;
struct Job {
  char label[40];
  Step steps[MAX_STEPS];
  uint8_t n;
};

struct PosStep {
  uint8_t mask, pos;
};
// One step per distinct position, so at most one per group.
static const int MAX_PRESET_STEPS = 4;
struct Preset {
  char name[PRESET_NAME_MAX + 1];
  uint8_t n;
  PosStep steps[MAX_PRESET_STEPS];
};
// Seeded on first boot: the same values as the HD firmware's bedtime /
// waketime / sunset commands.
static const Preset DEFAULT_PRESETS[] = {
    {"Bedtime", 2, {{0x08, 255}, {0x02, 255}}},
    {"Waketime", 3, {{0x08, 128}, {0x02, 40}, {0x05, 255}}},
    {"Sunset", 2, {{0x01, 100}, {0x04, 50}}},
};

// Stored as one fixed-size blob, so a save is a single flash write. Changing
// this layout needs a new key; an unreadable blob is replaced by the defaults.
struct PresetStore {
  uint8_t n;
  Preset p[MAX_PRESETS];
};
static PresetStore store;
static Preferences presetPrefs;
static const char *PRESET_KEY = "v1";

static Job jobs[MAX_JOBS];
static int head = 0, count = 0;
static int stepIdx = -1;  // -1: head job not started
static uint32_t stepStart = 0;
static int commandedPos[4] = {-1, -1, -1, -1};

static const int LOG_N = 16;
static String logBuf[LOG_N];
static int logHead = 0, logLen = 0;

void log(const char *fmt, ...) {
  char msg[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  uint32_t s = millis() / 1000;
  char line[112];
  snprintf(line, sizeof(line), "%02lu:%02lu:%02lu %s", (unsigned long)(s / 3600),
           (unsigned long)(s / 60 % 60), (unsigned long)(s % 60), msg);
  logBuf[(logHead + logLen) % LOG_N] = line;
  if (logLen < LOG_N) logLen++;
  else logHead = (logHead + 1) % LOG_N;
  Serial.printf("\r[bridge] %s\n", msg);
}

int logCount() { return logLen; }
const String &logEntry(int i) { return logBuf[(logHead + i) % LOG_N]; }

static String maskText(uint8_t mask) {
  if (mask == 0xFF || mask == 0x0F) return "All";  // group frames' All / all four position bits
  String s;
  for (int g = 1; g <= 8; g++)
    if (mask & (1 << (g - 1))) s += (s.isEmpty() ? "G" : "+G") + String(g);
  return s;
}

static Job *newJob() {
  if (count >= MAX_JOBS) return nullptr;
  Job *j = &jobs[(head + count) % MAX_JOBS];
  j->n = 0;
  return j;
}
static void add(Job *j, uint8_t mode, uint8_t grp, uint8_t action, uint16_t ms) {
  if (j->n < MAX_STEPS) j->steps[j->n++] = {mode, grp, action, ms};
}
static void commit() { count++; }

Result group(int g, bool up) {
  if (g < 0 || g > 4) return BAD;
  Job *j = newJob();
  if (!j) return FULL;
  uint8_t gb = g == 0 ? 0xFF : (1 << (g - 1));
  snprintf(j->label, sizeof(j->label), "%s %s", g == 0 ? "All" : ("Group " + String(g)).c_str(),
           up ? "up" : "down");
  add(j, 0x20, gb, up ? 0xA1 : 0xA2, GRP_MS);
  add(j, 0x20, gb, up ? 0x21 : 0x22, GRP_TAIL_MS);
  commit();
  log("queued: %s", j->label);
  return OK;
}

static bool addPos(Job *j, uint8_t mask, int pos) {
  if (mask == 0 || mask > 0x0F || pos < 0 || pos > 255) return false;
  add(j, 0x50, mask, pos, POS50_MS);
  add(j, 0x32, mask, pos, POS32_MS);
  return true;
}

Result position(uint8_t mask, int pos) {
  if (mask == 0 || mask > 0x0F || pos < 0 || pos > 255) return BAD;
  Job *j = newJob();
  if (!j) return FULL;
  snprintf(j->label, sizeof(j->label), "%s to %d", maskText(mask).c_str(), pos);
  addPos(j, mask, pos);
  commit();
  log("queued: %s", j->label);
  return OK;
}

static int findPreset(const String &name) {
  for (int i = 0; i < store.n; i++)
    if (name.equalsIgnoreCase(store.p[i].name)) return i;
  return -1;
}

Result preset(const String &name) {
  int i = findPreset(name);
  if (i < 0) return BAD;
  const Preset &p = store.p[i];
  Job *j = newJob();
  if (!j) return FULL;
  snprintf(j->label, sizeof(j->label), "Preset %s", p.name);
  for (int k = 0; k < p.n; k++) addPos(j, p.steps[k].mask, p.steps[k].pos);
  commit();
  log("queued: %s", j->label);
  return OK;
}

int clearPending() {
  int keep = (stepIdx >= 0 && count > 0) ? 1 : 0;
  int dropped = count - keep;
  count = keep;
  if (dropped) log("cleared %d queued job%s", dropped, dropped == 1 ? "" : "s");
  return dropped;
}

static void startStep(const Step &s) {
  stepStart = millis();
  // Track what has actually been sent (not just queued), so clearing the
  // queue never leaves the page showing positions that were never sent.
  for (int i = 0; i < 4; i++) {
    if (!(s.group & (1 << i))) continue;
    if (s.mode == 0x50) commandedPos[i] = s.action;    // position frame
    else if (s.mode == 0x20) commandedPos[i] = -1;     // arrow: end point unknown
  }
  // Dry run: show the frame the nRF24 would repeat on ch 33/52/71.
  Serial.printf("\r[dry-run] ID ID %02X %02X %02X  for %u ms on ch 33/52/71\n", s.mode, s.group,
                s.action, s.ms);
}

void poll() {
  if (count == 0) return;
  Job &j = jobs[head];
  if (stepIdx < 0) {
    stepIdx = 0;
    log("sending: %s", j.label);
    startStep(j.steps[0]);
    return;
  }
  if (millis() - stepStart < j.steps[stepIdx].ms) return;
  if (++stepIdx < j.n) {
    startStep(j.steps[stepIdx]);
    return;
  }
  log("done: %s", j.label);
  head = (head + 1) % MAX_JOBS;
  count--;
  stepIdx = -1;
}

bool busy() { return count > 0; }
int pending() { return count - ((stepIdx >= 0 && count > 0) ? 1 : 0); }
String currentLabel() { return (count > 0 && stepIdx >= 0) ? String(jobs[head].label) : String(); }

static uint32_t jobMs(const Job &j) {
  uint32_t t = 0;
  for (int i = 0; i < j.n; i++) t += j.steps[i].ms;
  return t;
}
uint32_t currentTotalMs() { return (count > 0 && stepIdx >= 0) ? jobMs(jobs[head]) : 0; }
uint32_t currentLeftMs() {
  if (count == 0 || stepIdx < 0) return 0;
  const Job &j = jobs[head];
  uint32_t el = millis() - stepStart;
  uint32_t t = j.steps[stepIdx].ms > el ? j.steps[stepIdx].ms - el : 0;
  for (int i = stepIdx + 1; i < j.n; i++) t += j.steps[i].ms;
  return t;
}
uint32_t remainingMs() {
  uint32_t t = currentLeftMs();
  for (int k = (stepIdx >= 0 ? 1 : 0); k < count; k++) t += jobMs(jobs[(head + k) % MAX_JOBS]);
  return t;
}
int commanded(int g) { return (g >= 1 && g <= 4) ? commandedPos[g - 1] : -1; }

static bool storePresets() {
  return presetPrefs.putBytes(PRESET_KEY, &store, sizeof(store)) == sizeof(store);
}

static bool validStore() {
  if (store.n > MAX_PRESETS) return false;
  for (int i = 0; i < store.n; i++) {
    const Preset &p = store.p[i];
    if (p.n == 0 || p.n > MAX_PRESET_STEPS || !memchr(p.name, 0, sizeof(p.name))) return false;
    for (int k = 0; k < p.n; k++)
      if (p.steps[k].mask == 0 || p.steps[k].mask > 0x0F) return false;
  }
  return true;
}

void beginPresets() {
  presetPrefs.begin("presets", false);
  // isKey() first: avoids a NOT_FOUND log line on first boot.
  bool loaded = presetPrefs.isKey(PRESET_KEY) && presetPrefs.getBytesLength(PRESET_KEY) == sizeof(store) &&
                presetPrefs.getBytes(PRESET_KEY, &store, sizeof(store)) == sizeof(store) && validStore();
  if (loaded) return;
  memset(&store, 0, sizeof(store));
  for (const Preset &p : DEFAULT_PRESETS) store.p[store.n++] = p;
  if (!storePresets()) log("couldn't save the default presets to flash");
}

int presetCount() { return store.n; }
const char *presetName(int i) { return store.p[i].name; }
int presetSteps(int i) { return store.p[i].n; }
uint8_t presetMask(int i, int k) { return store.p[i].steps[k].mask; }
uint8_t presetPos(int i, int k) { return store.p[i].steps[k].pos; }
String presetSummary(int i) {
  String s;
  for (int k = 0; k < store.p[i].n; k++) {
    if (k) s += ", ";
    s += maskText(store.p[i].steps[k].mask) + " " + String(store.p[i].steps[k].pos);
  }
  return s;
}

bool presetExists(const String &name) {
  String n = name;
  n.trim();
  return findPreset(n) >= 0;
}

String savePreset(const String &rawName, uint8_t groups, bool replace) {
  String name = rawName;
  name.trim();
  if (name.isEmpty() || name.length() > PRESET_NAME_MAX)
    return "Name must be 1 to " + String(PRESET_NAME_MAX) + " characters";
  for (char c : name)
    if (c < 0x20 || c > 0x7E) return "Name: letters, digits, spaces and punctuation only";
  if (groups == 0 || groups > 0x0F) return "Choose at least one group";
  if (busy()) return "Wait for the queue to finish, so the saved positions are the ones sent";

  Preset p = {};
  strlcpy(p.name, name.c_str(), sizeof(p.name));
  for (int g = 0; g < 4; g++) {
    if (!(groups & (1 << g))) continue;
    int pos = commandedPos[g];
    if (pos < 0) return "GRP " + String(g + 1) + "'s position is unknown: set it first";
    int k = 0;
    while (k < p.n && p.steps[k].pos != pos) k++;
    if (k < p.n) p.steps[k].mask |= 1 << g;  // same position: share the frame
    else p.steps[p.n++] = {(uint8_t)(1 << g), (uint8_t)pos};
  }

  int i = findPreset(name);
  if (i >= 0 && !replace) return "A preset called \"" + String(store.p[i].name) + "\" already exists";
  PresetStore before = store;
  if (i < 0) {
    if (store.n >= MAX_PRESETS) return "No room: " + String(MAX_PRESETS) + " presets at most. Delete one first";
    i = store.n++;
  }
  store.p[i] = p;
  if (!storePresets()) {
    store = before;
    return "Couldn't save to flash";
  }
  log("saved preset %s: %s", p.name, presetSummary(i).c_str());
  return String();
}

bool deletePreset(const String &rawName) {
  String name = rawName;
  name.trim();
  int i = findPreset(name);
  if (i < 0) return false;
  PresetStore before = store;
  String gone = store.p[i].name;
  for (int k = i; k + 1 < store.n; k++) store.p[k] = store.p[k + 1];
  store.n--;
  memset(&store.p[store.n], 0, sizeof(Preset));
  if (!storePresets()) {
    store = before;
    log("couldn't delete preset %s: flash write failed", gone.c_str());
    return false;
  }
  log("deleted preset %s", gone.c_str());
  return true;
}

}  // namespace txq
