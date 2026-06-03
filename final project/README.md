# 分布式智能仓库安防与自动化控制系统

本项目实现三端链路协同：

- Arduino 采集端：采集光敏电阻与 NTC 温度传感器，输出 `ALARM,status` 串口帧
- ESP32-C6 发射端：通过 `Serial1` 非阻塞接收 Arduino 帧，解析后通过 ESP-NOW 广播到接收端
- ESP32-C6 接收端：异步回调接收 ESP-NOW 数据，驱动 1602A I2C LCD 与舵机执行机构

## 1. 系统设计思路与整体架构

### 1.1 数据流拓扑

`LDR + NTC -> Arduino(SoftwareSerial协议帧) -> ESP32-C6 TX(Serial1解析 + ESP-NOW发送) -> ESP32-C6 RX(回调接收) -> LCD1602A + Servo`

### 1.2 协议层拆分

- 有线层（Arduino -> TX 网关）：文本帧 `ALARM,status\n`
- 无线层（TX 网关 -> RX 执行端）：结构体帧 `{header, status, seq, crc8}`

设计要点：

- Arduino 只负责采集和状态判定，不感知 ESP-NOW
- TX 网关只做串口到无线的桥接和协议转换
- RX 端只做执行逻辑，不再解析 Arduino 串口

这避免了“接收端混合串口解析 + 无线接收”的架构错位问题。

### 1.3 非阻塞串口设计的现实物理意义

TX 端在 `loop()` 中按字节读取 `Serial1.available()`，每次只处理可用字节，不使用 `readStringUntil()` 这类阻塞 API。

好处：

- 降低丢帧风险：主循环不被串口等待卡住，ESP-NOW 发包时机更稳定
- 避免控制延迟：危险状态更快传递到执行端
- 减少系统抖动：不会因长时间阻塞导致 LCD/舵机刷新节奏异常

### 1.4 状态切换锁机制的现实物理意义

RX 端保留并强化以下逻辑：

`if (currentStatus != lastStatus) { ...执行硬件动作... }`

好处：

- LCD 防闪烁：状态未变化时不重复 `clear/print`
- 舵机防抖与降温：避免重复写同角度造成 PWM 持续激励和发热
- 总线减负：减少无意义 I2C 与控制信号写入，降低时序冲突概率

## 2. 硬件物理接线指南（极详版）

## 2.1 表1：采集发射总成（Arduino + ESP32-C6 TX + 传感器）

| 模块 | 信号 | 连接到 | 说明 |
|---|---|---|---|
| 光敏电阻分压中点 | `LDR_OUT` | Arduino `A0` | 典型 10k 分压，`0~5V` 模拟输入 |
| NTC 分压中点 | `NTC_OUT` | Arduino `A1` | 建议 10k NTC + 10k 基准电阻 |
| Arduino SoftwareSerial TX | `ARD_TX_SOFT` | ESP32-C6 `RX1(GPIO18)` | 必须做 5V -> 3.3V 电平保护 |
| Arduino SoftwareSerial RX | `ARD_RX_SOFT` | ESP32-C6 `TX1(GPIO19)` | 3.3V 高电平通常可被 Arduino 识别 |
| Arduino `GND` | `GND` | ESP32-C6 `GND` | 必须共地 |
| Arduino `5V` | `5V` | 传感器供电（可选） | 仅供 5V 传感器，勿直连 ESP32 3.3V 引脚 |
| ESP32-C6 `3V3` | `3.3V` | 低压传感器/电平转换器 | 与 Arduino 供电域隔离设计更安全 |

电平保护建议（Arduino TX -> ESP32 RX）：

- 推荐方案 1：双向电平转换模块（如 BSS138）
- 推荐方案 2：电阻分压（例如 10k 上拉 + 20k 下拉）将 5V 降到约 3.3V
- 禁止：Arduino 5V TX 直接硬连 ESP32-C6 RX 引脚

## 2.2 表2：接收执行端（ESP32-C6 RX + 1602A + 舵机）

| 模块 | 信号 | 连接到 | 说明 |
|---|---|---|---|
| LCD1602A(I2C) | `SDA` | ESP32-C6 `GPIO6` | 如开发板引脚复用不同，请按实际改代码常量 |
| LCD1602A(I2C) | `SCL` | ESP32-C6 `GPIO7` | I2C 地址常见 `0x27` / `0x3F` |
| LCD1602A(I2C) | `VCC` | `5V` 或模块要求电压 | 依据背板规格 |
| LCD1602A(I2C) | `GND` | ESP32-C6 `GND` | 必须共地 |
| 舵机 | `SIG` | ESP32-C6 `GPIO4` | 通过 ESP32Servo 输出 PWM |
| 舵机 | `V+` | 外部 `5V` 独立电源 | 禁止由开发板 3.3V 直接供舵机 |
| 舵机 | `GND` | 外部电源 `GND` + ESP32 `GND` | 必须与 ESP32 共地 |

## 2.3 关键安全警告

1. 共地规则（必须）：
所有节点必须共享参考地，否则串口电平与 PWM 基准漂移，轻则通信错误，重则损坏 IO。

2. 舵机外部供电（必须）：
舵机是感性大电流负载，堵转电流可远高于开发板稳压能力。
请使用独立 5V 电源（建议 >=1A），并将其 GND 与 ESP32 GND 共地。

3. 抗干扰建议（强烈推荐）：

- 舵机电源端并联 470uF~1000uF 电解电容
- 舵机线与 I2C 线分开走线，减少串扰
- 供电线尽量短且粗，避免电压跌落导致重启

## 3. 软件构建与烧录说明

## 3.1 目录结构

- `firmware_1_arduino_collector/firmware_1_arduino_collector.ino`
- `firmware_2_esp32c6_tx_gateway/firmware_2_esp32c6_tx_gateway.ino`
- `firmware_3_esp32c6_rx_executor/firmware_3_esp32c6_rx_executor.ino`

## 3.2 编译环境建议

- Arduino IDE 2.x
- ESP32 Arduino Core 3.x（对应新版 ESP-IDF，支持 `esp_now_recv_info_t` 回调签名）
- 依赖库：
	- `esp_now`（随 ESP32 Core）
	- `WiFi`（随 ESP32 Core）
	- `LiquidCrystal_I2C`
	- `ESP32Servo`

## 3.3 联调步骤

1. 先单独验证 Arduino 串口输出是否稳定为 `ALARM,status`
2. 烧录 TX 网关，打开串口监视器检查是否出现 `Parsed status` 与 `ESP-NOW send ok`
3. 烧录 RX 执行端，观察 LCD 首屏状态与舵机初始角度
4. 模拟光照/温度变化，确认状态切换时 LCD 与舵机只在变更瞬间动作

## 4. 状态码约定

| 状态码 | 语义 | 执行动作 |
|---|---|---|
| 0 | 安全 | LCD 显示 SAFE，舵机开门角度 |
| 1 | 警戒 | LCD 显示 WARN，舵机保持开门 |
| 2 | 报警 | LCD 显示 ALARM，舵机闭锁角度 |

你可以在三端代码中统一扩展状态语义，但必须保持上述基础兼容。
