#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ------------------------------
// ESP32-C6 接收执行端：ESP-NOW 回调 + LCD + 舵机
// ------------------------------
const int PIN_I2C_SDA = 6;
const int PIN_I2C_SCL = 7;
const int PIN_SERVO = 4;

// 使用 ESP32 原生 LEDC 生成舵机 PWM，避免依赖 ESP32Servo 外部库
const int SERVO_PWM_FREQ = 50;
const int SERVO_PWM_RES_BITS = 14;

const uint8_t LCD_ADDR_A = 0x27;
const uint8_t LCD_ADDR_B = 0x3F;

const int SERVO_OPEN_ANGLE = 20;    // 安全/警戒时开门
const int SERVO_LOCK_ANGLE = 100;   // 报警时闭锁

struct __attribute__((packed)) AlarmPacket {
  char header[6];
  uint8_t status;
  uint32_t seq;
  uint8_t crc8;
};

LiquidCrystal_I2C lcdA(LCD_ADDR_A, 16, 2);
LiquidCrystal_I2C lcdB(LCD_ADDR_B, 16, 2);
LiquidCrystal_I2C* lcd = &lcdA;

volatile bool hasNewPacket = false;
volatile uint8_t latestStatus = 0;
volatile uint32_t latestSeq = 0;

int lastStatus = -1;
uint32_t lastSeq = 0;

uint32_t usToDutyTicks(int pulseUs) {
  const uint32_t maxDuty = (1UL << SERVO_PWM_RES_BITS) - 1;
  const uint32_t periodUs = 1000000UL / SERVO_PWM_FREQ;
  return (uint32_t)((uint64_t)pulseUs * maxDuty / periodUs);
}

void servoWriteAngle(int angle) {
  if (angle < 0) {
    angle = 0;
  }
  if (angle > 180) {
    angle = 180;
  }

  // 0~180 映射到 500~2400us（常见舵机范围）
  int pulseUs = 500 + ((2400 - 500) * angle) / 180;
  ledcWrite(PIN_SERVO, usToDutyTicks(pulseUs));
}

uint8_t calcCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
  }
  return crc;
}

bool i2cAddressExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void initLcd() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  if (i2cAddressExists(LCD_ADDR_B)) {
    lcd = &lcdB;
  } else {
    lcd = &lcdA;
  }

  lcd->init();
  lcd->backlight();
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Warehouse Guard");
  lcd->setCursor(0, 1);
  lcd->print("Waiting data...");
}

void renderStatus(int status, uint32_t seq) {
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print("Status:");

  if (status == 0) {
    lcd->print("SAFE ");
  } else if (status == 1) {
    lcd->print("WARN ");
  } else {
    lcd->print("ALARM");
  }

  char line2[17];
  snprintf(line2, sizeof(line2), "Seq:%lu", (unsigned long)seq);
  lcd->setCursor(0, 1);
  lcd->print(line2);
}

void applyActuatorByStatus(int status) {
  if (status >= 2) {
    servoWriteAngle(SERVO_LOCK_ANGLE);
  } else {
    servoWriteAngle(SERVO_OPEN_ANGLE);
  }
}

bool initEspNowRecv() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("[RX] Local STA MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[RX] esp_now_init failed");
    return false;
  }

  // 修复点：使用新版回调签名（esp_now_recv_info_t）
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("[RX] ESP-NOW recv callback registered");
  return true;
}

// 新版 ESP32 Arduino Core / ESP-IDF 回调签名
void OnDataRecv(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len) {
  (void)info;
  if (len != (int)sizeof(AlarmPacket)) {
    return;
  }

  AlarmPacket packet = {};
  memcpy(&packet, incomingData, sizeof(packet));

  if (strncmp(packet.header, "ALARM", 5) != 0) {
    return;
  }

  uint8_t expectedCrc = calcCrc8(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet) - 1);
  if (expectedCrc != packet.crc8) {
    return;
  }

  latestStatus = packet.status;
  latestSeq = packet.seq;
  hasNewPacket = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  initLcd();
  Serial.println("[RX] LCD ready, showing Waiting data...");

  ledcAttach(PIN_SERVO, SERVO_PWM_FREQ, SERVO_PWM_RES_BITS);
  servoWriteAngle(SERVO_OPEN_ANGLE);

  if (!initEspNowRecv()) {
    Serial.println("[RX] init failed, restart device");
  }
}

void loop() {
  if (!hasNewPacket) {
    return;
  }

  noInterrupts();
  uint8_t currentStatus = latestStatus;
  uint32_t currentSeq = latestSeq;
  hasNewPacket = false;
  interrupts();

  // 核心保留：状态切换锁，避免 LCD 狂闪与舵机重复驱动
  if ((int)currentStatus != lastStatus) {
    renderStatus((int)currentStatus, currentSeq);
    applyActuatorByStatus((int)currentStatus);

    Serial.print("[RX] status change: ");
    Serial.print(lastStatus);
    Serial.print(" -> ");
    Serial.println((int)currentStatus);

    lastStatus = (int)currentStatus;
  }

  // 记录序号可用于排查丢包和乱序，不触发硬件重复动作
  lastSeq = currentSeq;
  Serial.print("[RX] packet seq=");
  Serial.println((unsigned long)lastSeq);
}
