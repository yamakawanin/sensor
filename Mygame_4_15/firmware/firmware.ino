#if __has_include(<Arduino.h>)
#include <Arduino.h>
#else
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Minimal declarations for editor IntelliSense when Arduino headers are unavailable.
#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define A0 14

class HardwareSerial {
 public:
  void begin(unsigned long baud);
  int available(void);
  int read(void);
  void print(const char *s);
  void print(int v);
  void print(long v);
  void print(unsigned long v);
  void print(float v);
  void println(const char *s);
  void println(int v);
  void println(long v);
  void println(unsigned long v);
  void println(float v);
};

extern HardwareSerial Serial;

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
void delayMicroseconds(unsigned int us);
long pulseIn(int pin, int state, unsigned long timeout = 0UL);
int analogRead(int pin);
unsigned long millis(void);
void delay(unsigned long ms);
#endif

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
  LiquidCrystal_I2C(unsigned char addr, int cols, int rows) {}
  void init() {}
  void backlight() {}
  void clear() {}
  void setCursor(int col, int row) {}
  void print(const char *s) {}
  void print(int v) {}
};
#endif

const int lcdAddrA = 0x27;
const int lcdAddrB = 0x3F;
LiquidCrystal_I2C lcdA(lcdAddrA, 16, 2);
LiquidCrystal_I2C lcdB(lcdAddrB, 16, 2);
LiquidCrystal_I2C *lcd = &lcdA;
bool lcdReady = false;

bool i2cAddressExists(unsigned char addr) {
#if HAS_WIRE
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
#else
  (void)addr;
  return false;
#endif
}

unsigned char detectLcdAddress() {
  if (i2cAddressExists((unsigned char)lcdAddrA)) {
    return (unsigned char)lcdAddrA;
  }
  if (i2cAddressExists((unsigned char)lcdAddrB)) {
    return (unsigned char)lcdAddrB;
  }
  return 0;
}

void setupLcd() {
#if HAS_WIRE
  Wire.begin();
#endif

  unsigned char addr = detectLcdAddress();
  if (addr == (unsigned char)lcdAddrB) {
    lcd = &lcdB;
  } else {
    lcd = &lcdA;
  }

  lcd->init();
  lcd->backlight();
  lcd->clear();
  lcd->setCursor(0, 0);

  if (addr == 0) {
    lcd->print("LCD? 0x27/0x3F");
    Serial.println("[LCD] No I2C device at 0x27 or 0x3F");
    lcdReady = false;
    return;
  }

  lcd->print("Dino Sensor");
  Serial.print("[LCD] Connected at 0x");
  Serial.println(addr, HEX);
  lcdReady = true;
}

char scoreCmdBuf[16];
int scoreCmdLen = 0;
int currentScore = 0;

void renderScore(int score) {
  if (!lcdReady) {
    return;
  }

  if (score < 0) {
    score = 0;
  }
  if (score > 99999) {
    score = 99999;
  }

  char line[17];
  snprintf(line, sizeof(line), "Score:%-10d", score);
  lcd->setCursor(0, 1);
  lcd->print(line);
}

void applyScoreCommand() {
  if (scoreCmdLen < 2 || scoreCmdBuf[0] != 'S') {
    return;
  }

  int score = atoi(scoreCmdBuf + 1);
  if (score != currentScore) {
    currentScore = score;
    renderScore(currentScore);
  }
}

void readScoreFromSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (scoreCmdLen > 0) {
        scoreCmdBuf[scoreCmdLen] = '\0';
        applyScoreCommand();
        scoreCmdLen = 0;
      }
      continue;
    }

    if (scoreCmdLen < (int)sizeof(scoreCmdBuf) - 1) {
      if (scoreCmdLen == 0) {
        if (c == 'S') {
          scoreCmdBuf[scoreCmdLen++] = c;
        }
      } else if (c >= '0' && c <= '9') {
        scoreCmdBuf[scoreCmdLen++] = c;
      } else {
        scoreCmdLen = 0;
      }
    } else {
      scoreCmdLen = 0;
    }
  }
}

const int trigPin = 9;
const int echoPin = 10;
const int lightPin = A0;

const int JUMP_OBSTACLE_NEAR_CM = 5;
const int JUMP_OBSTACLE_FAR_CM = 10;
const unsigned long JUMP_DEBOUNCE_MS = 280;
const int JUMP_CONFIRM_SAMPLES = 2;

float filteredDistance = 200.0;
bool obstacleNear = false;
int nearSampleCount = 0;
unsigned long lastJumpMs = 0;

void setup() {
  Serial.begin(115200); // 使用更高波特率减少延迟
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  setupLcd();
  renderScore(0);
}

void loop() {
  // 读取来自 Python 的分数命令：S123\n
  readScoreFromSerial();

  // 超声波测距
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 25000);
  int distance = duration > 0 ? (int)(duration * 0.034 / 2.0) : 300;

  // 一阶低通滤波，减少抖动
  filteredDistance = 0.65 * filteredDistance + 0.35 * distance;
  int smoothDistance = (int)filteredDistance;

  // 读取光敏电阻
  int lightVal = analogRead(lightPin);

  // 用“障碍物近/远阈值 + 连续确认 + 防抖”触发跳跃
  int jump = 0;
  unsigned long now = millis();
  if (!obstacleNear) {
    if (smoothDistance <= JUMP_OBSTACLE_NEAR_CM) {
      nearSampleCount++;
      if (nearSampleCount >= JUMP_CONFIRM_SAMPLES && now - lastJumpMs >= JUMP_DEBOUNCE_MS) {
        jump = 1;
        obstacleNear = true;
        lastJumpMs = now;
        nearSampleCount = 0;
      }
    } else {
      nearSampleCount = 0;
    }
  } else if (smoothDistance >= JUMP_OBSTACLE_FAR_CM) {
    obstacleNear = false;
  }

  // 发送数据：距离,光敏值,跳跃脉冲(0/1)
  Serial.print(smoothDistance);
  Serial.print(",");
  Serial.print(lightVal);
  Serial.print(",");
  Serial.println(jump);

  delay(20); // 约 50Hz 的采样率，保证游戏流畅度
}