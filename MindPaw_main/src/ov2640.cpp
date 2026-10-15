#include "ov2640.h"
#include "camera_pixels.h"
#include <string.h>

#if defined(MINDPAW_CAMERA_SPI)
#include <Wire.h>

void OV2640_Camera::setFormat(bool jpeg) {
    _cam.set_format(jpeg ? JPEG : BMP);
    // set_format only updates a library flag; InitCAM programs the sensor.
    _cam.InitCAM();
    if (jpeg) _cam.OV2640_set_JPEG_size(OV2640_160x120);
    _jpegMode = jpeg; // BMP initialization produces 320x240 RGB565.
    delay(100);
}

bool OV2640_Camera::begin() {
    if (_available) return true;
    // The fixed board profile owns the chip-select pin.
    if (_cs != OV2640_CS) return false;
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    Wire.begin(MP_SDA, MP_SCL);
    SPI.begin();
    SPI.setFrequency(4000000);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    _cam.write_reg(0x07, 0x80);
    delay(100);
    _cam.write_reg(0x07, 0x00);
    delay(100);
    _cam.write_reg(ARDUCHIP_TEST1, 0x55);
    if (_cam.read_reg(ARDUCHIP_TEST1) != 0x55) {
        Serial.println("OV2640: SPI self-test failed; check ArduCAM/FIFO wiring");
        return false;
    }
    uint8_t vid = 0, pid = 0;
    if (_cam.wrSensorReg8_8(0xff, 0x01) != 1 ||
        _cam.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid) != 1 ||
        _cam.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid) != 1 ||
        vid != 0x26 || (pid != 0x41 && pid != 0x42)) {
        Serial.println("OV2640: sensor ID check failed; check SDA/SCL");
        return false;
    }
    setFormat(true);
    _available = true;
    Serial.println("OV2640: ArduCAM SPI/FIFO backend ready");
    return true;
}

bool OV2640_Camera::captureRawFrame() {
    _cam.flush_fifo();
    _cam.clear_fifo_flag();
    _cam.start_capture();
    const unsigned long started = millis();
    while (!_cam.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
        if (millis() - started >= 3000) return false;
        delay(2);
    }
    return true;
}

bool OV2640_Camera::captureGrayscale(uint8_t* buffer) {
    if (!_available || !buffer) return false;
    if (_jpegMode) setFormat(false);
    if (!captureRawFrame()) return false;
    constexpr int width = 320, height = 240;
    const size_t len = _cam.read_fifo_length();
    if (len < width * height * 2 || len >= 0x7fffff) return false;
    _cam.CS_LOW();
    _cam.set_fifo_burst();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Continuous SPI data reads: read_fifo() would toggle CS per byte.
            uint16_t pixel = uint16_t(SPI.transfer(0)) << 8;
            pixel |= SPI.transfer(0);
            if (y % 8 == 0 && x % 8 == 0)
                buffer[(y / 8) * CAM_GRAY_W + x / 8] = CameraPixels::gray565(pixel);
        }
        yield();
    }
    _cam.CS_HIGH();
    _lastCaptureMs = millis();
    return true;
}

bool OV2640_Camera::captureJpeg(uint8_t* buffer, size_t bufLen, size_t& outLen) {
    outLen = 0;
    if (!_available || !buffer) return false;
    if (!_jpegMode) setFormat(true);
    if (!captureRawFrame()) return false;
    const size_t len = _cam.read_fifo_length();
    if (!len || len >= 0x7fffff || len > bufLen) return false;
    _cam.CS_LOW();
    _cam.set_fifo_burst();
    for (size_t i = 0; i < len; ++i) {
        buffer[i] = SPI.transfer(0);
        if ((i & 1023) == 0) yield();
    }
    _cam.CS_HIGH();
    size_t start = 0;
    if (!CameraPixels::jpegRange(buffer, len, start, outLen)) return false;
    if (start) memmove(buffer, buffer + start, outLen);
    _lastCaptureMs = millis();
    return true;
}

#else
#include <esp_camera.h>
#include <img_converters.h>
#include <esp32-hal-psram.h>

bool OV2640_Camera::begin() {
    if (_available) return true;
    if (!psramFound()) {
        Serial.println("OV2640: N8R8 profile requires enabled OPI PSRAM");
        return false;
    }
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = MP_CAM_D0; config.pin_d1 = MP_CAM_D1;
    config.pin_d2 = MP_CAM_D2; config.pin_d3 = MP_CAM_D3;
    config.pin_d4 = MP_CAM_D4; config.pin_d5 = MP_CAM_D5;
    config.pin_d6 = MP_CAM_D6; config.pin_d7 = MP_CAM_D7;
    config.pin_xclk = MP_CAM_XCLK; config.pin_pclk = MP_CAM_PCLK;
    config.pin_vsync = MP_CAM_VSYNC; config.pin_href = MP_CAM_HREF;
    config.pin_sccb_sda = MP_CAM_SDA; config.pin_sccb_scl = MP_CAM_SCL;
    config.pin_pwdn = MP_CAM_PWDN; config.pin_reset = MP_CAM_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    const esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("OV2640: DVP init failed (0x%x)\n", unsigned(err));
        return false;
    }
    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor || sensor->id.PID != OV2640_PID) {
        esp_camera_deinit();
        return false;
    }
    _available = true;
    Serial.println("OV2640: ESP32-S3 DVP backend ready");
    return true;
}

bool OV2640_Camera::captureJpeg(uint8_t* buffer, size_t bufLen, size_t& outLen) {
    outLen = 0;
    if (!_available || !buffer) return false;
    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) return false;
    size_t start = 0, count = 0;
    const bool ok = frame->format == PIXFORMAT_JPEG &&
        CameraPixels::jpegRange(frame->buf, frame->len, start, count) && count <= bufLen;
    if (ok) {
        memcpy(buffer, frame->buf + start, count);
        outLen = count;
        _lastCaptureMs = millis();
    }
    esp_camera_fb_return(frame);
    return ok;
}

bool OV2640_Camera::captureGrayscale(uint8_t* buffer) {
    if (!_available || !buffer) return false;
    camera_fb_t* frame = esp_camera_fb_get();
    if (!frame) return false;
    bool ok = false;
    // Capture JPEG for both consumers; decode without reconfiguring camera DMA.
    if (frame->format == PIXFORMAT_JPEG && frame->width == CAM_JPEG_W && frame->height == CAM_JPEG_H) {
        uint8_t* rgb = static_cast<uint8_t*>(ps_malloc(CAM_JPEG_W * CAM_JPEG_H * 3));
        if (rgb) {
            ok = fmt2rgb888(frame->buf, frame->len, frame->format, rgb);
            if (ok) {
                for (int y = 0; y < CAM_GRAY_H; ++y) {
                    for (int x = 0; x < CAM_GRAY_W; ++x) {
                        const size_t i = (y * 4 * CAM_JPEG_W + x * 4) * 3;
                        // esp32-camera fmt2rgb888 outputs B,G,R byte order.
                        buffer[y * CAM_GRAY_W + x] = CameraPixels::gray(rgb[i + 2], rgb[i + 1], rgb[i]);
                    }
                }
                _lastCaptureMs = millis();
            }
            free(rgb);
        }
    }
    esp_camera_fb_return(frame);
    return ok;
}
#endif
