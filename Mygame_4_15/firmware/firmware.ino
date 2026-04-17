const int trigPin = 9;
const int echoPin = 10;
const int lightPin = A0;

const int JUMP_NEAR_CM = 20;
const int JUMP_FAR_CM = 28;
const unsigned long JUMP_DEBOUNCE_MS = 280;

float filteredDistance = 200.0;
bool handNear = false;
unsigned long lastJumpMs = 0;

void setup() {
  Serial.begin(115200); // 使用更高波特率减少延迟
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
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

  // 用近/远阈值 + 防抖做“抬手触发一次跳跃”
  int jump = 0;
  unsigned long now = millis();
  if (!handNear && smoothDistance <= JUMP_NEAR_CM) {
    handNear = true;
    if (now - lastJumpMs >= JUMP_DEBOUNCE_MS) {
      jump = 1;
      lastJumpMs = now;
    }
  } else if (handNear && smoothDistance >= JUMP_FAR_CM) {
    handNear = false;
  }

  // 发送数据：距离,光敏值,跳跃脉冲(0/1)
  Serial.print(smoothDistance);
  Serial.print(",");
  Serial.print(lightVal);
  Serial.print(",");
  Serial.println(jump);

  delay(20); // 约 50Hz 的采样率，保证游戏流畅度
}