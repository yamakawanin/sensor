#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>

// ------------------------------
// ESP32-C6 发射端：串口桥接 + ESP-NOW 发射
// ------------------------------
const int PIN_SERIAL1_RX = 18;  // 接 Arduino SoftwareSerial TX
const int PIN_SERIAL1_TX = 19;  // 接 Arduino SoftwareSerial RX
const uint32_t SERIAL1_BAUD = 9600;

// 为了先打通链路，默认使用广播地址测试。
// 验证通过后可切回接收端单播 MAC。
const bool USE_BROADCAST_TEST = true;
uint8_t PEER_MAC[6] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const uint8_t ESPNOW_CHANNEL = 1;

struct __attribute__((packed)) AlarmPacket {
  char header[6];      // "ALARM"
  uint8_t status;      // 0/1/2
  uint32_t seq;        // 递增序号
  uint8_t crc8;        // 简单校验
};

uint32_t packetSeq = 0;

String lineBuffer;

void printMacArray(const uint8_t* mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macStr);
}

uint8_t calcCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
  }
  return crc;
}

// ESP32 Arduino Core 3.x（IDF5）发送回调首参已从 MAC 地址改为 wifi_tx_info_t*
#if ESP_IDF_VERSION_MAJOR >= 5
void onDataSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
  (void)tx_info;
#else
void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  (void)mac_addr;
#endif

  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("[TX] ESP-NOW send ok");
  } else {
    Serial.println("[TX] ESP-NOW send fail");
  }
}

bool initEspNowPeer() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (USE_BROADCAST_TEST) {
    memcpy(PEER_MAC, BROADCAST_MAC, 6);
  }

  Serial.print("[TX] Local STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("[TX] Target MAC: ");
  printMacArray(PEER_MAC);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[TX] esp_now_init failed");
    return false;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, PEER_MAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[TX] esp_now_add_peer failed");
    return false;
  }

  Serial.println("[TX] ESP-NOW peer ready");
  return true;
}

void sendStatusPacket(uint8_t status) {
  AlarmPacket packet = {};
  memcpy(packet.header, "ALARM", 6);
  packet.status = status;
  packet.seq = packetSeq++;
  packet.crc8 = calcCrc8(reinterpret_cast<const uint8_t*>(&packet), sizeof(packet) - 1);

  esp_err_t ret = esp_now_send(PEER_MAC, reinterpret_cast<const uint8_t*>(&packet), sizeof(packet));
  if (ret != ESP_OK) {
    Serial.print("[TX] esp_now_send error: ");
    Serial.println((int)ret);
  } else {
    Serial.print("[TX] sent status=");
    Serial.print(status);
    Serial.print(", seq=");
    Serial.println(packet.seq);
  }
}

bool parseAlarmLine(const String& line, uint8_t& outStatus) {
  // 修复点：C++ 正确 API 是 startsWith（不是 startswith）
  if (!line.startsWith("ALARM,")) {
    return false;
  }

  int commaIndex = line.indexOf(',');
  if (commaIndex < 0 || commaIndex >= (int)line.length() - 1) {
    return false;
  }

  int status = line.substring(commaIndex + 1).toInt();
  if (status < 0 || status > 255) {
    return false;
  }

  outStatus = static_cast<uint8_t>(status);
  return true;
}

void pollSerial1NonBlocking() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (lineBuffer.length() > 0) {
        uint8_t parsedStatus = 0;
        if (parseAlarmLine(lineBuffer, parsedStatus)) {
          Serial.print("[TX] Parsed status=");
          Serial.println(parsedStatus);
          sendStatusPacket(parsedStatus);
        } else {
          Serial.print("[TX] Invalid frame: ");
          Serial.println(lineBuffer);
        }
        lineBuffer = "";
      }
      continue;
    }

    if (lineBuffer.length() < 63) {
      lineBuffer += c;
    } else {
      // 超长帧保护，防止脏数据长期占用缓冲
      lineBuffer = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial1.begin(SERIAL1_BAUD, SERIAL_8N1, PIN_SERIAL1_RX, PIN_SERIAL1_TX);
  Serial.println("[TX] Serial1 started, waiting Arduino frames...");

  if (!initEspNowPeer()) {
    Serial.println("[TX] init failed, restart device");
  }
}

void loop() {
  // 非阻塞监听串口帧，收到完整行后立即转发
  pollSerial1NonBlocking();
}
