//----------------------------------------------
// HLK-V20 (SU-03T) 语音识别模块驱动 (UART)
//
// 与 LD3320 不同，HLK-V20 自带语音识别固件，
// 通过 UART 输出识别结果。仅需 1 个引脚 (RX)。
//
// 接线:
//   HLK-V20 TX → ESP8266 GPIO (任意, 本文档用 D2=GPIO4)
//   HLK-V20 RX → 可选 (仅用于 PC 配置工具配置)
//   HLK-V20 VCC → 3.3V
//   HLK-V20 GND → GND
//
// 使用前需在 HLK-V20 配置工具中设置:
//   1. 波特率: 9600 (默认)
//   2. 每个语音命令 → 输出字符串 (如 "CMD1", "CMD2"...)
//   3. 烧录配置到模块
//----------------------------------------------
#ifndef HLKV20_H
#define HLKV20_H

#include <Arduino.h>
#include "board_config.h"
#if defined(ESP8266)
#include <SoftwareSerial.h>
#else
#include <HardwareSerial.h>
#endif

#define HLKV20_RX_PIN MP_VOICE_RX

// ==================== 语音命令枚举 (与 LD3320 兼容) ====================
enum VoiceCommand : uint8_t {
    VOICE_NONE       = 0,
    VOICE_FORWARD    = 1,   // 前进
    VOICE_BACKWARD   = 2,   // 后退
    VOICE_LEFT       = 3,   // 左转
    VOICE_RIGHT      = 4,   // 右转
    VOICE_STOP       = 5,   // 停止
    VOICE_SIT        = 6,   // 坐下
    VOICE_LIE        = 7,   // 趴下
    VOICE_HAPPY      = 8,   // 开心
    VOICE_ANGRY      = 9,   // 生气
    VOICE_SAD        = 10,  // 难受
    VOICE_CURIOUS    = 11,  // 好奇
    VOICE_LOVE       = 12,  // 喜欢
    VOICE_SLEEP      = 13,  // 睡觉
    VOICE_FREE       = 14,  // 自由模式
    VOICE_HAND_LEFT  = 15,  // 抬左手
    VOICE_HAND_RIGHT = 16,  // 抬右手
    VOICE_TIME       = 17,  // 时间
    VOICE_WEATHER    = 18,  // 天气
    VOICE_LOGO       = 19,  // 显示Logo
    VOICE_HELLO      = 20,  // 你好
    VOICE_BYE        = 21,  // 再见
    VOICE_ERROR      = 22,  // 识别无效
};

// ==================== HLK-V20 命令映射表条目 ====================
// 每个条目: HLK-V20 配置工具中设置的输出字符串 → VoiceCommand 枚举
typedef struct {
    const char* cmdStr;     // HLK-V20 输出的字符串 (如 "CMD1")
    VoiceCommand command;   // 对应的 VoiceCommand 枚举值
} VoiceMapping;

class HLKV20 {
public:
    HLKV20();

    // 初始化 — 开始监听 SoftwareSerial
    // rxPin: HLK-V20 TX 连接的 ESP8266 GPIO
    // baud: HLK-V20 配置的波特率 (默认 9600)
    bool begin(uint8_t rxPin = HLKV20_RX_PIN, uint32_t baud = 9600);

    // 主循环调用 — 检查识别结果
    // 返回 VOICE_NONE 表示无新识别
    VoiceCommand loop();

    // 获取最近一次识别的命令
    VoiceCommand getLastCommand() const { return _lastCmd; }

    // 清除命令缓存
    void clearCommand() { _lastCmd = VOICE_NONE; }

    // 模块是否已初始化
    bool isAvailable() const { return _initialized; }

    // 设置调试输出
    void setDebug(bool debug) { _debug = debug; }

    // 设置自定义命令映射表
    // 如果不调用，使用默认 "CMD1"/"CMD2"/... 映射
    void setMapping(const VoiceMapping* mapping, uint8_t count);

private:
#if defined(ESP8266)
    SoftwareSerial* _serial;
#else
    HardwareSerial* _serial;
#endif
    bool _initialized;
    bool _debug;
    VoiceCommand _lastCmd;

    // 行缓冲 — 存放 HLK-V20 输出的 ASCII 字符串
    char _lineBuf[32];
    uint8_t _linePos;

    // 命令映射表
    const VoiceMapping* _mapping;
    uint8_t _mappingCount;

    // 默认映射表
    static const VoiceMapping DEFAULT_MAPPING[];

    // 匹配命令字符串 → VoiceCommand
    VoiceCommand matchCommand(const char* str);
};

#endif // HLKV20_H
