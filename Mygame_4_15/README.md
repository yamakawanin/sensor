# Mygame - Arduino 体感控制小恐龙

这是一个基于 Python + Pygame 的离线版小恐龙游戏，支持 Arduino 传感器输入：
- 超声波传感器控制跳跃
- 光敏电阻控制昼夜模式

项目目录：

```text
app/
  game.py
firmware/
  firmware.ino
```

## 1. 功能概览

- 键盘控制
  - `Space` / `Up`：跳跃
  - `C`：手动切换昼夜（未连接 Arduino 时）
- Arduino 控制
  - 距离靠近触发跳跃
  - 环境变暗自动切夜间模式

## 2. 运行环境

- macOS / Windows / Linux
- Python 3.9+
- Arduino IDE
- 依赖包：`pygame`、`pyserial`

安装依赖：

```bash
python -m pip install pygame pyserial
```

## 3. 快速启动

### 3.1 烧录 Arduino 程序

1. 打开 `firmware/firmware.ino`
2. 选择板子与串口
3. 上传程序

### 3.2 启动游戏

在项目根目录执行：

```bash
python app/game.py
```

游戏会自动扫描串口并优先连接 Arduino；若未连接，仍可用键盘游玩。

## 4. Arduino 控制小恐龙跳跃方式

本项目采用“距离触发 + 防抖脉冲”的方式控制跳跃：

1. 超声波每 20ms 测一次距离
2. 距离做低通滤波，减少抖动
3. 使用双阈值判断手势状态
   - `JUMP_NEAR_CM`：进入近距离区，触发一次跳跃脉冲
   - `JUMP_FAR_CM`：回到远距离区，允许下一次触发
4. 使用 `JUMP_DEBOUNCE_MS` 防止连跳

Arduino 串口输出格式：

```text
distance_cm,light_value,jump
```

- `distance_cm`：平滑后的距离
- `light_value`：光敏电阻模拟值
- `jump`：0 或 1，`1` 表示本帧触发跳跃

Python 端兼容两种协议：
- 新协议：3 列（推荐）
- 旧协议：2 列 `distance_cm,light_value`（会按距离阈值兜底触发）

## 5. 传感器接线示例

以常见 Arduino UNO 为例：

- HC-SR04
  - `VCC` -> `5V`
  - `GND` -> `GND`
  - `TRIG` -> `D9`
  - `ECHO` -> `D10`
- 光敏电阻分压
  - 分压输出 -> `A0`

> 注意：如果你使用的是 3.3V 板子，请确认 ECHO 电平兼容，必要时加分压或电平转换。

## 6. 实验中不同硬件的作用

### 6.1 Arduino 开发板

- 负责采集传感器数据（超声波、光敏）
- 对距离做平滑、阈值判断与防抖
- 通过串口持续发送 `distance_cm,light_value,jump`

### 6.2 超声波传感器（HC-SR04）

- 负责体感跳跃输入
- 测量手与传感器距离，距离进入近阈值时触发一次跳跃脉冲

### 6.3 光敏电阻（含分压电路）

- 负责环境亮度输入
- 将亮度转换为模拟量给 Arduino，Python 端据此切换昼夜模式

### 6.4 USB 数据线（串口链路）

- 负责 Arduino 与电脑之间的实时数据传输
- 是体感控制链路中的通信桥梁

### 6.5 电脑（Python + Pygame）

- 负责游戏主循环、碰撞检测、计分、画面渲染
- 消费串口数据并映射为游戏行为：
  - `jump=1` 时触发跳跃
  - `light_value` 触发昼夜模式切换

### 6.6 面包板与导线（若使用）

- 负责电路连接与供电稳定
- 不直接参与计算逻辑，但影响实验稳定性与可复现性

### 6.7 系统分层视角

- 输入层：Arduino + 传感器（采集动作与环境）
- 传输层：USB 串口（传输控制信号）
- 决策与显示层：Python 游戏（判定并渲染结果）

## 7. 可调参数

### 7.1 Arduino 侧（`firmware/firmware.ino`）

- `JUMP_NEAR_CM`：靠近触发阈值，建议 15~25
- `JUMP_FAR_CM`：释放阈值，建议比 `JUMP_NEAR_CM` 大 6~12
- `JUMP_DEBOUNCE_MS`：跳跃防抖时间，建议 220~350ms

### 7.2 Python 侧（`app/game.py`）

- `DARK_ON_THRESHOLD` / `DARK_OFF_THRESHOLD`：夜间模式开关阈值
- `SPEED` / `ACCELERATION` / `MAX_SPEED`：游戏速度曲线

## 8. 常见问题

### 8.1 找不到串口

- 检查数据线是否支持数据传输
- 确认 Arduino IDE 中可以正常看到端口
- macOS 如使用 CH340/CP210 芯片，需确认驱动正常

### 8.2 体感跳跃太敏感或不灵敏

- 先调 `JUMP_NEAR_CM` 和 `JUMP_FAR_CM`
- 再调 `JUMP_DEBOUNCE_MS`
- 最后观察传感器摆放角度与手势距离

### 8.3 运行报缺少模块

重新安装依赖：

```bash
python -m pip install --upgrade pygame pyserial
```

## 9. 后续可扩展

- 加入蜂鸣器反馈（跳跃提示音）
- 加入按键输入，做“物理按键 + 体感”双模控制
- 在 Python 端加入串口可视化调参面板
