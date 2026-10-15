# 04 — 固件烧录指南 (VSCode + PlatformIO)

> **硬件版本提醒：** 先看 [两种摄像头方案](10_Camera_Profiles.md) 选择环境：ESP8266 用 `nodemcuv2`，ESP32-S3 N8R8 用 `esp32s3_dvp`。下文 ESP12F 烧录接线仅用于方案 A；两者都需 uploadfs。

> ⭐ **使用本教程前，请先支持一下这个开源项目：**
>
> 1. **Star ⭐** — 点击 GitHub 仓库右上角 ⭐ Star，收藏本项目，方便以后找到
> 2. **Fork 🍴** — 点击 Fork，把项目复制到你的 GitHub 账号下，可以自由修改
> 3. **Download 📥** — 点击绿色 **Code** 按钮 → **Download ZIP**，下载到本地开始复刻
>
> 📸 **操作示意图：**
>
> ![Star-Fork-Download](../Picture/demo.jpg)

---

## 环境搭建

### 1. 安装 VSCode

1. 访问 [code.visualstudio.com](https://code.visualstudio.com/) 下载安装
2. 安装完成后，打开 VSCode

### 2. 安装 PlatformIO 扩展

1. 在 VSCode 左侧点击 **扩展** 图标（或按 `Ctrl+Shift+X`）
2. 搜索 **PlatformIO IDE**
3. 点击 **安装**（首次安装可能需要 3-5 分钟）

### 3. 验证安装

安装完成后，VSCode 底部状态栏会出现一个 **蚂蚁头图标**（PlatformIO 的 Logo）
点击它 → **PlatformIO → Home**，如果能打开主页，说明安装成功。

---

## 打开项目

### 方法一：VSCode 打开文件夹

1. **文件 → 打开文件夹...**
2. 选择 `D:\TZY_Robot\MindPaw_main`
3. VSCode 会自动识别 PlatformIO 项目
4. 等待左下角出现 PlatformIO 的蚂蚁图标

### 方法二：从 PlatformIO Home 打开

1. 点击底部蚂蚁图标 → **Home**
2. 点击 **Open Project**
3. 选择 `D:\TZY_Robot\MindPaw_main`

---

## 编译固件

### 方法一：通过底部工具栏

1. 确保底部状态栏显示的默认环境是 `nodemcuv2`
2. 点击 **✔ (Build)** 图标 → 开始编译
3. 编译需要 1-3 分钟（首次会下载依赖库）
4. 出现 `SUCCESS` 即编译成功

### 方法二：通过命令面板

1. 按 `Ctrl+Shift+P` 打开命令面板
2. 输入并选择 **PlatformIO: Build**
3. 等待编译完成

### 方法三：通过终端

```bash
# 在 VSCode 终端中
cd D:\TZY_Robot\MindPaw_main
pio run
```

---

## 上传 SPIFFS 文件系统

SPIFFS 存储 Web 控制页面的 HTML 文件。**首次烧录必须执行此步骤。**

### 方法一：通过 PlatformIO

1. 按 `Ctrl+Shift+P` 打开命令面板
2. 输入并选择 **PlatformIO: Upload File System Image**
3. 等待上传完成（约 10-30 秒）

### 方法二：通过终端

```bash
pio run -t uploadfs
```

---

## 烧录到 ESP12F

### 接线

| ESP12F 引脚 | 连接 |
|-------------|------|
| TX (GPIO1) | USB-TTL RX |
| RX (GPIO3) | USB-TTL TX |
| GND | USB-TTL GND |
| 3.3V | USB-TTL 3.3V |
| GPIO0 | ⚠烧录前**短接到 GND** |
| GPIO15 | ⚠必须**10KΩ 下拉到 GND** |

### 烧录步骤

1. **进入下载模式：**
   - 将 GPIO0 短接到 GND
   - 给模块重新上电（或按 RST 复位）
   - 此时模块进入下载模式

2. **开始烧录：**
   - 点击底部工具栏 **→ (Upload)** 图标
   - 或终端运行：`pio run -t upload`
   - 等待烧录完成（约 30-60 秒）

3. **退出下载模式：**
   - 断开 GPIO0 与 GND 的连接
   - 按 RST 复位

4. **验证：**
   - 打开串口监视器：点击底部 **🔌 (Serial Monitor)** 图标
   - 波特率设置为 `115200`
   - 看到 `热点已启动` 即表示运行成功

### 常见烧录错误

| 错误信息 | 原因 | 解决方法 |
|---------|------|---------|
| `A fatal error occurred: Failed to connect to ESP8266` | 未进入下载模式 | 检查 GPIO0 是否接地，重新上电 |
| `ESP8266 chip doesn't exist` | 接线错误 | 检查 TX/RX 是否接反，检查 3.3V 供电 |
| `Timed out waiting for packet header` | 波特率问题 | 降低上传速度：在 platformio.ini 添加 `upload_speed = 115200` |

---

## 串口命令参考

烧录并启动后，可通过串口发送命令测试：

| 命令 | 功能 |
|------|------|
| `front` | 前进 |
| `back` | 后退 |
| `left` | 左转 ×3 |
| `right` | 右转 ×3 |
| `sitdown` | 坐下 |
| `lie` | 趴下 |
| `dosleep` | 睡觉 |
| `kaixin` | 开心表情 |
| `shengqi` | 生气表情 |
| `nanshou` | 难过表情 |
| `haoqi` | 好奇表情 |
| `xihuan` | 喜爱表情 |
| `shijian` | 显示时间 |
| `tianqi` | 显示天气 |
| `ask <文本>` | 发送给 AI Agent |
| `ai clear` | 清除 AI 对话历史 |
| `emotion` | 查看当前 PAD 情感状态 |
| `emotion reset` | 重置情感状态 |
| `gesture` | 触发一次手势识别 |

---

## 烧录流程小结

```mermaid
graph TD
    A[安装 VSCode] --> B[安装 PlatformIO 扩展]
    B --> C[打开 MindPaw_main 目录]
    C --> D{首次烧录?}
    D -->|是| E[编译固件 pio run]
    D -->|否| G[修改代码后编译]
    E --> F[上传 SPIFFS pio run -t uploadfs]
    F --> H[短路 GPIO0→GND]
    G --> H
    H --> I[上电/复位]
    I --> J[点击 Upload]
    J --> K[断开 GPIO0→GND]
    K --> L[按 RST 复位]
    L --> M[打开串口监视器 115200]
    M --> N[看到"热点已启动"→ 成功!]
```
