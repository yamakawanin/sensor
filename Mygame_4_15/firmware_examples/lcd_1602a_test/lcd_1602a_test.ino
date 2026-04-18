#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t FALLBACK_A = 0x27;
const uint8_t FALLBACK_B = 0x3F;

bool i2cAddressExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

uint8_t detectAddress() {
  if (i2cAddressExists(FALLBACK_A)) {
    return FALLBACK_A;
  }
  if (i2cAddressExists(FALLBACK_B)) {
    return FALLBACK_B;
  }
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    if (i2cAddressExists(addr)) {
      return addr;
    }
  }
  return 0;
}

void printScan() {
  Serial.println("[I2C] Scan start");
  bool found = false;
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    if (i2cAddressExists(addr)) {
      found = true;
      char buf[10];
      snprintf(buf, sizeof(buf), "0x%02X", addr);
      Serial.print("[I2C] Found ");
      Serial.println(buf);
    }
  }
  if (!found) {
    Serial.println("[I2C] No device found");
  }
  Serial.println("[I2C] Scan end");
}

LiquidCrystal_I2C lcdA(FALLBACK_A, 16, 2);
LiquidCrystal_I2C lcdB(FALLBACK_B, 16, 2);
LiquidCrystal_I2C lcdDyn(0x20, 16, 2);
LiquidCrystal_I2C* lcd = &lcdA;

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin();

  printScan();

  uint8_t addr = detectAddress();
  if (addr == FALLBACK_B) {
    lcd = &lcdB;
  } else if (addr != 0 && addr != FALLBACK_A) {
    lcdDyn = LiquidCrystal_I2C(addr, 16, 2);
    lcd = &lcdDyn;
  }

  lcd->init();
  lcd->backlight();
  lcd->clear();

  if (addr == 0) {
    lcd->setCursor(0, 0);
    lcd->print("No I2C device");
    Serial.println("[LCD] No I2C device");
    return;
  }

  lcd->setCursor(0, 0);
  lcd->print("1602A TEST OK");
  lcd->setCursor(0, 1);
  lcd->print("LiquidCrystal");

  char buf[10];
  snprintf(buf, sizeof(buf), "0x%02X", addr);
  Serial.print("[LCD] Address ");
  Serial.println(buf);
}

void loop() {
}
