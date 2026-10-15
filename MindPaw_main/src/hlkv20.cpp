//----------------------------------------------
// HLK-V20 (SU-03T) 语音识别模块驱动实现
// UART 协议读取 + 命令字符串匹配
//----------------------------------------------
#include "hlkv20.h"
#include <string.h>

// ==================== 默认命令映射表 ====================
// 对应 HLK-V20 配置工具中设置的输出字符串
// 格式: "CMD<编号>" → VoiceCommand 枚举
const VoiceMapping HLKV20::DEFAULT_MAPPING[] = {
    {"CMD1",  VOICE_FORWARD},
    {"CMD2",  VOICE_BACKWARD},
    {"CMD3",  VOICE_LEFT},
    {"CMD4",  VOICE_RIGHT},
    {"CMD5",  VOICE_STOP},
    {"CMD6",  VOICE_SIT},
    {"CMD7",  VOICE_LIE},
    {"CMD8",  VOICE_HAPPY},
    {"CMD9",  VOICE_ANGRY},
    {"CMD10", VOICE_SAD},
    {"CMD11", VOICE_CURIOUS},
    {"CMD12", VOICE_LOVE},
    {"CMD13", VOICE_SLEEP},
    {"CMD14", VOICE_FREE},
    {"CMD15", VOICE_HAND_LEFT},
    {"CMD16", VOICE_HAND_RIGHT},
    {"CMD17", VOICE_TIME},
    {"CMD18", VOICE_WEATHER},
    {"CMD19", VOICE_LOGO},
    {"CMD20", VOICE_HELLO},
    {"CMD21", VOICE_BYE},
    {"CMD22", VOICE_ERROR},
};
// ==================== 构造函数 ====================
HLKV20::HLKV20() {
    _serial = nullptr;
    _initialized = false;
    _debug = false;
    _lastCmd = VOICE_NONE;
    _linePos = 0;
    _lineBuf[0] = '\0';
    _mapping = DEFAULT_MAPPING;
    _mappingCount = sizeof(DEFAULT_MAPPING) / sizeof(DEFAULT_MAPPING[0]);
}

// ==================== 初始化 ====================
bool HLKV20::begin(uint8_t rxPin, uint32_t baud) {
    if (_initialized) return true;

    // 创建 SoftwareSerial 实例 (仅接收, 不发送)
    // 使用动态分配避免在模块未用时占用内存
#if defined(ESP8266)
    _serial = new SoftwareSerial(rxPin, -1, false); // RX first, no TX
#else
    _serial = new HardwareSerial(1);
#endif
    if (!_serial) {
        Serial.println("HLK-V20: Failed to create UART receiver");
        return false;
    }

#if defined(ESP8266)
    _serial->begin(baud);
#else
    _serial->begin(baud, SERIAL_8N1, rxPin, -1);
#endif
    _linePos = 0;
    _initialized = true;

    if (_debug) {
        Serial.printf("HLK-V20: Initialized on GPIO%d @ %d baud\n", rxPin, baud);
    }

    return true;
}

// ==================== 设置自定义映射表 ====================
void HLKV20::setMapping(const VoiceMapping* mapping, uint8_t count) {
    if (mapping && count > 0) {
        _mapping = mapping;
        _mappingCount = count;
        if (_debug) {
            Serial.printf("HLK-V20: Custom mapping set (%d commands)\n", count);
        }
    }
}

// ==================== 字符串 → VoiceCommand 匹配 ====================
VoiceCommand HLKV20::matchCommand(const char* str) {
    if (!str || str[0] == '\0') return VOICE_NONE;

    // 去除首尾空白
    while (*str == ' ' || *str == '\r' || *str == '\n') str++;
    if (str[0] == '\0') return VOICE_NONE;

    if (_debug) {
        Serial.printf("HLK-V20: Matching \"%s\"\n", str);
    }

    // 遍历映射表
    for (uint8_t i = 0; i < _mappingCount; i++) {
        if (strcmp(str, _mapping[i].cmdStr) == 0) {
            if (_debug) {
                Serial.printf("HLK-V20: Matched \"%s\" → cmd %d\n",
                             _mapping[i].cmdStr, _mapping[i].command);
            }
            return _mapping[i].command;
        }
    }

    // 尝试数字匹配 (兼容直接输出数字的配置)
    char* endptr = nullptr;
    long num = strtol(str, &endptr, 10);
    if (endptr != str && num >= 1 && num <= 22) {
        VoiceCommand cmd = (VoiceCommand)(uint8_t)num;
        if (_debug) Serial.printf("HLK-V20: Numeric match %ld → cmd %d\n", num, cmd);
        return cmd;
    }

    if (_debug) Serial.printf("HLK-V20: No match for \"%s\"\n", str);
    return VOICE_ERROR;
}

// ==================== 主循环 ====================
VoiceCommand HLKV20::loop() {
    if (!_initialized || !_serial) return VOICE_NONE;

    // 读取所有可用字符
    while (_serial->available() > 0) {
        char c = (char)_serial->read();

        // 调试输出 (显示原始数据)
        if (_debug && c >= 32 && c < 127) {
            // 不在这里输出，避免刷屏
        }

        // 换行符 → 处理完整一行
        if (c == '\n' || c == '\r') {
            if (_linePos > 0) {
                _lineBuf[_linePos] = '\0';  // 终止字符串
                _lastCmd = matchCommand(_lineBuf);
                _linePos = 0;
                return _lastCmd;
            }
            continue;
        }

        // 可打印字符 → 加入行缓冲
        if (c >= 32 && c < 127 && _linePos < sizeof(_lineBuf) - 1) {
            _lineBuf[_linePos++] = c;
        }
    }

    return VOICE_NONE;  // 无新命令
}
