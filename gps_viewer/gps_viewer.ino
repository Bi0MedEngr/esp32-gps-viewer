// GPS viewer: ESP32-S3 N16R8 + GY-GPS6MV2 (u-blox NEO-6M), served as a web app.
//
// The S3 reads NMEA from the GPS on UART1, parses GGA / RMC / GSA / GSV itself
// (no GPS library), and serves a page at http://gps.local (or the IP printed on
// the USB serial port) with live coordinates, a map, a track and a sky plot.
//
// Wi-Fi:
//   - Joins the network in wifi_secrets.h, then the one saved in flash.
//     If neither works, it opens the "GPS-Setup" portal for 3 minutes: join it from a
//     phone and pick a network.
//   - It always runs its own access point as well, "ESP32-GPS" / password gps12345,
//     with the app at http://192.168.4.1. Use this when the shared network blocks
//     device-to-device traffic (guest networks often do). The map tiles need internet,
//     so on the access point you get coordinates without the map.
//   - Hold BOOT while pressing RST to forget the saved network and open the portal.
//
// Wiring: see WIRING.md (GPS TX -> GPIO 18, GPS RX -> GPIO 17).

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include "page.h"
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

#define GPS_RX_PIN 18   // <- GPS TX  (not the board RX pin: 43/44 are tied to the onboard USB-serial chip)
#define GPS_TX_PIN 17   // -> GPS RX
#define RGB_PIN    48
#define BOOT_PIN   0

static const char *AP_SSID = "ESP32-GPS";
static const char *AP_PASS = "gps12345";
static const char *HOSTNAME = "gps";

WebServer server(80);

// ---------------------------------------------------------------- GPS state
struct Sat { uint8_t prn; int8_t el; int16_t az; int8_t snr; };  // snr -1 = not tracked

struct GpsState {
  bool   haveLoc = false;
  double lat = 0, lon = 0;
  float  alt = NAN, geoid = NAN, hdop = NAN, pdop = NAN, vdop = NAN;
  float  spdKmh = NAN, course = NAN;
  uint8_t fixQ = 0;       // GGA: 0 none, 1 GPS, 2 DGPS
  uint8_t mode = 1;       // GSA: 1 none, 2 2D, 3 3D
  uint8_t satsUsed = 0;
  bool   rmcValid = false;
  char   time[12] = "";   // hhmmss.ss
  char   date[8]  = "";   // ddmmyy
  uint32_t locMs = 0;     // millis() of last good position
  uint32_t lastSentenceMs = 0;
  uint32_t sentences = 0, badChecksum = 0, rxBytes = 0;
  Sat inView[40]; uint8_t nInView = 0;
  uint8_t used[12]; uint8_t nUsed = 0;  // PRNs in the fix (GSA)
  Sat pend[40];   uint8_t nPend = 0;
} gps;

// raw NMEA ring for the console
#define RAW_LINES 40
static char rawRing[RAW_LINES][100];
static uint32_t rawSeq = 0;

// baud autodetect
static const uint32_t BAUDS[] = {9600, 38400, 115200, 57600, 4800};
static uint8_t baudIdx = 0;
static uint32_t baudSince = 0;
static bool baudLocked = false;

// ---------------------------------------------------------------- NMEA parsing
static int splitFields(char *s, char *f[], int maxF) {
  int n = 0;
  f[n++] = s;
  for (char *p = s; *p && n < maxF; p++)
    if (*p == ',') { *p = 0; f[n++] = p + 1; }
  return n;
}

static bool checksumOk(const char *line) {
  // line starts with '$', contains '*hh'
  const char *star = strrchr(line, '*');
  if (!star || strlen(star) < 3) return false;
  uint8_t c = 0;
  for (const char *p = line + 1; p < star; p++) c ^= (uint8_t)*p;
  return c == (uint8_t)strtol(star + 1, nullptr, 16);
}

static double nmeaToDeg(const char *v, const char *hemi) {
  if (!*v) return NAN;
  double raw = atof(v);
  int deg = (int)(raw / 100);
  double d = deg + (raw - deg * 100) / 60.0;
  if (*hemi == 'S' || *hemi == 'W') d = -d;
  return d;
}

static float fOrNan(const char *v) { return *v ? (float)atof(v) : NAN; }

static void handleSentence(char *line) {
  char *star = strrchr(line, '*');
  if (star) *star = 0;
  char *f[24];
  int n = splitFields(line + 1, f, 24);
  const char *type = f[0];
  if (strlen(type) < 5) return;
  const char *t = type + 2;  // skip talker (GP, GN, GL...)

  if (!strcmp(t, "GGA") && n >= 10) {
    strlcpy(gps.time, f[1], sizeof gps.time);
    gps.fixQ = atoi(f[6]);
    gps.satsUsed = atoi(f[7]);
    gps.hdop = fOrNan(f[8]);
    gps.alt = fOrNan(f[9]);
    if (n >= 12) gps.geoid = fOrNan(f[11]);
    if (gps.fixQ > 0 && *f[2] && *f[4]) {
      gps.lat = nmeaToDeg(f[2], f[3]);
      gps.lon = nmeaToDeg(f[4], f[5]);
      gps.haveLoc = true;
      gps.locMs = millis();
    }
  } else if (!strcmp(t, "RMC") && n >= 10) {
    strlcpy(gps.time, f[1], sizeof gps.time);
    gps.rmcValid = (*f[2] == 'A');
    gps.spdKmh = *f[7] ? atof(f[7]) * 1.852f : NAN;
    gps.course = fOrNan(f[8]);
    strlcpy(gps.date, f[9], sizeof gps.date);
    if (gps.rmcValid && *f[3] && *f[5]) {
      gps.lat = nmeaToDeg(f[3], f[4]);
      gps.lon = nmeaToDeg(f[5], f[6]);
      gps.haveLoc = true;
      gps.locMs = millis();
    }
  } else if (!strcmp(t, "GSA") && n >= 18) {
    gps.mode = atoi(f[2]);
    gps.nUsed = 0;
    for (int i = 3; i <= 14; i++)
      if (*f[i] && gps.nUsed < 12) gps.used[gps.nUsed++] = atoi(f[i]);
    gps.pdop = fOrNan(f[15]);
    gps.hdop = fOrNan(f[16]);
    gps.vdop = fOrNan(f[17]);
  } else if (!strcmp(t, "GSV") && n >= 4) {
    int total = atoi(f[1]), num = atoi(f[2]);
    if (num == 1) gps.nPend = 0;
    for (int i = 4; i < n; i += 4) {
      if (!*f[i] || gps.nPend >= 40) continue;
      Sat &s = gps.pend[gps.nPend++];
      s.prn = atoi(f[i]);
      s.el  = (i + 1 < n && *f[i + 1]) ? atoi(f[i + 1]) : -1;
      s.az  = (i + 2 < n && *f[i + 2]) ? atoi(f[i + 2]) : -1;
      s.snr = (i + 3 < n && *f[i + 3]) ? atoi(f[i + 3]) : -1;
    }
    if (num == total) {
      memcpy(gps.inView, gps.pend, sizeof(Sat) * gps.nPend);
      gps.nInView = gps.nPend;
    }
  }
}

static char lineBuf[120];
static uint8_t lineLen = 0;

static void pollGps() {
  while (Serial1.available()) {
    char c = Serial1.read();
    gps.rxBytes++;
    if (c == '$') { lineLen = 0; lineBuf[lineLen++] = c; continue; }
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = 0;
      if (lineLen > 6 && lineBuf[0] == '$') {
        strlcpy(rawRing[rawSeq % RAW_LINES], lineBuf, sizeof rawRing[0]);
        rawSeq++;
        if (checksumOk(lineBuf)) {
          gps.sentences++;
          gps.lastSentenceMs = millis();
          if (!baudLocked) {
            baudLocked = true;
            Serial.printf("GPS: NMEA found at %lu baud\n", (unsigned long)BAUDS[baudIdx]);
          }
          handleSentence(lineBuf);
        } else {
          gps.badChecksum++;
        }
      }
      lineLen = 0;
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
    else lineLen = 0;
  }

  // No valid sentence for 4 s: try the next baud rate (until one has worked once).
  uint32_t now = millis();
  if (!baudLocked && now - baudSince > 4000) {
    baudIdx = (baudIdx + 1) % (sizeof(BAUDS) / sizeof(BAUDS[0]));
    Serial1.updateBaudRate(BAUDS[baudIdx]);
    baudSince = now;
    Serial.printf("GPS: no NMEA yet (%lu bytes so far), trying %lu baud\n",
                  (unsigned long)gps.rxBytes, (unsigned long)BAUDS[baudIdx]);
  }
}

// ---------------------------------------------------------------- JSON
static void jNum(String &o, const char *k, double v, int dp) {
  o += '"'; o += k; o += "\":";
  if (isnan(v)) o += "null"; else o += String(v, dp);
  o += ',';
}

static void handleApiGps() {
  uint32_t now = millis();
  String o; o.reserve(2048);
  o += '{';
  o += "\"up\":" + String(now / 1000) + ',';
  o += "\"rxBytes\":" + String(gps.rxBytes) + ',';
  o += "\"sentences\":" + String(gps.sentences) + ',';
  o += "\"badChecksum\":" + String(gps.badChecksum) + ',';
  o += "\"baud\":" + String(BAUDS[baudIdx]) + ',';
  o += "\"baudLocked\":" + String(baudLocked ? "true" : "false") + ',';
  o += "\"dataAge\":" + (gps.lastSentenceMs ? String(now - gps.lastSentenceMs) : String("null")) + ',';
  o += "\"fixQ\":" + String(gps.fixQ) + ',';
  o += "\"mode\":" + String(gps.mode) + ',';
  o += "\"rmcValid\":" + String(gps.rmcValid ? "true" : "false") + ',';
  o += "\"satsUsed\":" + String(gps.satsUsed) + ',';
  o += "\"haveLoc\":" + String(gps.haveLoc ? "true" : "false") + ',';
  o += "\"locAge\":" + (gps.haveLoc ? String(now - gps.locMs) : String("null")) + ',';
  jNum(o, "lat", gps.haveLoc ? gps.lat : NAN, 7);
  jNum(o, "lon", gps.haveLoc ? gps.lon : NAN, 7);
  jNum(o, "alt", gps.alt, 1);
  jNum(o, "geoid", gps.geoid, 1);
  jNum(o, "hdop", gps.hdop, 2);
  jNum(o, "pdop", gps.pdop, 2);
  jNum(o, "vdop", gps.vdop, 2);
  jNum(o, "spd", gps.spdKmh, 2);
  jNum(o, "crs", gps.course, 1);
  o += "\"time\":\""; o += gps.time; o += "\",";
  o += "\"date\":\""; o += gps.date; o += "\",";
  o += "\"sats\":[";
  for (int i = 0; i < gps.nInView; i++) {
    const Sat &s = gps.inView[i];
    if (i) o += ',';
    bool inFix = false;
    for (int j = 0; j < gps.nUsed; j++) if (gps.used[j] == s.prn) inFix = true;
    o += '[' + String(s.prn) + ',' + String(s.el) + ',' + String(s.az) + ',' + String(s.snr) + ',' + (inFix ? '1' : '0') + ']';
  }
  o += "],";
  o += "\"rawSeq\":" + String(rawSeq);
  o += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", o);
}

static void handleApiNmea() {
  // ?since=N returns lines with sequence > N (at most RAW_LINES)
  uint32_t since = server.hasArg("since") ? strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
  uint32_t first = rawSeq > RAW_LINES ? rawSeq - RAW_LINES : 0;
  if (since > first) first = since;
  String o; o.reserve(4096);
  o += String(rawSeq) + '\n';
  for (uint32_t i = first; i < rawSeq; i++) { o += rawRing[i % RAW_LINES]; o += '\n'; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", o);
}

// ---------------------------------------------------------------- LED
static void updateLed() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 250) return;
  last = now;
  bool blink = (now / 500) % 2;
  bool live = gps.lastSentenceMs && now - gps.lastSentenceMs < 3000;
  bool fix = live && gps.haveLoc && now - gps.locMs < 3000;
  if (!live)        rgbLedWrite(RGB_PIN, blink ? 20 : 0, 0, 0);   // red blink: no data from GPS
  else if (!fix)    rgbLedWrite(RGB_PIN, blink ? 20 : 0, 12, 0);  // amber blink: searching
  else              rgbLedWrite(RGB_PIN, 0, 20, 0);               // green: fix
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\nGPS viewer starting (reset reason %d)\n", (int)esp_reset_reason());

  pinMode(BOOT_PIN, INPUT_PULLUP);
  rgbLedWrite(RGB_PIN, 0, 0, 20);

  Serial1.setRxBufferSize(2048);
  Serial1.begin(BAUDS[0], SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  baudSince = millis();

  WiFi.setHostname(HOSTNAME);
  WiFiManager wm;
  if (digitalRead(BOOT_PIN) == LOW) {
    Serial.println("BOOT held: forgetting saved Wi-Fi");
    wm.resetSettings();
  }
  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  bool sta = false;
  if (digitalRead(BOOT_PIN) == HIGH && strlen(WIFI_SSID)) {
    Serial.println("Joining " WIFI_SSID "...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (uint32_t t0 = millis(); millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED;) delay(100);
    sta = WiFi.status() == WL_CONNECTED;
  }
  if (!sta) {
    Serial.println("Trying saved Wi-Fi, then portal \"GPS-Setup\"...");
    sta = wm.autoConnect("GPS-Setup");
  }

  WiFi.mode(sta ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  if (sta) Serial.printf("Joined %s  ->  http://%s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  else     Serial.println("Not on a network (portal timed out)");
  Serial.printf("Own access point %s / %s  ->  http://%s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("Also at http://%s.local\n", HOSTNAME);
  }

  server.on("/", [] { server.send_P(200, "text/html", PAGE_HTML); });
  server.on("/api/gps", handleApiGps);
  server.on("/api/nmea", handleApiNmea);
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();

  // throw away whatever piled up in the UART while Wi-Fi was connecting
  while (Serial1.available()) Serial1.read();
  baudSince = millis();
}

void loop() {
  pollGps();
  server.handleClient();
  updateLed();

  static uint32_t lastLog = 0;
  if (millis() - lastLog > 5000) {
    lastLog = millis();
    if (gps.haveLoc)
      Serial.printf("fix q%u mode %u  %.6f, %.6f  alt %.1f m  sats %u  hdop %.1f\n",
                    gps.fixQ, gps.mode, gps.lat, gps.lon, gps.alt, gps.satsUsed, gps.hdop);
    else
      Serial.printf("no fix  bytes %lu  sentences %lu  bad %lu  in view %u\n",
                    (unsigned long)gps.rxBytes, (unsigned long)gps.sentences,
                    (unsigned long)gps.badChecksum, gps.nInView);
  }
}
