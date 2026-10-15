//----------------------------------------------
// 扬声器/蜂鸣器音频输出模块实现
// 使用 tone() 函数播放 PWM 音调
//----------------------------------------------
#include "speaker.h"

// ============ 内置旋律定义 ============

// 开机音效: 上行音阶
const Note Speaker::_startupMelody[] = {
    {NOTE_C4, 100}, {NOTE_E4, 100}, {NOTE_G4, 100}, {NOTE_C5, 150},
    {NOTE_REST, 50}, {NOTE_G4, 100}, {NOTE_C5, 200},
    {NOTE_REST, 0}
};

// 命令确认: 短促双音
const Note Speaker::_commandOkMelody[] = {
    {NOTE_E5, 80}, {NOTE_REST, 40}, {NOTE_G5, 120},
    {NOTE_REST, 0}
};

// 错误提示: 低沉下降
const Note Speaker::_errorMelody[] = {
    {NOTE_C4, 150}, {NOTE_REST, 50}, {NOTE_G3, 150}, {NOTE_REST, 50}, {NOTE_E3, 200},
    {NOTE_REST, 0}
};

// 打招呼: 欢快三音
const Note Speaker::_helloMelody[] = {
    {NOTE_C5, 100}, {NOTE_E5, 100}, {NOTE_G5, 150},
    {NOTE_REST, 0}
};

// 再见: 下行
const Note Speaker::_goodbyeMelody[] = {
    {NOTE_G5, 120}, {NOTE_E5, 120}, {NOTE_C5, 200},
    {NOTE_REST, 0}
};

// 开心: 跳跃音
const Note Speaker::_happyMelody[] = {
    {NOTE_C5, 80}, {NOTE_D5, 80}, {NOTE_E5, 80}, {NOTE_C5, 80},
    {NOTE_E5, 80}, {NOTE_D5, 80}, {NOTE_C5, 160},
    {NOTE_REST, 0}
};

// 难过: 缓慢下降
const Note Speaker::_sadMelody[] = {
    {NOTE_G4, 200}, {NOTE_E4, 200}, {NOTE_C4, 300},
    {NOTE_REST, 0}
};

// 警告: 快速重复
const Note Speaker::_alertMelody[] = {
    {NOTE_C5, 100}, {NOTE_REST, 100}, {NOTE_C5, 100}, {NOTE_REST, 100},
    {NOTE_C5, 100}, {NOTE_REST, 100}, {NOTE_C5, 200},
    {NOTE_REST, 0}
};

// 完成: 下行三音
const Note Speaker::_doneMelody[] = {
    {NOTE_G5, 100}, {NOTE_E5, 100}, {NOTE_C5, 200},
    {NOTE_REST, 0}
};

// 简单哔声
const Note Speaker::_beepMelody[] = {
    {NOTE_C5, 200},
    {NOTE_REST, 0}
};

// 旋律指针映射表
const Note* Speaker::_melodyMap[] = {
    Speaker::_startupMelody,    // MELODY_STARTUP
    Speaker::_commandOkMelody,  // MELODY_COMMAND_OK
    Speaker::_errorMelody,      // MELODY_ERROR
    Speaker::_helloMelody,      // MELODY_HELLO
    Speaker::_goodbyeMelody,    // MELODY_GOODBYE
    Speaker::_happyMelody,      // MELODY_HAPPY
    Speaker::_sadMelody,        // MELODY_SAD
    Speaker::_alertMelody,      // MELODY_ALERT
    Speaker::_doneMelody,       // MELODY_DONE
    Speaker::_beepMelody,       // MELODY_BEEP
};

// ==================== 构造函数 ====================
Speaker::Speaker(uint8_t pin) : _pin(pin) {
    _volume = 100;
    _mute = false;
    _isPlaying = false;
    _currentNotes = nullptr;
    _noteCount = 0;
    _noteIndex = 0;
    _noteStartMs = 0;
}

// ==================== 初始化 ====================
void Speaker::begin() {
#if defined(ESP32)
    setToneChannel(7); // Camera XCLK owns LEDC channel/timer 0.
#endif
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

// ==================== 播放控制 ====================
void Speaker::play(Melody melody) {
    if (_mute) return;

    if (melody < sizeof(_melodyMap) / sizeof(_melodyMap[0])) {
        const Note* notes = _melodyMap[melody];
        // 计算音符数量 (直到 freq=0)
        uint16_t count = 0;
        while (notes[count].freq != NOTE_REST || notes[count].duration > 0) {
            count++;
        }
        // 加 1 包含终止标记
        playNotes(notes, count + 1);
    }
}

void Speaker::playNotes(const Note* notes, uint16_t count) {
    if (_mute || notes == nullptr || count == 0) return;

    // 如果正在播放，先停止
    if (_isPlaying) {
        noTone(_pin);
    }

    _currentNotes = notes;
    _noteCount = count;
    _noteIndex = 0;
    _isPlaying = true;
    _noteStartMs = millis();

    // 开始第一个音符
    if (count > 0 && notes[0].freq != 0) {
        tone(_pin, notes[0].freq, notes[0].duration);
    }
}

void Speaker::playTone(uint16_t freq, uint16_t duration) {
    if (_mute) return;
    if (_isPlaying) {
        noTone(_pin);
        _isPlaying = false;
    }
    tone(_pin, freq, duration);
    delay(duration);
    noTone(_pin);
}

void Speaker::stop() {
    if (_isPlaying) {
        noTone(_pin);
        _isPlaying = false;
    }
    _currentNotes = nullptr;
    _noteCount = 0;
    _noteIndex = 0;
}

// ==================== 主循环 (非阻塞播放更新) ====================
void Speaker::loop() {
    if (!_isPlaying || _currentNotes == nullptr) return;

    unsigned long now = millis();
    unsigned long elapsed = now - _noteStartMs;
    unsigned long accumulatedDuration = 0;

    // 计算当前应该播放到哪个音符
    for (uint16_t i = 0; i < _noteCount; i++) {
        unsigned long noteEnd = accumulatedDuration + _currentNotes[i].duration;
        if (elapsed < noteEnd) {
            // 当前正在播放这个音符
            if (i != _noteIndex) {
                _noteIndex = i;
                // 切换到新音符
                if (_currentNotes[i].freq != 0) {
                    tone(_pin, _currentNotes[i].freq, _currentNotes[i].duration);
                } else {
                    noTone(_pin);  // 休止符
                }
            }
            return;
        }
        accumulatedDuration = noteEnd;
    }

    // 播放完毕
    noTone(_pin);
    _isPlaying = false;
    _currentNotes = nullptr;
    _noteCount = 0;
    _noteIndex = 0;
}
