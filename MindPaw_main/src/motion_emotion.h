//----------------------------------------------
// 情感动作模块 — 非阻塞舵机运动
// 支持步进序列 + 参数化正弦波生成
//----------------------------------------------
#ifndef MOTION_EMOTION_H
#define MOTION_EMOTION_H

#include <Arduino.h>
#include "servo_output.h"
#include "doubao_config.h"

// 正弦振荡参数
struct OscillatorParams {
    int16_t center[4];     // 各舵机中心角度 [0, 180]
    int16_t amplitude[4];  // 各舵机振幅 [0, 90]
    float frequency[4];    // 各舵机频率 (Hz)
    float phase[4];        // 各舵机相位偏移 (radians)
    uint16_t durationMs;   // 总持续时间
};

class MotionEmotion {
public:
    MotionEmotion();

    void attach(MindPawServo& s1, MindPawServo& s2, MindPawServo& s3, MindPawServo& s4);

    // 触发情感动作 (非阻塞)
    void trigger(EmotionAction action, uint8_t repeat = 1);

    // 触发参数化动作 (非阻塞)
    // 用正弦波生成有机运动
    void triggerParametric(const OscillatorParams& params, uint8_t repeat = 1);

    // 主循环
    void loop();

    bool isPlaying() const { return _isPlaying; }
    void stop();
    bool isAttached() const { return _attached; }

    // 转换情感强度 (归一化情感值 → 振幅/频率缩放)
    // pleasure: [-1,1], arousal: [0,1]
    void setEmotionalIntensity(float pleasure, float arousal);

private:
    MindPawServo* _s[4];
    bool _attached;
    bool _isPlaying;

    // 动作状态
    EmotionAction _currentAction;
    uint8_t _repeatLeft;
    unsigned long _stepStartMs;
    uint8_t _stepIndex;
    bool _parametricMode;  // true=参数化, false=步进

    // 参数化振荡状态
    OscillatorParams _osc;
    unsigned long _oscStartMs;

    // 情感强度缩放 (影响参数化动作的幅度和频率)
    float _intensityScale;    // [0.5, 1.5]
    float _speedScale;        // [0.5, 1.5]

    // ---------- 步进定义 ----------
    struct MotionStep {
        int16_t pos1, pos2, pos3, pos4;
        uint16_t durationMs;
    };

    static const MotionStep _bowSteps[];
    static const MotionStep _stretchSteps[];
    static const MotionStep _hideFaceSteps[];

    // ---------- 参数化动作预设 ----------
    static OscillatorParams makeDance(float intensity, float speed);
    static OscillatorParams makeNod(float intensity, float speed);
    static OscillatorParams makeShake(float intensity, float speed);
    static OscillatorParams makeCelebrate(float intensity, float speed);
    static OscillatorParams makeComfort(float intensity, float speed);

    const MotionStep* getSteps(EmotionAction action, uint8_t& count) const;
    void applyStep(const MotionStep& step);
    void applyOscillator(unsigned long elapsedMs);
};

#endif // MOTION_EMOTION_H
