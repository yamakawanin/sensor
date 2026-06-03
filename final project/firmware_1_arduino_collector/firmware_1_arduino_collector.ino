#include <Arduino.h>
#include <SoftwareSerial.h>

// ------------------------------
// 硬件引脚定义（Arduino 采集端）
// ------------------------------
const int PIN_LDR = A0;      // 光敏电阻分压输出
const int PIN_NTC = A1;      // NTC 分压输出
const int PIN_SOFT_RX = 2;   // 软串口 RX（接 ESP32-C6 TX1）
const int PIN_SOFT_TX = 3;   // 软串口 TX（接 ESP32-C6 RX1，需电平保护）

SoftwareSerial gatewaySerial(PIN_SOFT_RX, PIN_SOFT_TX);

// ------------------------------
// 阈值参数（根据现场标定）
// ------------------------------
const int LDR_DARK_THRESHOLD = 350;      // 越小越暗
const int TEMP_WARN_THRESHOLD_C = 30;    // 高温警戒阈值
const int TEMP_ALARM_THRESHOLD_C = 30;   // 高温报警阈值

// NTC 简化换算参数（10k NTC + 10k 分压，B=3950）
const float NTC_BETA = 3950.0f;
const float NTC_R0 = 10000.0f;
const float T0_KELVIN = 298.15f;  // 25C
const float SERIES_RESISTOR = 10000.0f;
const float ADC_MAX = 1023.0f;    // UNO 10bit ADC
const float VREF = 5.0f;

unsigned long lastFrameMs = 0;
const unsigned long FRAME_INTERVAL_MS = 200;

int lastStatus = -1;

float ntcAdcToCelsius(int adc) {
  if (adc <= 0) {
    adc = 1;
  }
  if (adc >= (int)ADC_MAX) {
    adc = (int)ADC_MAX - 1;
  }

  float vOut = (adc / ADC_MAX) * VREF;
  float rNtc = (vOut * SERIES_RESISTOR) / (VREF - vOut);
  float invT = (1.0f / T0_KELVIN) + (1.0f / NTC_BETA) * log(rNtc / NTC_R0);
  float tKelvin = 1.0f / invT;
  return tKelvin - 273.15f;
}

int decideStatus(int ldrAdc, float tempC) {
  // 约定：0=SAFE, 1=WARN, 2=ALARM
  bool isDark = (ldrAdc < LDR_DARK_THRESHOLD);
  bool isTempAlarm = (tempC >= TEMP_ALARM_THRESHOLD_C);

  // 温度达到报警阈值时，无论光照如何都直接进入 ALARM。
  // 这样接收端会立刻执行舵机闭锁动作。
  if (isTempAlarm) {
    return 2;
  }
  if (isDark || tempC >= TEMP_WARN_THRESHOLD_C) {
    return 1;
  }
  return 0;
}

void sendAlarmFrame(int status) {
  // 维持既有协议：ALARM,status\n
  gatewaySerial.print("ALARM,");
  gatewaySerial.println(status);

  // USB 调试口打印，便于联调采样与状态判定
  Serial.print("LDR/NTC -> frame: ALARM,");
  Serial.println(status);
}

void setup() {
  Serial.begin(115200);
  gatewaySerial.begin(9600);

  pinMode(PIN_LDR, INPUT);
  pinMode(PIN_NTC, INPUT);

  Serial.println("[Arduino] Collector started");
}

void loop() {
  // 非阻塞定时发送，避免 delay 阻塞后续采样
  unsigned long now = millis();
  if (now - lastFrameMs < FRAME_INTERVAL_MS) {
    return;
  }
  lastFrameMs = now;

  int ldrAdc = analogRead(PIN_LDR);
  int ntcAdc = analogRead(PIN_NTC);
  float tempC = ntcAdcToCelsius(ntcAdc);

  int currentStatus = decideStatus(ldrAdc, tempC);

  // 采集端可持续上报帧，TX 端会做解析与无线发送
  sendAlarmFrame(currentStatus);

  // USB 串口输出诊断数据
  if (currentStatus != lastStatus) {
    Serial.print("[Arduino] status change: ");
    Serial.print(lastStatus);
    Serial.print(" -> ");
    Serial.println(currentStatus);
    lastStatus = currentStatus;
  }

  Serial.print("[Arduino] LDR=");
  Serial.print(ldrAdc);
  Serial.print(", TEMP=");
  Serial.print(tempC, 1);
  Serial.println("C");
}
