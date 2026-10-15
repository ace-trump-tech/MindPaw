#pragma once
#include "SPI.h"
#define OV2640 1
#define JPEG 1
#define BMP 0
#define OV2640_160x120 0
#define OV2640_CHIPID_HIGH 0x0a
#define OV2640_CHIPID_LOW 0x0b
#define ARDUCHIP_TEST1 0
#define ARDUCHIP_TRIG 1
#define CAP_DONE_MASK 1
struct ArduCAM {
    ArduCAM(int, int) {}
    void set_format(int format) { fixture().format = format; }
    void InitCAM() { ++fixture().initCalls; }
    void OV2640_set_JPEG_size(int) {}
    void write_reg(int, int) {}
    uint8_t read_reg(int) { return fixture().spiOk ? 0x55 : 0; }
    // Upstream returns 1 on success, NOT the Wire/ESP-IDF convention of 0.
    uint8_t wrSensorReg8_8(int, int) { return fixture().sensorOk ? 1 : 0; }
    uint8_t rdSensorReg8_8(int reg, uint8_t* value) {
        *value = fixture().sensorOk ? (reg == OV2640_CHIPID_HIGH ? 0x26 : 0x42) : 0;
        return 1;
    }
    void flush_fifo() { fixture().offset = 0; }
    void clear_fifo_flag() {}
    void start_capture() {}
    bool get_bit(int, int) { return fixture().captureDone; }
    size_t read_fifo_length() { return fixture().bytes.size(); }
    void CS_LOW() { assert(!fixture().selected); fixture().selected = true; }
    void CS_HIGH() { assert(fixture().selected); fixture().selected = false; }
    void set_fifo_burst() { assert(fixture().selected); }
};
