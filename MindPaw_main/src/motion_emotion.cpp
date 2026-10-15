//----------------------------------------------
// 情感动作模块实现
// 支持步进序列 + 参数化正弦波动作生成
//----------------------------------------------
#include "motion_emotion.h"
#include <math.h>

// ==================== 步进序列 (离散动作) ====================
const MotionEmotion::MotionStep MotionEmotion::_bowSteps[] = {
    {90, 90, 90, 90, 200},
    {40, 40, 140, 140, 500},   // 坐下
    {130, 130, 90, 90, 400},   // 前倾
    {130, 130, 90, 90, 500},   // 保持
    {40, 40, 140, 140, 400},   // 回正
    {90, 90, 90, 90, 400},     // 站起
    {0, 0, 0, 0, 0},
};

const MotionEmotion::MotionStep MotionEmotion::_stretchSteps[] = {
    {140, 140, 90, 90, 400},
    {90, 90, 40, 40, 400},
    {140, 140, 40, 40, 500},
    {90, 90, 90, 90, 400},
    {0, 0, 0, 0, 0},
};

const MotionEmotion::MotionStep MotionEmotion::_hideFaceSteps[] = {
    {140, 140, 90, 90, 400},
    {130, 130, 70, 70, 300},
    {140, 140, 90, 90, 500},
    {90, 90, 90, 90, 400},
    {0, 0, 0, 0, 0},
};

// ==================== 参数化动作预设 ====================
// 使用正弦波生成有机运动，每个舵机独立 ω 和 φ

OscillatorParams MotionEmotion::makeDance(float intensity, float speed) {
    OscillatorParams p;
    float amp = 30.0f * intensity;   // 振幅随情感强度变化
    float freq = 2.0f * speed;       // 频率随情感速度变化
    p.center[0] = 90;    p.amplitude[0] = (int16_t)amp;
    p.center[1] = 90;    p.amplitude[1] = (int16_t)(amp * 0.7f);
    p.center[2] = 90;    p.amplitude[2] = (int16_t)(amp * 0.5f);
    p.center[3] = 90;    p.amplitude[3] = (int16_t)(amp * 0.7f);
    for (int i = 0; i < 4; i++) {
        p.frequency[i] = freq + i * 0.3f;
        p.phase[i] = i * 1.57f;  // 90° 相位差
    }
    p.durationMs = (uint16_t)(2000 / speed);
    return p;
}

OscillatorParams MotionEmotion::makeNod(float intensity, float speed) {
    OscillatorParams p;
    float amp = 20.0f * intensity;
    float freq = 1.5f * speed;
    p.center[0] = 90;    p.amplitude[0] = (int16_t)(amp * 0.5f);
    p.center[1] = 90;    p.amplitude[1] = (int16_t)(amp * 0.5f);
    p.center[2] = 90;    p.amplitude[2] = (int16_t)(-amp * 0.5f);
    p.center[3] = 90;    p.amplitude[3] = (int16_t)(-amp * 0.5f);
    for (int i = 0; i < 4; i++) {
        p.frequency[i] = freq;
        p.phase[i] = 0;
    }
    p.durationMs = (uint16_t)(1500 / speed);
    return p;
}

OscillatorParams MotionEmotion::makeShake(float intensity, float speed) {
    OscillatorParams p;
    float amp = 25.0f * intensity;
    float freq = 3.0f * speed;
    p.center[0] = 90;    p.amplitude[0] = (int16_t)amp;
    p.center[1] = 90;    p.amplitude[1] = (int16_t)(-amp);
    p.center[2] = 90;    p.amplitude[2] = (int16_t)(-amp);
    p.center[3] = 90;    p.amplitude[3] = (int16_t)amp;
    for (int i = 0; i < 4; i++) {
        p.frequency[i] = freq;
        p.phase[i] = (i < 2) ? 0 : 3.14f;  // 前后相反相位
    }
    p.durationMs = (uint16_t)(1200 / speed);
    return p;
}

OscillatorParams MotionEmotion::makeCelebrate(float intensity, float speed) {
    OscillatorParams p;
    float amp = 40.0f * intensity;
    float freq = 4.0f * speed;
    p.center[0] = 90;    p.amplitude[0] = (int16_t)amp;
    p.center[1] = 90;    p.amplitude[1] = (int16_t)amp;
    p.center[2] = 90;    p.amplitude[2] = (int16_t)(amp * 0.3f);
    p.center[3] = 90;    p.amplitude[3] = (int16_t)(amp * 0.3f);
    for (int i = 0; i < 4; i++) {
        p.frequency[i] = freq;
        p.phase[i] = (i % 2 == 0) ? 0 : 3.14f;
    }
    p.durationMs = (uint16_t)(1500 / speed);
    return p;
}

OscillatorParams MotionEmotion::makeComfort(float intensity, float speed) {
    OscillatorParams p;
    float amp = 15.0f * intensity;
    float freq = 0.8f * speed;
    p.center[0] = 90;    p.amplitude[0] = (int16_t)amp;
    p.center[1] = 90;    p.amplitude[1] = (int16_t)(-amp);
    p.center[2] = 90;    p.amplitude[2] = (int16_t)amp;
    p.center[3] = 90;    p.amplitude[3] = (int16_t)(-amp);
    for (int i = 0; i < 4; i++) {
        p.frequency[i] = freq;
        p.phase[i] = i * 1.57f;
    }
    p.durationMs = (uint16_t)(3000 / speed);
    return p;
}

// ==================== 构造函数 ====================
MotionEmotion::MotionEmotion() {
    _s[0] = _s[1] = _s[2] = _s[3] = nullptr;
    _attached = false;
    _isPlaying = false;
    _currentAction = ACTION_NONE;
    _repeatLeft = 0;
    _stepStartMs = 0;
    _stepIndex = 0;
    _parametricMode = false;
    _oscStartMs = 0;
    _intensityScale = 1.0f;
    _speedScale = 1.0f;
    // 初始化振荡器
    for (int i = 0; i < 4; i++) {
        _osc.center[i] = 90;
        _osc.amplitude[i] = 0;
        _osc.frequency[i] = 1.0f;
        _osc.phase[i] = 0;
    }
    _osc.durationMs = 1000;
}

// ==================== 关联舵机 ====================
void MotionEmotion::attach(MindPawServo& s1, MindPawServo& s2, MindPawServo& s3, MindPawServo& s4) {
    _s[0] = &s1; _s[1] = &s2; _s[2] = &s3; _s[3] = &s4;
    _attached = true;
}

// ==================== 触发情感动作 ====================
void MotionEmotion::trigger(EmotionAction action, uint8_t repeat) {
    if (!_attached) return;
    if (repeat == 0) repeat = 1;

    _isPlaying = false;  // 停止当前
    _parametricMode = false;

    // 根据动作类型选择：步进 or 参数化
    switch (action) {
        case ACTION_DANCE:
            _osc = makeDance(_intensityScale, _speedScale);
            _parametricMode = true;
            break;
        case ACTION_NOD:
            _osc = makeNod(_intensityScale, _speedScale);
            _parametricMode = true;
            break;
        case ACTION_SHAKE:
            _osc = makeShake(_intensityScale, _speedScale);
            _parametricMode = true;
            break;
        case ACTION_CELEBRATE:
            _osc = makeCelebrate(_intensityScale, _speedScale);
            _parametricMode = true;
            break;
        case ACTION_COMFORT:
            _osc = makeComfort(_intensityScale, _speedScale);
            _parametricMode = true;
            break;
        case ACTION_BOW:
        case ACTION_STRETCH:
        case ACTION_HIDE_FACE:
            _parametricMode = false;
            break;
        default:
            return;
    }

    _currentAction = action;
    _repeatLeft = repeat;
    _stepIndex = 0;
    _stepStartMs = millis();
    _oscStartMs = millis();
    _isPlaying = true;

    if (_parametricMode) {
        Serial.printf("MOTION: Parametric action %d (intensity=%.1f, speed=%.1f)\n",
                     action, _intensityScale, _speedScale);
    } else {
        Serial.printf("MOTION: Step action %d\n", action);
    }
}

// ==================== 触发参数化动作 ====================
void MotionEmotion::triggerParametric(const OscillatorParams& params, uint8_t repeat) {
    if (!_attached) return;
    _isPlaying = false;
    _osc = params;
    _parametricMode = true;
    _currentAction = ACTION_DANCE;  // 通用标记
    _repeatLeft = (repeat > 0) ? repeat : 1;
    _oscStartMs = _stepStartMs = millis();
    _stepIndex = 0;
    _isPlaying = true;
}

// ==================== 设置情感强度 ====================
void MotionEmotion::setEmotionalIntensity(float pleasure, float arousal) {
    // pleasure [-1,1] → intensity [0.5, 1.5]
    // arousal [0,1] → speed [0.5, 1.5]
    _intensityScale = 0.5f + (pleasure + 1.0f) * 0.5f;
    if (_intensityScale < 0.5f) _intensityScale = 0.5f;
    if (_intensityScale > 1.5f) _intensityScale = 1.5f;

    _speedScale = 0.5f + arousal;
    if (_speedScale < 0.5f) _speedScale = 0.5f;
    if (_speedScale > 1.5f) _speedScale = 1.5f;
}

// ==================== 停止 ====================
void MotionEmotion::stop() {
    _isPlaying = false;
    _parametricMode = false;
    _currentAction = ACTION_NONE;
    _repeatLeft = 0;
}

// ==================== 应用步进 ====================
void MotionEmotion::applyStep(const MotionStep& step) {
    if (!_attached) return;
    if (step.pos1 >= 0 && step.pos1 <= 180) _s[0]->write(step.pos1);
    if (step.pos2 >= 0 && step.pos2 <= 180) _s[1]->write(step.pos2);
    if (step.pos3 >= 0 && step.pos3 <= 180) _s[2]->write(step.pos3);
    if (step.pos4 >= 0 && step.pos4 <= 180) _s[3]->write(step.pos4);
}

// ==================== 应用振荡器 ====================
void MotionEmotion::applyOscillator(unsigned long elapsedMs) {
    if (!_attached) return;
    float t = elapsedMs / 1000.0f;  // 秒
    for (int i = 0; i < 4; i++) {
        float angle = _osc.center[i]
                    + _osc.amplitude[i] * sinf(2.0f * 3.14159f * _osc.frequency[i] * t + _osc.phase[i]);
        // 限幅
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;
        _s[i]->write((uint16_t)angle);
    }
}

// ==================== 获取步进序列 ====================
const MotionEmotion::MotionStep* MotionEmotion::getSteps(EmotionAction action, uint8_t& count) const {
    const MotionStep* steps = nullptr;
    count = 0;
    switch (action) {
        case ACTION_BOW:       steps = _bowSteps;       break;
        case ACTION_STRETCH:   steps = _stretchSteps;   break;
        case ACTION_HIDE_FACE: steps = _hideFaceSteps;  break;
        default: return nullptr;
    }
    while (steps[count].durationMs > 0) count++;
    return steps;
}

// ==================== 主循环 ====================
void MotionEmotion::loop() {
    if (!_isPlaying || !_attached) return;

    unsigned long now = millis();

    if (_parametricMode) {
        // 参数化模式：持续更新正弦振荡
        unsigned long elapsed = now - _oscStartMs;
        if (elapsed >= _osc.durationMs) {
            // 完成一个周期
            if (_repeatLeft > 1) {
                _repeatLeft--;
                _oscStartMs = now;
            } else {
                _isPlaying = false;
                // 回到中立位置
                for (int i = 0; i < 4; i++) _s[i]->write(90);
                return;
            }
        }
        applyOscillator(elapsed);
    } else {
        // 步进模式
        uint8_t stepCount = 0;
        const MotionStep* steps = getSteps(_currentAction, stepCount);
        if (!steps || stepCount == 0) {
            _isPlaying = false;
            return;
        }

        unsigned long elapsed = now - _stepStartMs;
        if (_stepIndex < stepCount && elapsed >= steps[_stepIndex].durationMs) {
            _stepIndex++;
            if (_stepIndex >= stepCount || steps[_stepIndex].durationMs == 0) {
                if (_repeatLeft > 1) {
                    _repeatLeft--;
                    _stepIndex = 0;
                    _stepStartMs = now;
                    applyStep(steps[0]);
                } else {
                    _isPlaying = false;
                }
            } else {
                _stepStartMs = now;
                applyStep(steps[_stepIndex]);
            }
        }
    }
}
