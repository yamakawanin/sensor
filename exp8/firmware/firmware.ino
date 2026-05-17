#if defined(ARDUINO)
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#else
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

class HardwareSerial {
 public:
  void begin(unsigned long baud);
  void print(const char *s);
  void print(int v);
  void println(void);
  void println(const char *s);
  void println(int v);
};

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

class TwoWire {
 public:
  void begin() {}
  void setPins(int sda, int scl) {
    (void)sda;
    (void)scl;
  }
  void setClock(uint32_t hz) { (void)hz; }
  void beginTransmission(uint8_t addr) { (void)addr; }
  void write(uint8_t value) { (void)value; }
  uint8_t endTransmission() { return 0; }
};

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

extern HardwareSerial Serial;
extern WiFiClass WiFi;
extern TwoWire Wire;

void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
unsigned long millis(void);

#define WIFI_STA 1
#endif

#include <math.h>

const int I2C_SDA = 21;
const int I2C_SCL = 22;
const uint8_t LCD_ADDR_A = 0x27;
const uint8_t LCD_ADDR_B = 0x3F;
const unsigned long SCAN_INTERVAL_MS = 2000;

constexpr int NUM_APS = 3;
const char *AP_LIST[NUM_APS] = {"yamakawa", "pura", "OPPO"};

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

LiquidCrystal_I2C lcdA(LCD_ADDR_A, 16, 2);
LiquidCrystal_I2C lcdB(LCD_ADDR_B, 16, 2);
LiquidCrystal_I2C lcdDyn(0x20, 16, 2);
LiquidCrystal_I2C *lcd = &lcdA;
uint8_t lcdAddr = LCD_ADDR_A;
bool lcdReady = false;
unsigned long lastScanMs = 0;

bool i2cAddressExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

uint8_t detectLcdAddress() {
  if (i2cAddressExists(LCD_ADDR_A)) {
    return LCD_ADDR_A;
  }
  if (i2cAddressExists(LCD_ADDR_B)) {
    return LCD_ADDR_B;
  }
  return 0;
}

void lcdClear() {
  lcd->clear();
}

void lcdSetCursor(uint8_t col, uint8_t row) {
  lcd->setCursor(col, row);
}

void lcdPrint(const char *text) {
  lcd->print(text);
}

void lcdPrintLine(uint8_t row, const char *text) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", text);
  buf[16] = '\0';
  lcdSetCursor(0, row);
  lcdPrint(buf);
}

void lcdInit() {
  lcd->init();
  delay(50);
  lcd->backlight();
  delay(50);
  lcd->clear();
  delay(2);
}

void setupLcd() {
  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();
  Wire.setClock(50000);

  lcdAddr = detectLcdAddress();
  if (lcdAddr == 0) {
    lcdReady = false;
    Serial.println("[LCD] No usable I2C LCD");
    return;
  }

  if (lcdAddr == LCD_ADDR_B) {
    lcd = &lcdB;
  } else if (lcdAddr != LCD_ADDR_A) {
    lcdDyn = LiquidCrystal_I2C(lcdAddr, 16, 2);
    lcd = &lcdDyn;
  } else {
    lcd = &lcdA;
  }

  lcdInit();
  lcdClear();
  lcdPrintLine(0, "WiFi Locate");
  lcdPrintLine(1, "Scanning...");
  lcdReady = true;

  char addrHex[7];
  snprintf(addrHex, sizeof(addrHex), "0x%02X", (unsigned int)lcdAddr);
  Serial.print("[LCD] Connected at ");
  Serial.println(addrHex);
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
      if (ssid == AP_LIST[j] && rssi > outRssi[j]) {
        outRssi[j] = rssi;
      }
    }
  }
  WiFi.scanDelete();
}

float distanceTo(const int *scan, const Fingerprint &fp) {
  float sum = 0.0f;
  for (int i = 0; i < NUM_APS; i++) {
    float diff = (float)scan[i] - (float)fp.rssi[i];
    sum += diff * diff;
  }
  return sqrtf(sum);
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
        float tempDist = dist[i];
        dist[i] = dist[j];
        dist[j] = tempDist;
        int tempIdx = idx[i];
        idx[i] = idx[j];
        idx[j] = tempIdx;
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
    float dist2 = dx * dx + dy * dy;
    if (dist2 < bestDist) {
      bestDist = dist2;
      bestIdx = i;
    }
  }
  return bestIdx;
}

void renderLocation(const int *scan, int nnIdx, int wknnIdx) {
  if (!lcdReady) {
    return;
  }

  (void)nnIdx;
  (void)wknnIdx;
  char line0[17];
  char line1[17];
  snprintf(line0, sizeof(line0), "R1:%d", scan[0]);
  snprintf(line1, sizeof(line1), "R2:%d R3:%d", scan[1], scan[2]);
  lcdPrintLine(0, line0);
  lcdPrintLine(1, line1);
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

  renderLocation(scan, nnIdx, wknnIdx);
  printRssiVector(scan);
}
