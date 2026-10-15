#pragma once

// Different camera buses require different wiring. Both profiles use PCA9685.
#if defined(ESP8266)
#define MINDPAW_CAMERA_SPI 1
#define MP_SDA 4
#define MP_SCL 5
#define MP_VOICE_RX 0
#define MP_SPEAKER 16
#define MP_BUTTON 2
#define MP_BATTERY_ADC A0
#define MP_ISR_ATTR ICACHE_RAM_ATTR
#define OV2640_CS 15
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define MINDPAW_CAMERA_DVP 1
#define MP_SDA 1
#define MP_SCL 2
#define MP_VOICE_RX 47
#define MP_SPEAKER 14
#define MP_BUTTON 38
#define MP_BATTERY_ADC 10
#define MP_ISR_ATTR IRAM_ATTR
#define OV2640_CS 255 // Unused by DVP.
// ESP32-S3-DevKitC-1 N8R8 with external 3.3V-compatible DVP breakout.
#define MP_CAM_D0 4
#define MP_CAM_D1 5
#define MP_CAM_D2 6
#define MP_CAM_D3 7
#define MP_CAM_D4 8
#define MP_CAM_D5 9
#define MP_CAM_D6 12
#define MP_CAM_D7 11
#define MP_CAM_PCLK 13
#define MP_CAM_XCLK 15
#define MP_CAM_VSYNC 16
#define MP_CAM_HREF 17
#define MP_CAM_SDA 18
#define MP_CAM_SCL 21
#define MP_CAM_PWDN -1 // Tie module PWDN low.
#define MP_CAM_RESET -1 // Tie module RESET high (3.3V logic).
#else
#error "Select nodemcuv2 or esp32s3_dvp in platformio.ini"
#endif
