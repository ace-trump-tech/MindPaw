#pragma once
#include <Arduino.h>
#include "board_config.h"
#if defined(MINDPAW_CAMERA_SPI)
#include <ArduCAM.h>
#include <SPI.h>
#endif

#define CAM_GRAY_W 40
#define CAM_GRAY_H 30
#define CAM_JPEG_W 160
#define CAM_JPEG_H 120

// Common API: SPI/FIFO on ESP8266, DVP on ESP32-S3. Call from loop only.
class OV2640_Camera {
public:
    explicit OV2640_Camera(uint8_t csPin = OV2640_CS) : _cs(csPin) {}
    bool begin();
    // Caller owns buffers: grayscale >=1200 bytes; JPEG is bounded by bufLen.
    bool captureGrayscale(uint8_t* buffer);
    bool captureJpeg(uint8_t* buffer, size_t bufLen, size_t& outLen);
    void setInterval(unsigned long ms) { _intervalMs = ms; }
    bool isAvailable() const { return _available; }
    unsigned long getLastCaptureMs() const { return _lastCaptureMs; }
    unsigned long getIntervalMs() const { return _intervalMs; }
    void setDebug(bool debug) { _debug = debug; }
private:
    uint8_t _cs;
    bool _available = false;
    bool _debug = false;
    unsigned long _lastCaptureMs = 0;
    unsigned long _intervalMs = 500;
#if defined(MINDPAW_CAMERA_SPI)
    ArduCAM _cam{OV2640, OV2640_CS};
    bool _jpegMode = true;
    void setFormat(bool jpeg);
    bool captureRawFrame();
#endif
};
