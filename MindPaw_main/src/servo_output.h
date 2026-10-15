#pragma once
#include <Arduino.h>

// attach() takes a PCA9685 channel, NOT a microcontroller GPIO.
// A missing driver leaves outputs disabled, never falls back onto camera pins.
class MindPawServo {
public:
    bool attach(uint8_t channel, uint16_t minUs = 500, uint16_t maxUs = 2500);
    void write(int angle);
private:
    uint8_t _channel = 255;
    uint16_t _minUs = 500, _maxUs = 2500;
};
