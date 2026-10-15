# MindPaw 当前引脚说明

完整接线表与代码配置对应关系见 [两种摄像头方案](../Docs/10_Camera_Profiles.md)。
代码中的引脚定义集中在 `src/board_config.h`。

| 环境 | 主控 | 摄像头 | 舵机 |
|---|---|---|---|
| `nodemcuv2` | ESP8266 | ArduCAM Mini 2MP Plus OV2640，SPI/FIFO | PCA9685 通道 0–3 |
| `esp32s3_dvp` | ESP32-S3-DevKitC-1 N8R8 | OV2640 DVP 转接模块，无 FIFO | PCA9685 通道 0–3 |

旧版 GPIO14/16/12/13 直连舵机的表格已撤回。当前固件不会驱动这些引脚上的舵机。
升级前断电并将四个信号移到 PCA9685；不要靠 1kΩ 将 PWM 和 SPI 共接。
GPIO15 在 ESP8266 上专用于摄像头 CS，不接第二按键。
摄像头 SDA/SCL 也必须接线，不能只接 SPI。

ESP32-S3 不是原 PCB 的直接替换件。原 PCB 工程仅作为旧硬件参考，尚未按双方案重画验证。
新增配置尚待实物验收，具体步骤见上述方案文档。
