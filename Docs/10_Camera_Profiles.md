# 两种 OV2640 方案：采购、接线与验证

两种摄像头分别有固件后端，但必须配套选择主控、构建环境和接线。
**它们不能在原 ESP8266 接线上直接互换。新增适配尚未完成实物验收；编译通过不等于整机运行通过。**

| 项目 | A：SPI / FIFO | B：DVP / 无 FIFO |
|---|---|---|
| 主控 | ESP12F / NodeMCU ESP8266 | ESP32-S3-DevKitC-1，ESP32-S3-WROOM-1 N8R8（8MB Flash、8MB OPI PSRAM） |
| 摄像头 | ArduCAM Mini **2MP Plus OV2640**，SPI、带 FIFO | OV2640 **8 位 DVP 并口转接模块**，无 FIFO，带所需稳压及接口，明确支持 3.3V 供电/逻辑 |
| 固件环境 | `nodemcuv2` | `esp32s3_dvp` |
| 驱动 | ArduCAM，上游版本固定，显式选择 `OV2640_MINI_2MP_PLUS` | Espressif `esp_camera`，由固定的 Arduino ESP32 平台提供 |
| 舵机 | PCA9685 通道 0–3 | PCA9685 通道 0–3 |
| 输出 | 160×120 JPEG；320×240 RGB565 下采样为 40×30 灰度 | 160×120 JPEG；解码后下采样为 40×30 灰度 |

“OV2640”只代表传感器型号。不要仅凭商品标题购买；核对模块型号、接口和供电规格。
方案 B 不是直接给裸传感器芯片或无转接板的 FPC 排线供电，也不适用 ESP32-CAM 默认引脚。
其他 Mini 版本、其他 ESP32 开发板没有被本配置覆盖。原文档的 ¥15 / ¥25 不作为当前报价，整机总价待选型和实测后更新。

## 共同外设

| 外设信号 | A：ESP8266 GPIO | B：ESP32-S3 GPIO |
|---|---:|---:|
| OLED + PCA9685 SDA | 4 / D2 | 1 |
| OLED + PCA9685 SCL | 5 / D1 | 2 |
| 语音模块 TX → 主控 RX | 0 / D3 | 47 |
| 扬声器晶体管驱动输入 | 16 / D0 | 14 |
| 按键 → GND | 2 / D4 | 38 |
| 可选电池分压 ADC | A0 | 10 |

PCA9685 地址为 `0x40`，四个舵机信号接通道 0、1、2、3；OE 接 GND。
PCA9685 VCC 使用 3.3V，V+ 使用独立的足额 5V 舵机电源，全部 GND 共地。
I²C 上拉到 3.3V；OLED、PCA9685 与方案 A 摄像头控制线共用该总线，避免重复并联过多上拉电阻。
扬声器通过原晶体管电路驱动，不能直接连接 GPIO。

**迁移旧接线：断电后，将四个舵机信号从 GPIO14/16/12/13 移到 PCA9685。**
不得继续用 1kΩ 电阻把舵机 PWM 与摄像头 SPI 共接。移除 GPIO15 上的第二按键，它由摄像头 CS 独占。
PCA9685 缺失时固件不输出舵机信号，不会回退到旧 GPIO。

原仓库 PCB 属于旧硬件资料，未经本次两方案改板验证；不要直接按旧 PCB 打样后假定可用。
先按本页使用面包板/转接板验证。方案 B 的 GPIO35–37 留给 N8R8 的 OPI 存储，GPIO19/20 留给 USB，GPIO43/44 留给 UART0。

## A：ArduCAM Mini 2MP Plus

| 摄像头引脚 | ESP8266 |
|---|---|
| CS | GPIO15 / D8，保留启动所需 10kΩ 下拉；不接按键 |
| SCK | GPIO14 / D5，独占 |
| MISO | GPIO12 / D6，独占 |
| MOSI | GPIO13 / D7，独占 |
| SDA | GPIO4 / D2，与 OLED/PCA9685 共用 |
| SCL | GPIO5 / D1，与 OLED/PCA9685 共用 |
| VCC / GND | 按所选模块规格接 3.3V / GND |

SPI 负责图像读取，SDA/SCL 负责配置传感器，两部分都要接。
GPIO0/2 上电应保持高，GPIO15 上电保持低；烧录 ESP8266 时 GPIO0 才拉低。
若语音模块影响 GPIO0 启动电平，先断开其 TX 验证启动。

## B：ESP32-S3 N8R8 + DVP 转接模块

以模块信号名称核对接线，不以排针位置或线色判断。

| OV2640 信号 | ESP32-S3 GPIO |
|---|---:|
| D0 / Y2 | 4 |
| D1 / Y3 | 5 |
| D2 / Y4 | 6 |
| D3 / Y5 | 7 |
| D4 / Y6 | 8 |
| D5 / Y7 | 9 |
| D6 / Y8 | 12 |
| D7 / Y9 | 11 |
| PCLK | 13 |
| XCLK | 15 |
| VSYNC | 16 |
| HREF | 17 |
| SIOD / SDA | 18 |
| SIOC / SCL | 21 |
| PWDN | GND（低有效运行） |
| RESET / RESETB | 3.3V 逻辑高（按模块规格上拉） |
| VCC / GND | 确认支持 3.3V 的转接模块接 3.3V / GND |

DVP 摄像头的 SCCB 使用单独引脚，不与 GPIO1/2 的外设 I²C 共接。
使用短线。不要给不具备稳压/电平适配的传感器裸板直接套用上述供电。
此配置要求检测到 PSRAM；未检测到时会禁用摄像头并输出诊断信息。

## 编译、烧录

在仓库根目录选择一组命令。两种方案都要上传网页文件系统。

```bash
# A
pio run -d MindPaw_main -e nodemcuv2
pio run -d MindPaw_main -e nodemcuv2 -t upload
pio run -d MindPaw_main -e nodemcuv2 -t uploadfs

# B：使用开发板 USB-to-UART 接口；必要时按住 BOOT 再按 RESET 进入下载模式
pio run -d MindPaw_main -e esp32s3_dvp
pio run -d MindPaw_main -e esp32s3_dvp -t upload
pio run -d MindPaw_main -e esp32s3_dvp -t uploadfs
```

串口为 115200。先断开舵机 V+，仅测试主控、摄像头和 OLED。
不要把 ESP8266 的固件烧到 ESP32-S3，也不要把原 ESP8266 PCB 用作 S3 接线依据。

## 实物验收（尚待执行）

1. 记录主控完整型号、摄像头正反面照片、供电规格和固件提交号。
2. 摄像头未接时应输出初始化失败；网页仍应启动，手势采集应禁用。
3. 接上摄像头，确认 SPI/传感器 ID 或 DVP 初始化成功；日志成功本身不代表图像验收通过。
4. 关闭串口监视器，执行 `python MindPaw_main/tools/capture_camera.py COM5 --output frame.jpg`，打开 JPEG 确认是实际场景。
5. 执行 `python MindPaw_main/tools/capture_camera.py COM5 --gray --output frame.pgm`，检查 40×30 灰度图；交替采集 JPEG/灰度至少 20 次，确认没有错帧或重启。
6. 如需推流，再用实际 JPEG POST 到 recon 服务，确认返回 hazard JSON。推流延迟需要重新测量，不能沿用旧延迟宣传。
7. 最后接独立舵机电源，架空机器人，确认四路动作与摄像头同时运行；记录电压、电流和复位情况。

电池采样可选。代码保留 8.4 倍分压假设；NodeMCU A0 自带分压可能与 ESP12F 裸芯片不同，必须按板卡实际分压校准，不能把电池直接接 ADC。
遇到舵机异常先切断 V+。回退旧固件时也必须恢复匹配的接线，不能在新旧接线间直接换固件。

参考：[ArduCAM 官方库](https://github.com/ArduCAM/Arduino)、[Espressif 摄像头驱动](https://github.com/espressif/esp32-camera)。
