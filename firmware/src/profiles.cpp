#include "profiles.h"
#include <vector>
#include <ctype.h>
#include "storage.h"
#include "wallclock.h"
#include "calc/json.h"
#include "calc/crc32.h"
#include <time.h>

namespace profiles {
namespace {

constexpr char FILE_NAME[] = "/profiles.tsv";
constexpr char FILE_TMP[]  = "/profiles.tmp";
constexpr size_t NAME_MAX_LEN = 40;   // longer names are cut (display, run header)

struct Item { String name, etrto; Hub hub; Attr at; uint32_t epoch = 0; };   // etrto: tires, hub: hubs, at: tubes/rims
uint8_t attrCount(List l) { return l == TUBE ? 3 : l == RIM ? 4 : 0; }

std::vector<Item>    g_list[LIST_N];
std::vector<Profile> g_prof;
bool                 g_dirty = false;
SemaphoreHandle_t    g_mx = nullptr;

struct Lock {
  Lock()  { if (g_mx) xSemaphoreTake(g_mx, portMAX_DELAY); }
  ~Lock() { if (g_mx) xSemaphoreGive(g_mx); }
};

const char LIST_TAG[LIST_N] = {'T', 'U', 'R', 'H'};

// Name as stored: no TAB/line breaks, trimmed, at most NAME_MAX_LEN characters.
String clean(const char* s) {
  String o = s ? s : "";
  o.replace('\t', ' '); o.replace('\n', ' '); o.replace('\r', ' ');
  o.trim();
  if (o.length() > NAME_MAX_LEN) o = o.substring(0, NAME_MAX_LEN);
  return o;
}
bool same(const String& a, const char* b) { String c = clean(b); return a.equalsIgnoreCase(c); }

int indexOf(List l, const char* name) {
  for (size_t i = 0; i < g_list[l].size(); i++) if (same(g_list[l][i].name, name)) return static_cast<int>(i);
  return -1;
}
String& field(Profile& p, List l) { return l == TIRE ? p.tire : l == TUBE ? p.tube : l == RIM ? p.rim : p.hub; }
int usedBy(List l, const char* name) {
  int n = 0;
  for (auto& p : g_prof) if (same(field(p, l), name)) n++;
  return n;
}
void addItem(List l, const char* name, const char* etrto) {
  const String n = clean(name);
  if (!n.length()) return;
  const int i = indexOf(l, n.c_str());
  if (i < 0) { Item it; it.name = n; it.etrto = clean(etrto); it.epoch = wallclock::now(); g_list[l].push_back(it); g_dirty = true; }
  else if (l == TIRE && etrto && *etrto && !same(g_list[l][i].etrto, etrto)) { g_list[l][i].etrto = clean(etrto); g_list[l][i].epoch = wallclock::now(); g_dirty = true; }
}
int findLocked(const char* t, const char* s, const char* r, const char* h) {
  for (size_t i = 0; i < g_prof.size(); i++) {
    const Profile& p = g_prof[i];
    if (same(p.tire, t) && same(p.tube, s) && same(p.rim, r) && same(p.hub, h)) return static_cast<int>(i);
  }
  return -1;
}

String num(float v, int d) { return isnan(v) ? String("") : String(v, d); }
float  parseNum(const String& s) { return s.length() ? s.toFloat() : NAN; }

// Split one line at TAB into at most n fields.
int split(const String& line, String* f, int n) {
  int k = 0, from = 0;
  while (k < n) {
    const int t = line.indexOf('\t', from);
    if (t < 0) { f[k++] = line.substring(from); break; }
    f[k++] = line.substring(from, t);
    from = t + 1;
  }
  return k;
}

void load() {
  fs::FS* fs = storage::fs(storage::FLASH);
  if (!fs || !fs->exists(FILE_NAME)) return;
  File f = fs->open(FILE_NAME, "r");
  if (!f) return;
  const String magic = f.readStringUntil('\n');
  if (magic != "RRP1" && magic != "RRP2") { f.close(); Serial.println("# Profiles: unknown file format, ignored"); return; }
  String fl[17];
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (!line.length()) continue;
    const int n = split(line, fl, 17);
    const char tag = fl[0].length() ? fl[0][0] : 0;
    if (tag == 'P' && n >= 13) {
      Profile p;
      p.tire = fl[1]; p.tube = fl[2]; p.rim = fl[3]; p.hub = fl[4];
      p.a1 = parseNum(fl[5]); p.b1 = parseNum(fl[6]); p.uWheel = parseNum(fl[7]);
      p.mWheel = parseNum(fl[8]); p.jWheel = parseNum(fl[9]); p.bar = parseNum(fl[10]);
      p.run = fl[11]; p.epoch = strtoul(fl[12].c_str(), nullptr, 10);
      if (n >= 17) { p.bifD = parseNum(fl[13]); p.bifL = parseNum(fl[14]); p.bifN = parseNum(fl[15]); p.bifT = parseNum(fl[16]); }
      g_prof.push_back(p);
    } else {
      for (uint8_t l = 0; l < LIST_N; l++) if (tag == LIST_TAG[l] && n >= 2) {
        Item it; it.name = fl[1];
        if (l == TIRE) { it.etrto = n >= 3 ? fl[2] : ""; it.epoch = n >= 4 ? strtoul(fl[3].c_str(), nullptr, 10) : 0; }
        else if (l == HUB && n >= 8) {
          it.hub.spokes = fl[2].toInt(); it.hub.axle = fl[3]; it.hub.brake = fl[4];
          it.hub.drive = fl[5].length() ? static_cast<int8_t>(fl[5].toInt()) : -1; it.hub.colour = fl[6];
          it.epoch = strtoul(fl[7].c_str(), nullptr, 10);
        }
        else if (attrCount(static_cast<List>(l)) && n >= 3 + attrCount(static_cast<List>(l))) {
          const uint8_t c = attrCount(static_cast<List>(l));
          for (uint8_t k = 0; k < c; k++) it.at.a[k] = fl[2 + k];
          it.epoch = strtoul(fl[2 + c].c_str(), nullptr, 10);
        }
        else it.epoch = n >= 3 ? strtoul(fl[2].c_str(), nullptr, 10) : 0;
        g_list[l].push_back(it);
      }
    }
  }
  f.close();
}

// Write to a temporary file, then replace: a power cut leaves the old file.
bool save() {
  fs::FS* fs = storage::fs(storage::FLASH);
  if (!fs) return false;
  File f = fs->open(FILE_TMP, "w");
  if (!f) return false;
  f.print("RRP2\n");
  for (uint8_t l = 0; l < LIST_N; l++)
    for (auto& it : g_list[l]) {
      f.print(LIST_TAG[l]); f.print('\t'); f.print(it.name);
      if (l == TIRE) { f.print('\t'); f.print(it.etrto); }
      for (uint8_t k = 0; k < attrCount(static_cast<List>(l)); k++) { f.print('\t'); f.print(it.at.a[k]); }
      if (l == HUB) {
        f.print('\t'); f.print(it.hub.spokes); f.print('\t'); f.print(it.hub.axle); f.print('\t'); f.print(it.hub.brake);
        f.print('\t'); if (it.hub.drive >= 0) f.print(it.hub.drive); f.print('\t'); f.print(it.hub.colour);
      }
      f.print('\t'); f.print(it.epoch); f.print('\n');
    }
  for (auto& p : g_prof) {
    f.print("P\t"); f.print(p.tire); f.print('\t'); f.print(p.tube); f.print('\t'); f.print(p.rim); f.print('\t'); f.print(p.hub);
    f.print('\t'); f.print(num(p.a1, 6)); f.print('\t'); f.print(num(p.b1, 9)); f.print('\t'); f.print(num(p.uWheel, 4));
    f.print('\t'); f.print(num(p.mWheel, 3)); f.print('\t'); f.print(num(p.jWheel, 5)); f.print('\t'); f.print(num(p.bar, 2));
    f.print('\t'); f.print(p.run); f.print('\t'); f.print(p.epoch);
    f.print('\t'); f.print(num(p.bifD, 1)); f.print('\t'); f.print(num(p.bifL, 1)); f.print('\t'); f.print(num(p.bifN, 0)); f.print('\t'); f.print(num(p.bifT, 2));
    f.print('\n');
  }
  const bool ok = f.getWriteError() == 0;
  f.close();
  if (!ok) { fs->remove(FILE_TMP); return false; }
  fs->remove(FILE_NAME);
  return fs->rename(FILE_TMP, FILE_NAME);
}

void jstr(String& o, const String& s) {
  char b[2 * NAME_MAX_LEN + 8];
  calc::jsonStr(b, sizeof(b), s.c_str());
  o += b;
}
void jnum(String& o, float v, int d) { if (isnan(v)) o += "null"; else o += String(v, d); }

String missingLocked(int i) {
  if (i < 0 || i >= static_cast<int>(g_prof.size())) return "no profile";
  const Profile& p = g_prof[i];
  String m;
  auto add = [&](const char* w) { if (m.length()) m += ", "; m += w; };
  if (!p.tire.length()) add("tire");
  else { const int t = indexOf(TIRE, p.tire.c_str()); if (t < 0 || !g_list[TIRE][t].etrto.length()) add("ETRTO"); }
  if (!p.tube.length()) add("tube");
  else { const int u = indexOf(TUBE, p.tube.c_str()); if (u < 0 || !g_list[TUBE][u].at.a[2].length()) add("tube material"); }
  if (!p.rim.length()) add("rim");
  else {
    const int r = indexOf(RIM, p.rim.c_str());
    const Attr ra = r >= 0 ? g_list[RIM][r].at : Attr();
    if (ra.a[2].toFloat() <= 0.0f) add("rim inner width");
    if (ra.a[3].toInt() <= 0) add("rim diameter");
    else {   // bead seat diameter of rim and tire must agree: ETRTO "28-622" -> 622
      const int t = indexOf(TIRE, p.tire.c_str());
      const String e = t >= 0 ? g_list[TIRE][t].etrto : String("");
      const int dash = e.indexOf('-');
      if (dash > 0 && e.substring(dash + 1).toInt() != ra.a[3].toInt()) add("rim diameter does not match the tire ETRTO");
    }
  }
  if (!p.hub.length()) add("hub");
  else {
    const int h = indexOf(HUB, p.hub.c_str());
    const Hub hb = h >= 0 ? g_list[HUB][h].hub : Hub();
    if (hb.spokes <= 0) add("spokes");
    if (!hb.axle.length()) add("axle");
    if (!hb.brake.length()) add("brake");
    if (hb.drive < 0) add("drive wheel");
  }
  if (isnan(p.mWheel)) add("mass");
  return m;
}

// ---------------------------------------------------------------- SD copy (wheels.md)
// The four lists also live on the SD card as /vmrr/wheels.md, Markdown tables that can be
// edited on any computer (2026-11-26). The flash stays the master: the stand works
// without a card. The file ends with "<!-- parts crc XXXXXXXX -->", the CRC of everything
// above it as the stand wrote it. When the card is mounted the stand reads the file; if
// the CRC no longer matches (or is missing), someone edited it and the tables are taken
// over: rows added become entries, changed rows update them, rows removed delete the
// entry unless a profile uses it. Afterwards the file is written back in normal form.
constexpr char MD_NAME[] = "/vmrr/wheels.md";
constexpr char MD_TMP[]  = "/vmrr/wheels.tmp";
constexpr char MD_STAMP[] = "<!-- parts crc ";
constexpr size_t MD_MAX = 32768;            // larger files are not read
bool g_mdDirty = true;                      // write once after boot (after the import)
bool g_sdSeen  = false;                     // card handled since it was mounted
uint16_t g_sdMountSeen = 0;                 // storage::sdMounts() handled: a format remounts
                                            // fast enough that READY never drops in between
const char* const MD_HEAD[LIST_N] = {"Tires", "Tubes", "Rims", "Hubs"};
const char* const MD_COLS[LIST_N] = {
  "| Name | ETRTO | Changed |\n|---|---|---|\n",
  "| Name | Maker | Model | Material | Changed |\n|---|---|---|---|---|\n",
  "| Name | Maker | Model | Inner width mm | Bead seat mm | Changed |\n|---|---|---|---|---|---|\n",
  "| Name | Spokes | Axle | Brake | Drive wheel | Colour | Changed |\n|---|---|---|---|---|---|---|\n"};
constexpr uint8_t MD_CELLS = 6;

uint8_t cellsOf(List l, const Item& it, String* c) {
  c[0] = it.name;
  if (l == TIRE) { c[1] = it.etrto; return 2; }
  if (l == HUB) {
    c[1] = it.hub.spokes > 0 ? String(it.hub.spokes) : String("");
    c[2] = it.hub.axle; c[3] = it.hub.brake;
    c[4] = it.hub.drive < 0 ? "" : it.hub.drive ? "yes" : "no";
    c[5] = it.hub.colour; return 6;
  }
  const uint8_t n = attrCount(l);
  for (uint8_t k = 0; k < n; k++) c[1 + k] = it.at.a[k];
  return 1 + n;
}
String dateText(uint32_t e) {
  if (!e) return "";
  time_t t = e; struct tm tm; gmtime_r(&t, &tm);   // epoch is local time already
  char b[20]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M", &tm);
  return b;
}
String mdText() {
  String o;
  o += "# Wheel parts\n\nTires, tubes, rims and hubs as the test stand knows them (Setup, wheel profiles).\n"
       "Edit the tables on a computer and put the card back: the stand reads this file when\n"
       "the card is mounted. A new row is a new entry, a removed row deletes the entry unless\n"
       "a profile uses it. \"Changed\" is set by the stand. Profiles and wheel models stay in\n"
       "the stand's flash.\n";
  for (uint8_t l = 0; l < LIST_N; l++) {
    o += "\n## "; o += MD_HEAD[l]; o += "\n\n"; o += MD_COLS[l];
    for (auto& it : g_list[l]) {
      String c[MD_CELLS]; const uint8_t n = cellsOf(static_cast<List>(l), it, c);
      o += "|";
      for (uint8_t k = 0; k < n; k++) { String v = c[k]; v.replace("|", "/"); o += " "; o += v; o += " |"; }
      o += " "; o += dateText(it.epoch); o += " |\n";
    }
  }
  return o;
}
uint32_t crcOf(const String& s) {
  return calc::crc32(0, reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
}
bool writeMd() {
  fs::FS* f = storage::fs(storage::SD_CARD);
  if (!f || storage::info(storage::SD_CARD).state != storage::READY) return false;
  if (!storage::makeDir(*f, storage::ROOT_DIR)) return false;
  const String body = mdText();
  char stamp[40]; snprintf(stamp, sizeof(stamp), "\n%s%08lx -->\n", MD_STAMP, static_cast<unsigned long>(crcOf(body)));
  File o = f->open(MD_TMP, "w");
  if (!o) return false;
  o.print(body); o.print(stamp);
  const bool ok = o.getWriteError() == 0;
  o.close();
  if (!ok) { f->remove(MD_TMP); return false; }
  f->remove(MD_NAME);
  return f->rename(MD_TMP, MD_NAME);
}

// Split a Markdown table row into cells; 0 for anything that is not a data row.
int mdCells(String line, String* c, int maxc) {
  line.trim();
  if (!line.startsWith("|")) return 0;
  line = line.substring(1);
  if (line.endsWith("|")) line = line.substring(0, line.length() - 1);
  int n = 0, from = 0;
  while (n < maxc) {
    const int bar = line.indexOf('|', from);
    String v = bar < 0 ? line.substring(from) : line.substring(from, bar);
    v.trim(); c[n++] = v;
    if (bar < 0) break;
    from = bar + 1;
  }
  bool rule = true;   // |---|:--| separator row
  for (int k = 0; k < n; k++) for (size_t i = 0; i < c[k].length(); i++) if (c[k][i] != '-' && c[k][i] != ':') rule = false;
  if (rule || c[0].equalsIgnoreCase("Name")) return 0;
  return n;
}
// Take over one row; true if something changed.
bool applyRow(List l, String* c, int n) {
  const String name = clean(c[0].c_str());
  if (!name.length()) return false;
  Item nw; nw.name = name;
  const int i = indexOf(l, name.c_str());
  if (i >= 0) nw = g_list[l][i];
  auto cell = [&](int k) { return k < n ? clean(c[k].c_str()) : String(""); };
  if (l == TIRE) nw.etrto = cell(1);
  else if (l == HUB) {
    nw.hub.spokes = cell(1).toInt(); nw.hub.axle = cell(2); nw.hub.brake = cell(3);
    const String d = cell(4);
    nw.hub.drive = d.equalsIgnoreCase("yes") || d == "1" ? 1 : d.equalsIgnoreCase("no") || d == "0" ? 0 : -1;
    nw.hub.colour = cell(5);
  } else for (uint8_t k = 0; k < attrCount(l); k++) nw.at.a[k] = cell(1 + k);
  String a[MD_CELLS], b[MD_CELLS];
  const uint8_t na = cellsOf(l, nw, a);
  bool diff = i < 0;
  if (!diff) { cellsOf(l, g_list[l][i], b); for (uint8_t k = 0; k < na; k++) if (a[k] != b[k]) diff = true; }
  if (!diff) return false;
  nw.epoch = wallclock::now();
  if (i < 0) g_list[l].push_back(nw); else g_list[l][i] = nw;
  g_dirty = true;
  return true;
}
void importMd() {
  fs::FS* f = storage::fs(storage::SD_CARD);
  if (!f) return;
  if (!f->exists(MD_NAME)) { g_mdDirty = true; return; }   // new or empty card: write the copy from the flash
  File in = f->open(MD_NAME, "r");
  if (!in) return;
  if (in.size() > MD_MAX) { in.close(); Serial.println("# Parts: wheels.md too large, ignored"); return; }
  String text = in.readString();
  in.close();
  text.replace("\r", "");
  const int st = text.lastIndexOf(MD_STAMP);
  if (st >= 0) {
    const String body = text.substring(0, st > 0 && text[st - 1] == '\n' ? st - 1 : st);
    const uint32_t want = strtoul(text.c_str() + st + strlen(MD_STAMP), nullptr, 16);
    if (crcOf(body) == want) return;   // as written by the stand: nothing to take over
  }
  int sec = -1, added = 0, changed = 0, removed = 0;
  std::vector<String> seen[LIST_N];
  bool had[LIST_N] = {false, false, false, false};
  int from = 0;
  while (from < static_cast<int>(text.length())) {
    int nl = text.indexOf('\n', from); if (nl < 0) nl = text.length();
    String line = text.substring(from, nl); from = nl + 1;
    line.trim();
    if (line.startsWith("## ")) {
      String h = line.substring(3); h.trim(); h.toLowerCase();
      sec = h.startsWith("tire") ? TIRE : h.startsWith("tube") ? TUBE : h.startsWith("rim") ? RIM : h.startsWith("hub") ? HUB : -1;
      if (sec >= 0) had[sec] = true;
      continue;
    }
    if (sec < 0) continue;
    String c[MD_CELLS + 2];
    const int n = mdCells(line, c, MD_CELLS + 2);
    if (n < 1) continue;
    const List l = static_cast<List>(sec);
    const bool isNew = indexOf(l, c[0].c_str()) < 0;
    if (applyRow(l, c, n)) { if (isNew) added++; else changed++; }
    seen[sec].push_back(clean(c[0].c_str()));
  }
  for (uint8_t l = 0; l < LIST_N; l++) {
    if (!had[l]) continue;   // a section missing from the file deletes nothing
    for (size_t i = g_list[l].size(); i-- > 0;) {
      bool keep = false;
      for (auto& s : seen[l]) if (g_list[l][i].name.equalsIgnoreCase(s)) keep = true;
      if (keep || usedBy(static_cast<List>(l), g_list[l][i].name.c_str())) continue;
      g_list[l].erase(g_list[l].begin() + i); removed++; g_dirty = true;
    }
  }
  g_mdDirty = true;
  Serial.printf("# Parts: wheels.md taken over, %d new, %d changed, %d removed\n", added, changed, removed);
}

}  // namespace

bool addRow(char tag, const char* row, const char** why) {
  const char* const T = "TURH";
  const char* p = strchr(T, toupper(tag));
  if (!tag || !p) { *why = "List T, U, R or H"; return false; }
  String c[MD_CELLS + 2];
  const int n = mdCells(String(row), c, MD_CELLS + 2);
  if (n < 1 || !clean(c[0].c_str()).length()) { *why = "Row | name | ... |"; return false; }
  Lock l;
  const List lst = static_cast<List>(p - T);
  // Console rows only add: an empty cell keeps what the entry already has (the file
  // import, by contrast, takes an emptied cell as cleared).
  const int i = indexOf(lst, c[0].c_str());
  int m = n;
  if (i >= 0) {
    String old[MD_CELLS]; m = cellsOf(lst, g_list[lst][i], old);
    for (int k = 1; k < m; k++) if (k >= n || !c[k].length()) c[k] = old[k];
  }
  applyRow(lst, c, m > n ? m : n);
  return true;
}

void begin() {
  g_mx = xSemaphoreCreateMutex();
  Lock l;
  load();
  Serial.print("# Profiles: "); Serial.print(g_prof.size()); Serial.print(" combinations, ");
  Serial.print(g_list[TIRE].size()); Serial.print(" tires, "); Serial.print(g_list[TUBE].size()); Serial.print(" tubes, ");
  Serial.print(g_list[RIM].size()); Serial.print(" rims, "); Serial.print(g_list[HUB].size()); Serial.println(" hubs");
}

void service() {
  // SD copy: on (re)mount take over edits first, then write when something changed.
  // Only at standstill, like the flash (no SD traffic in the middle of a run).
  const bool sdOk = storage::info(storage::SD_CARD).state == storage::READY;
  if (!sdOk || storage::sdMounts() != g_sdMountSeen) g_sdSeen = false;
  if (sdOk && !g_sdSeen && storage::flashWritable()) {
    Lock l; storage::lockSd(); importMd(); storage::unlockSd(); g_sdSeen = true; g_sdMountSeen = storage::sdMounts();
  }
  if (!g_dirty && !g_mdDirty) return;
  if (!storage::flashWritable()) return;
  Lock l;
  if (g_dirty) {
    g_mdDirty = true;
    if (save()) g_dirty = false;
    else { Serial.println("# Profiles: not saved (flash)"); g_dirty = false; }   // no retry loop on a broken flash
  }
  if (g_mdDirty && g_sdSeen) {
    storage::lockSd();
    if (writeMd()) g_mdDirty = false;
    storage::unlockSd();
  }
}

int find(const char* t, const char* s, const char* r, const char* h) { Lock l; return findLocked(t, s, r, h); }
String missing(int i) { Lock l; return missingLocked(i); }

int save(int i, const char* tire, const char* etrto, const char* tube, const char* rim, const char* hub,
         float massKg, float bifD, float bifL, float bifN, float bifT, float jWheel, const char** why) {
  Lock l;
  if (i >= static_cast<int>(g_prof.size())) { *why = "Not found"; return -1; }
  if (!clean(tire).length()) { *why = "Tire empty"; return -1; }
  const int other = findLocked(tire, tube, rim, hub);
  if (other >= 0 && other != i) { *why = "Profile exists"; return -1; }
  addItem(TIRE, tire, etrto); addItem(TUBE, tube, nullptr); addItem(RIM, rim, nullptr); addItem(HUB, hub, nullptr);
  if (i < 0) { g_prof.push_back(Profile()); i = static_cast<int>(g_prof.size()) - 1; }
  Profile& p = g_prof[i];
  const bool changed = !same(p.tire, tire) || !same(p.tube, tube) || !same(p.rim, rim) || !same(p.hub, hub);
  p.tire = clean(tire); p.tube = clean(tube); p.rim = clean(rim); p.hub = clean(hub);
  if (changed) { p.a1 = p.b1 = p.bar = p.uWheel = NAN; p.run = ""; p.epoch = 0; }   // the model was the old wheel's
  p.mWheel = massKg > 0.0f ? massKg : NAN;
  p.bifD = bifD; p.bifL = bifL; p.bifN = bifN; p.bifT = bifT;
  p.jWheel = jWheel > 0.0f ? jWheel : NAN;
  g_dirty = true;
  return i;
}
bool setHub(const char* name, const Hub& h, const char** why) {
  Lock l;
  if (!clean(name).length()) { *why = "Hub empty"; return false; }
  addItem(HUB, name, nullptr);
  Item& it = g_list[HUB][indexOf(HUB, name)];
  it.hub = h; it.hub.axle = clean(h.axle.c_str()); it.hub.brake = clean(h.brake.c_str()); it.hub.colour = clean(h.colour.c_str());
  it.epoch = wallclock::now();
  g_dirty = true;
  return true;
}
bool setAttr(List lst, const char* name, const Attr& at, const char** why) {
  Lock l;
  if (!attrCount(lst)) { *why = "No details for this list"; return false; }
  if (!clean(name).length()) { *why = "Name empty"; return false; }
  addItem(lst, name, nullptr);
  Item& it = g_list[lst][indexOf(lst, name)];
  for (uint8_t k = 0; k < ATTR_N; k++) it.at.a[k] = k < attrCount(lst) ? clean(at.a[k].c_str()) : String("");
  it.epoch = wallclock::now();
  g_dirty = true;
  return true;
}
bool attr(List lst, const char* name, Attr& out) {
  Lock l;
  const int i = indexOf(lst, name);
  if (i < 0) return false;
  out = g_list[lst][i].at;
  return true;
}
String etrto(const char* tire) {
  Lock l;
  const int i = indexOf(TIRE, tire);
  return i < 0 ? String("") : g_list[TIRE][i].etrto;
}
bool hub(const char* name, Hub& out) {
  Lock l;
  const int i = indexOf(HUB, name);
  if (i < 0) return false;
  out = g_list[HUB][i].hub;
  return true;
}

bool get(int i, Profile& out) {
  Lock l;
  if (i < 0 || i >= static_cast<int>(g_prof.size())) return false;
  out = g_prof[i];
  return true;
}


void setModel(int i, float a1, float b1, float uWheel, float bar, const char* run, uint32_t epoch) {
  Lock l;
  if (i < 0 || i >= static_cast<int>(g_prof.size())) return;
  Profile& p = g_prof[i];
  p.a1 = a1; p.b1 = b1; if (!isnan(uWheel)) p.uWheel = uWheel; p.bar = bar;
  p.run = run ? run : ""; p.epoch = epoch;
  g_dirty = true;
}
void setMass(int i, float kg) {
  Lock l;
  if (i < 0 || i >= static_cast<int>(g_prof.size()) || isnan(kg)) return;
  if (g_prof[i].mWheel != kg) { g_prof[i].mWheel = kg; g_dirty = true; }
}
void setJ(int i, float j) {
  Lock l;
  if (i < 0 || i >= static_cast<int>(g_prof.size())) return;
  if (!(g_prof[i].jWheel == j) && !(isnan(g_prof[i].jWheel) && isnan(j))) { g_prof[i].jWheel = j; g_dirty = true; }
}

bool rename(List lst, const char* from, const char* to, const char** why) {
  Lock l;
  const int i = indexOf(lst, from);
  const String n = clean(to);
  if (i < 0) { *why = "Not found"; return false; }
  if (!n.length()) { *why = "Name empty"; return false; }
  const int j = indexOf(lst, n.c_str());
  if (j >= 0 && j != i) { *why = "Name exists"; return false; }
  const String old = g_list[lst][i].name;
  g_list[lst][i].name = n; g_list[lst][i].epoch = wallclock::now();
  for (auto& p : g_prof) if (same(field(p, lst), old.c_str())) field(p, lst) = n;
  g_dirty = true;
  return true;
}
bool remove(List lst, const char* name, const char** why) {
  Lock l;
  const int i = indexOf(lst, name);
  if (i < 0) { *why = "Not found"; return false; }
  if (usedBy(lst, name)) { *why = "Used by a profile"; return false; }
  g_list[lst].erase(g_list[lst].begin() + i);
  g_dirty = true;
  return true;
}
bool setEtrto(const char* tire, const char* etrto, const char** why) {
  Lock l;
  const int i = indexOf(TIRE, tire);
  if (i < 0) { *why = "Not found"; return false; }
  g_list[TIRE][i].etrto = clean(etrto); g_list[TIRE][i].epoch = wallclock::now();
  g_dirty = true;
  return true;
}
bool removeProfile(int i, const char** why) {
  Lock l;
  if (i < 0 || i >= static_cast<int>(g_prof.size())) { *why = "Not found"; return false; }
  g_prof.erase(g_prof.begin() + i);
  g_dirty = true;
  return true;
}
bool clearModel(int i, const char** why) {
  Lock l;
  if (i < 0 || i >= static_cast<int>(g_prof.size())) { *why = "Not found"; return false; }
  Profile& p = g_prof[i];
  p.a1 = p.b1 = p.bar = NAN; p.run = ""; p.epoch = 0;
  g_dirty = true;
  return true;
}

void json(String& o) {
  Lock l;
  static const char* const KEY[LIST_N] = {"tire", "tube", "rim", "hub"};
  o = "{";
  for (uint8_t k = 0; k < LIST_N; k++) {
    o += k ? ",\"" : "\""; o += KEY[k]; o += "\":[";
    for (size_t i = 0; i < g_list[k].size(); i++) {
      o += i ? ",{\"n\":" : "{\"n\":"; jstr(o, g_list[k][i].name);
      if (k == TIRE) { o += ",\"e\":"; jstr(o, g_list[k][i].etrto); }
      if (attrCount(static_cast<List>(k))) {
        o += ",\"a\":[";
        for (uint8_t a = 0; a < attrCount(static_cast<List>(k)); a++) { if (a) o += ","; jstr(o, g_list[k][i].at.a[a]); }
        o += "]";
      }
      if (k == HUB) {
        const Hub& h = g_list[k][i].hub;
        o += ",\"sp\":"; o += h.spokes; o += ",\"ax\":"; jstr(o, h.axle); o += ",\"br\":"; jstr(o, h.brake);
        o += ",\"dr\":"; o += h.drive; o += ",\"co\":"; jstr(o, h.colour);
      }
      o += ",\"use\":"; o += usedBy(static_cast<List>(k), g_list[k][i].name.c_str());
      o += ",\"ep\":"; o += g_list[k][i].epoch; o += "}";
    }
    o += "]";
  }
  o += ",\"prof\":[";
  for (size_t i = 0; i < g_prof.size(); i++) {
    const Profile& p = g_prof[i];
    o += i ? ",{\"t\":" : "{\"t\":"; jstr(o, p.tire);
    o += ",\"s\":"; jstr(o, p.tube); o += ",\"r\":"; jstr(o, p.rim); o += ",\"h\":"; jstr(o, p.hub);
    o += ",\"a1\":"; jnum(o, p.a1, 6); o += ",\"b1\":"; jnum(o, p.b1, 9); o += ",\"u\":"; jnum(o, p.uWheel, 4);
    o += ",\"m\":"; jnum(o, p.mWheel, 3); o += ",\"j\":"; jnum(o, p.jWheel, 5); o += ",\"bar\":"; jnum(o, p.bar, 2);
    o += ",\"run\":"; jstr(o, p.run); o += ",\"ep\":"; o += p.epoch;
    o += ",\"bd\":"; jnum(o, p.bifD, 1); o += ",\"bl\":"; jnum(o, p.bifL, 1); o += ",\"bn\":"; jnum(o, p.bifN, 0); o += ",\"bt\":"; jnum(o, p.bifT, 2);
    o += ",\"miss\":"; jstr(o, missingLocked(static_cast<int>(i))); o += "}";
  }
  o += "],\"pend\":"; o += g_dirty ? "true" : "false"; o += "}";
}

}  // namespace profiles
