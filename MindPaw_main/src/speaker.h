//----------------------------------------------
// 扬声器/蜂鸣器音频输出模块
// 支持播放提示音、旋律和简单互动音效
//----------------------------------------------
#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>

//============================================================
// 扬声器引脚配置
//
// 扬声器本身有 2 根线 (+/-)，但控制只需 1 个 GPIO。
// 推荐接线 (三极管驱动):
//   GPIO16 ──[1KΩ]── NPN三极管(B)  三极管(C)── 喇叭(-)
//   三极管(E)── GND                喇叭(+)── VCC(5V)
//
// 或者用功放模块 (MAX98357/PAM8403):
//   GPIO16 ──▶ 功放 IN   功放 OUT+/- ──▶ 喇叭
//============================================================
#include "board_config.h"
#define SPEAKER_PIN MP_SPEAKER

// 音符频率定义 (Hz)
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_D1  37
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_G1  49
#define NOTE_A1  55
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_D2  73
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_G2  98
#define NOTE_A2  110
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_D3  147
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_G6  1568
#define NOTE_A6  1760
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_D7  2349
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_G7  3136
#define NOTE_A7  3520
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_REST 0  // 休止符

// 音符结构: {频率, 持续时间(ms)}
typedef struct {
    uint16_t freq;
    uint16_t duration;
} Note;

// 内置旋律枚举
enum Melody : uint8_t {
    MELODY_STARTUP,      // 开机音效
    MELODY_COMMAND_OK,   // 命令确认
    MELODY_ERROR,        // 错误提示
    MELODY_HELLO,        // 打招呼
    MELODY_GOODBYE,      // 再见
    MELODY_HAPPY,        // 开心
    MELODY_SAD,          // 难过
    MELODY_ALERT,        // 警告
    MELODY_DONE,         // 完成
    MELODY_BEEP,         // 简单哔声
};

class Speaker {
public:
    // 构造函数
    Speaker(uint8_t pin = SPEAKER_PIN);

    // 初始化
    void begin();

    // 播放指定旋律 (非阻塞)
    void play(Melody melody);

    // 播放自定义音符序列 (非阻塞)
    void playNotes(const Note* notes, uint16_t count);

    // 播放单个音调 (阻塞)
    void playTone(uint16_t freq, uint16_t duration);

    // 停止播放
    void stop();

    // 主循环调用 — 处理非阻塞播放
    void loop();

    // 是否正在播放
    bool isPlaying() const { return _isPlaying; }

    // 设置音量 (0-100)
    void setVolume(uint8_t vol) { _volume = constrain(vol, 0, 100); }

    // 设置静音
    void setMute(bool mute) { _mute = mute; if (mute) stop(); }
    bool isMuted() const { return _mute; }

private:
    uint8_t _pin;
    uint8_t _volume;
    bool _mute;
    bool _isPlaying;

    // 非阻塞播放状态
    const Note* _currentNotes;
    uint16_t _noteCount;
    uint16_t _noteIndex;
    unsigned long _noteStartMs;

    // 内置旋律定义
    static const Note _startupMelody[];
    static const Note _commandOkMelody[];
    static const Note _errorMelody[];
    static const Note _helloMelody[];
    static const Note _goodbyeMelody[];
    static const Note _happyMelody[];
    static const Note _sadMelody[];
    static const Note _alertMelody[];
    static const Note _doneMelody[];
    static const Note _beepMelody[];
    static const Note* _melodyMap[];
};

#endif // SPEAKER_H
