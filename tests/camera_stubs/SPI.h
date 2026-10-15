#pragma once
#include <vector>
#include <cassert>
#define SPI_MODE0 0
#define MSBFIRST 1
struct CameraFixture {
    bool spiOk = true, sensorOk = true, captureDone = true, selected = false;
    int initCalls = 0, format = 1;
    size_t offset = 0;
    std::vector<uint8_t> bytes;
};
inline CameraFixture& fixture() { static CameraFixture value; return value; }
struct SPIStub {
    void begin() {}
    void setFrequency(int) {}
    void setDataMode(int) {}
    void setBitOrder(int) {}
    uint8_t transfer(int) {
        auto& f = fixture();
        assert(f.selected && f.offset < f.bytes.size());
        return f.bytes[f.offset++];
    }
};
static SPIStub SPI;
