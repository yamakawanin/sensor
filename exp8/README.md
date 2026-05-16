# 实验8：基于 WiFi 的定位（ESP32-C6）

## 1. 实验目标
- 理解基于 WiFi 指纹的定位流程（离线训练 + 在线定位）。
- 能在 ESP32-C6 上完成 RSSI 扫描与指纹匹配。
- 使用至少两种定位算法（NN 与 WKNN）。
- 在 LCD 上实时显示定位结果。

## 2. 实验器材
- ESP32-C6 开发板
- 2.4GHz WiFi 热点（手机或路由器，需开启 2.4GHz）
- I2C 1602A LCD 模块（常见地址 0x27/0x3F）
- 杜邦线、电源线

## 2.1 热点如何开启（手机）
> 只要保证是 2.4GHz 热点即可，SSID 名称必须与 `AP_LIST` 完全一致。

### iPhone（iOS）
1. 设置 -> 个人热点 -> 允许其他人加入。
2. 记下热点名称（与手机名称一致）和密码。
3. iPhone 默认是 2.4GHz，可直接使用。
4. 若有 “最大兼容性” 选项，请打开。

### Android（以常见系统为例）
1. 设置 -> 网络和互联网 -> 热点与网络共享 -> WLAN 热点。
2. 开启热点，进入 “热点设置/配置”。
3. 频段选择 “2.4GHz”。
4. 记下 SSID 与密码，确保与 `AP_LIST` 一致。

### 便携路由器/校园路由
1. 登录路由器管理页面。
2. 确保 2.4GHz SSID 已开启且可用。
3. 关闭 5GHz 同名 SSID，避免 ESP32 扫描混淆。

## 3. 接线说明（ESP32-C6 + I2C LCD）
> 不同 ESP32-C6 板子的 I2C 引脚可能不同，请以开发板丝印或资料为准，代码默认 SDA=GPIO6、SCL=GPIO7，可在固件中修改。

- LCD VCC -> ESP32-C6 3.3V
- LCD GND -> ESP32-C6 GND
- LCD SDA -> ESP32-C6 SDA（默认 GPIO6）
- LCD SCL -> ESP32-C6 SCL（默认 GPIO7）

## 4. 项目结构
```
exp8/
  firmware/firmware.ino
  tools/fingerprint_builder.py
  README.md
```

## 5. 实验步骤（操作流程）
### 5.1 离线训练（建立指纹库）
1. 选取目标区域的多个参考点（至少 3 个），并给每个点取短名称（如 A1、A2、A3）。
2. 保证 2.4GHz 热点稳定开启（至少 2 个以上 AP）。
3. 在 [exp8/firmware/firmware.ino](exp8/firmware/firmware.ino) 修改 `AP_LIST`，让其与热点 SSID 完全一致。
4. 用 Arduino IDE 选择 ESP32-C6 开发板并烧录固件。
5. 打开串口监视器（115200 波特率）。
6. 将 ESP32 放到某个参考点，连续记录多次串口输出的 `RSSI:` 行。
7. 将采集的数据整理成 CSV 后，用 `tools/fingerprint_builder.py` 计算各点均值（或手工平均也可）。

CSV 示例（samples.csv）：
```
location,rssi1,rssi2,rssi3
A1,-48,-62,-79
A1,-50,-60,-80
A2,-62,-46,-76
A3,-78,-66,-52
```

运行（在 exp8 目录下）：
```
python tools/fingerprint_builder.py samples.csv
```

### 5.2 在线定位（ESP32 上运行）
1. 将均值指纹写入 `firmware/firmware.ino` 的 `FINGERPRINTS` 数组。
2. 再次确认 `AP_LIST` 与真实 SSID 完全一致。
3. 烧录固件到 ESP32-C6。
4. 将设备放到不同位置，观察 LCD 显示的定位结果。

## 5.3 如何运行（从零开始）
1. 开启 2.4GHz 热点（见第 2.1 节），确认 SSID 名称。
2. 修改 [exp8/firmware/firmware.ino](exp8/firmware/firmware.ino) 的 `AP_LIST`。
3. 连接 LCD，按第 3 节完成接线。
4. 在 Arduino IDE 中选择 ESP32-C6 开发板与正确串口。
5. 烧录固件并打开串口监视器（115200）。
6. 记录 RSSI 数据并生成指纹（第 5.1 节）。
7. 把指纹写回 `FINGERPRINTS` 后重新烧录。
8. 放到不同位置验证 NN/WKNN 输出。

## 6. 定位算法说明
- NN（最近邻）：计算当前 RSSI 向量与指纹库中每个指纹的欧氏距离，选距离最小者。
- WKNN（加权 KNN）：取距离最近的 K 个指纹，按 $w = 1/(d+\epsilon)$ 加权求坐标中心，再映射回最近指纹。

LCD 显示格式：
- 第 1 行：`NN:<位置>`
- 第 2 行：`WK:<位置>`

## 7. 关键参数
- `AP_LIST`：需要参与定位的 AP SSID 列表（2.4GHz）。
- `FINGERPRINTS`：离线训练得到的 RSSI 均值指纹。
- `SCAN_INTERVAL_MS`：扫描周期（默认 2 秒）。
- `I2C_SDA/I2C_SCL`：I2C 引脚。

## 8. 常见问题
- LCD 无显示：检查 I2C 地址（0x27/0x3F）与引脚连接是否正确。
- RSSI 波动大：增加采样次数后取平均，或适当缩短实验区域，保证 AP 稳定。
- 只识别到部分 AP：确认手机热点为 2.4GHz，SSID 与 `AP_LIST` 完全一致。

## 9. 结果记录建议
- 对每个参考点记录 NN 与 WKNN 的定位输出，统计正确率。
- 比较两种算法在边界位置或遮挡环境下的差异。
