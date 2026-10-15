#pragma once
#include <cstdint>
#include <cstddef>
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0
#define PIXFORMAT_JPEG 1
#define FRAMESIZE_QQVGA 0
#define CAMERA_FB_IN_PSRAM 1
#define CAMERA_GRAB_WHEN_EMPTY 0
#define OV2640_PID 0x2640
#define ESP_OK 0
using esp_err_t = int;
struct camera_config_t {
    int ledc_channel, ledc_timer;
    int pin_d0, pin_d1, pin_d2, pin_d3, pin_d4, pin_d5, pin_d6, pin_d7;
    int pin_xclk, pin_pclk, pin_vsync, pin_href, pin_sccb_sda, pin_sccb_scl;
    int pin_pwdn, pin_reset, xclk_freq_hz, pixel_format, frame_size;
    int jpeg_quality, fb_count, fb_location, grab_mode;
};
struct camera_fb_t {
    uint8_t* buf;
    size_t len, width, height;
    int format;
};
struct sensor_t { struct { int PID; } id; };
struct DvpFixture {
    bool psram = true, allocation = true, decode = true, hasFrame = true;
    int initError = 0, returned = 0, deinitialized = 0;
    sensor_t sensor{{OV2640_PID}};
    camera_fb_t frame{};
};
inline DvpFixture& dvp() { static DvpFixture f; return f; }
inline int esp_camera_init(camera_config_t*) { return dvp().initError; }
inline void esp_camera_deinit() { ++dvp().deinitialized; }
inline sensor_t* esp_camera_sensor_get() { return &dvp().sensor; }
inline camera_fb_t* esp_camera_fb_get() { return dvp().hasFrame ? &dvp().frame : nullptr; }
inline void esp_camera_fb_return(camera_fb_t*) { ++dvp().returned; }
