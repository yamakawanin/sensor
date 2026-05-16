#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

class HardwareSerial {
 public:
  void begin(unsigned long baud);
  void print(const char *s);
  void print(int v);
  void print(float v);
  void println(const char *s);
  void println(int v);
  void println(float v);
};

extern HardwareSerial Serial;

void delay(unsigned long ms);
unsigned long millis(void);
#endif

#if __has_include(<WiFi.h>)
#include <WiFi.h>
#define HAS_WIFI 1
#else
#define HAS_WIFI 0
class String {
 public:
  String() {}
  String(const char *s) { (void)s; }
  bool operator==(const char *s) const {
    (void)s;
    return false;
  }
};

class WiFiClass {
 public:
  int scanNetworks(bool async = false, bool show_hidden = false) {
    (void)async;
    (void)show_hidden;
    return 0;
  }
  String SSID(int i) {
    (void)i;
    return String();
  }
  int RSSI(int i) {
    (void)i;
    return -100;
  }
  void scanDelete() {}
  void mode(int m) { (void)m; }
  void disconnect(bool w = true) { (void)w; }
};

extern WiFiClass WiFi;
#define WIFI_STA 1
#endif
#include <math.h>

#if __has_include(<Wire.h>)
#include <Wire.h>
#define HAS_WIRE 1
#else
#define HAS_WIRE 0
#endif

#if __has_include(<LiquidCrystal_I2C.h>)
#include <LiquidCrystal_I2C.h>
#else
class LiquidCrystal_I2C {
 public:
  LiquidCrystal_I2C(unsigned char addr, unsigned char cols, unsigned char rows) {
    (void)addr;
    (void)cols;
    (void)rows;
  }
  void init() {}
  void backlight() {}
  void clear() {}
  void setCursor(unsigned char col, unsigned char row) {
    (void)col;
    (void)row;
  }
  void print(const char *s) { (void)s; }
};
#endif

// Adjust these pins to match your ESP32-C6 board.
const int I2C_SDA = 6;
const int I2C_SCL = 7;
const uint8_t LCD_ADDR = 0x27;

constexpr int NUM_APS = 3;
const char *AP_LIST[NUM_APS] = {"AP_1", "AP_2", "AP_3"};

struct Fingerprint {
  const char *name;
  int rssi[NUM_APS];
  float x;
  float y;
};

const Fingerprint FINGERPRINTS[] = {
    {"A1", {-45, -60, -80}, 0.0f, 0.0f},
    {"A2", {-60, -45, -75}, 1.0f, 0.0f},
    {"A3", {-75, -65, -50}, 0.0f, 1.0f},
};
constexpr int NUM_FP = sizeof(FINGERPRINTS) / sizeof(FINGERPRINTS[0]);

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

const unsigned long SCAN_INTERVAL_MS = 2000;
unsigned long lastScanMs = 0;

void setupLcd() {
#if HAS_WIRE
  Wire.begin(I2C_SDA, I2C_SCL);
#endif
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Locate");
  lcd.setCursor(0, 1);
  lcd.print("Scanning...");
}

void printLine(int row, const char *label, const char *value) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%s:%-12s", label, value);
  buf[16] = '\0';
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void scanRssi(int *outRssi) {
  for (int i = 0; i < NUM_APS; i++) {
    outRssi[i] = -100;
  }

  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    for (int j = 0; j < NUM_APS; j++) {
      if (ssid == AP_LIST[j]) {
        if (rssi > outRssi[j]) {
          outRssi[j] = rssi;
        }
      }
    }
  }
  WiFi.scanDelete();
}

float distanceTo(const int *scan, const Fingerprint &fp) {
  float acc = 0.0f;
  for (int i = 0; i < NUM_APS; i++) {
    float diff = (float)scan[i] - (float)fp.rssi[i];
    acc += diff * diff;
  }
  return sqrtf(acc);
}

int locateNN(const int *scan) {
  int bestIdx = 0;
  float bestDist = 1e9f;
  for (int i = 0; i < NUM_FP; i++) {
    float dist = distanceTo(scan, FINGERPRINTS[i]);
    if (dist < bestDist) {
      bestDist = dist;
      bestIdx = i;
    }
  }
  return bestIdx;
}

int locateWKNN(const int *scan, int k) {
  if (k > NUM_FP) {
    k = NUM_FP;
  }
  int idx[NUM_FP];
  float dist[NUM_FP];
  for (int i = 0; i < NUM_FP; i++) {
    idx[i] = i;
    dist[i] = distanceTo(scan, FINGERPRINTS[i]);
  }

  for (int i = 0; i < NUM_FP - 1; i++) {
    for (int j = i + 1; j < NUM_FP; j++) {
      if (dist[j] < dist[i]) {
        float td = dist[i];
        dist[i] = dist[j];
        dist[j] = td;
        int ti = idx[i];
        idx[i] = idx[j];
        idx[j] = ti;
      }
    }
  }

  float sumW = 0.0f;
  float wx = 0.0f;
  float wy = 0.0f;
  for (int i = 0; i < k; i++) {
    float w = 1.0f / (dist[i] + 1e-3f);
    sumW += w;
    wx += w * FINGERPRINTS[idx[i]].x;
    wy += w * FINGERPRINTS[idx[i]].y;
  }
  float x = wx / sumW;
  float y = wy / sumW;

  int bestIdx = 0;
  float bestDist = 1e9f;
  for (int i = 0; i < NUM_FP; i++) {
    float dx = x - FINGERPRINTS[i].x;
    float dy = y - FINGERPRINTS[i].y;
    float d = dx * dx + dy * dy;
    if (d < bestDist) {
      bestDist = d;
      bestIdx = i;
    }
  }
  return bestIdx;
}

void printRssiVector(const int *scan) {
  Serial.print("RSSI:");
  for (int i = 0; i < NUM_APS; i++) {
    if (i > 0) {
      Serial.print(",");
    }
    Serial.print(scan[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  setupLcd();
}

void loop() {
  unsigned long now = millis();
  if (now - lastScanMs < SCAN_INTERVAL_MS) {
    delay(10);
    return;
  }
  lastScanMs = now;

  int scan[NUM_APS];
  scanRssi(scan);

  int nnIdx = locateNN(scan);
  int wknnIdx = locateWKNN(scan, 3);

  printLine(0, "NN", FINGERPRINTS[nnIdx].name);
  printLine(1, "WK", FINGERPRINTS[wknnIdx].name);

  printRssiVector(scan);
}
