# MindPaw_main：初学者复现路径

这里是 MindPaw 的 ESP8266 / ESP32-S3 固件和网页控制端。目标是让你先把机器狗完整跑起来：运动、OLED、网页控制、语音、摄像头手势和可选 AI 对话都保留在同一份固件中。

`ai-infra` 不是本路径的必需组件。第一次复现建议直接使用 ESP8266 直连豆包；完成基础复现后，再进入 [AI Infra 研究路径](../ai-infra/README.md)。

## 1. 先选硬件范围

先阅读 [双摄像头方案](../Docs/10_Camera_Profiles.md)。A 是 ESP8266 + ArduCAM Mini 2MP Plus；B 是 ESP32-S3 N8R8 + DVP 无 FIFO 模块。两者不可混用接线；新增配置尚待实物验收。

| 目标 | 最少硬件 | 可以体验 |
|------|---------|---------|
| 基础复现 | 所选主控、PCA9685、4 个舵机、OLED | 运动、表情、网页控制 |
| 完整 Demo | 基础复现 + HLK-V20 + 扬声器 + 对应方案的 OV2640 模块 | 语音、声音、手势、AI 对话 |
| 只验证软件 | NodeMCU + USB-TTL | 编译、网页和部分串口功能 |

硬件接线、PCB 和电源注意事项见 [SCH&PCB/README.md](../SCH&PCB/README.md) 与 [PIN_WIRING.md](PIN_WIRING.md)。

## 2. 安装软件

1. 安装 VS Code。
2. 安装 PlatformIO IDE 扩展。
3. 使用 USB-TTL 连接 ESP12F，确认是 3.3V 电平。
4. 在 VS Code 中打开本目录 `MindPaw_main`，不要只打开 `src`。

更详细的安装截图和烧录错误表见 [Docs/04_Firmware_Flashing.md](../Docs/04_Firmware_Flashing.md)。

## 3. 编译和烧录

在本目录执行：

```bash
# A；方案 B 将 nodemcuv2 替换为 esp32s3_dvp
pio run -e nodemcuv2
pio run -e nodemcuv2 -t upload
pio run -e nodemcuv2 -t uploadfs
```

首次烧录必须执行 `uploadfs`，否则网页文件不存在。ESP8266 烧录时 GPIO0 接 GND；完成后断开 GPIO0，再按 RST。串口监视器使用 `115200`。

看到以下信息，说明固件基本启动：

```text
热点已启动
访问地址: 192.168.4.1
```

## 4. 第一次运行

1. 给设备上电，等待 OLED 显示 WiFi 信息。
2. 手机连接热点 `MindPaw`，密码 `mindpaw1234`。
3. 浏览器打开 `http://192.168.4.1`。
4. 依次测试：前进、后退、表情、坐下、网页返回。
5. 舵机动作正常后，再连接语音、扬声器和摄像头。

逐个接模块可以快速定位问题，建议顺序是：

```text
电源 → 所选主控 → OLED → PCA9685 → 舵机 → 网页 → HLK-V20 → 扬声器 → OV2640
```

## 5. 配置 AI 对话（可选）

最简单的方式是直连豆包：

1. 在方舟平台创建推理接入点和 API Key。
2. 打开 `http://192.168.4.1/aiconfig.html`。
3. 填写 API Key 和 Endpoint ID。
4. Gateway 地址留空。
5. 打开 `http://192.168.4.1/aichat.html` 测试。

配置细节见 [Docs/07_API_Guide.md](../Docs/07_API_Guide.md)。如果使用 Gateway，请先阅读 [ai-infra/README.md](../ai-infra/README.md)。

## 6. 完整 Demo 验收表

| 验收项 | 操作 | 正常结果 |
|--------|------|---------|
| 启动 | 上电 | OLED 亮，串口无连续重启 |
| 网页 | 访问 192.168.4.1 | 首页可以打开 |
| 舵机 | 点击前进/坐下 | 动作完成后停止 |
| 表情 | 点击开心/难过 | OLED 图像切换 |
| 语音 | 说“你好”“前进” | 播放反馈并执行动作 |
| 手势 | 摄像头前挥手 | 触发对应动作或自由模式 |
| AI | 发送一句聊天 | 网页显示回复，设备执行动作/表情 |

手势模型当前提供轻量推理管线；如果没有使用训练后的权重，识别准确率不能作为验收标准。训练工具见 `tools/distill_gesture.py`。

## 7. 常见问题

| 现象 | 优先检查 |
|------|---------|
| 编译找不到库 | PlatformIO 是否打开了 `MindPaw_main`，重新执行 `pio run` |
| 网页 404 | 是否执行 `pio run -t uploadfs`，是否重新断电启动 |
| 无法烧录 | GPIO0 接地、TX/RX 交叉、USB-TTL 使用 3.3V |
| ESP 不启动 | GPIO15 必须 10KΩ 下拉到 GND，舵机不要由 3.3V 供电 |
| 舵机抖动/重启 | 舵机使用独立 5V 供电并与 ESP 共地 |
| OLED 不亮 | 检查 GPIO4=SDA、GPIO5=SCL 和 I2C 上拉 |
| 语音没反应 | HLK-V20 是否已烧录 CMD1-CMD22，确认 TX 接 GPIO0 |
| 摄像头失败 | 按所选方案检查完整接线、供电与摄像头初始化日志 |
| AI 未配置 | 确认 API Key、Endpoint ID 和 WiFi STA 已连接 |
| Gateway 连接失败 | 网关与设备 STA 网络是否互通，地址是否包含 `/v1/chat/completions` |

更完整的硬件排查见 [Docs/08_Product_Manual.md](../Docs/08_Product_Manual.md)。

## 8. 串口诊断

串口监视器设置为 `115200`，可发送：

```text
front
back
left
right
ai 你好
ai clear
```

串口日志中的 `DOUBAO`、`EMOTION`、`GESTURE_NN` 前缀分别对应 AI、情绪和手势模块。

## 下一步

- 想修改动作、表情或网页：从 `src/main.cpp` 和 `data/` 开始。
- 想修改电路或打样：阅读 [SCH&PCB/README.md](../SCH&PCB/README.md)。
- 想研究边缘-云 AI 调度：阅读 [ai-infra/README.md](../ai-infra/README.md)。
