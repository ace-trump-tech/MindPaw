#include "servo_output.h"
#include <Adafruit_PWMServoDriver.h>

namespace {
Adafruit_PWMServoDriver pwm(0x40);
bool attempted = false;
bool ready = false;
}

bool MindPawServo::attach(uint8_t channel, uint16_t minUs, uint16_t maxUs) {
    if (!attempted) {
        attempted = true;
        ready = pwm.begin();
        if (ready) {
            pwm.setPWMFreq(50);
            for (uint8_t i = 0; i < 16; ++i) pwm.setPWM(i, 0, 4096);
        } else {
            Serial.println("PCA9685 missing: servo outputs disabled");
        }
    }
    if (!ready || channel > 3 || minUs >= maxUs) return false;
    _channel = channel;
    _minUs = minUs;
    _maxUs = maxUs;
    return true;
}

void MindPawServo::write(int angle) {
    if (!ready || _channel > 3) return;
    const uint32_t us = _minUs + (uint32_t)constrain(angle, 0, 180) * (_maxUs - _minUs) / 180;
    pwm.setPWM(_channel, 0, (us * 4096UL + 10000UL) / 20000UL);
}
