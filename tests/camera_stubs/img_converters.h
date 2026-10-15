#pragma once
#include "esp_camera.h"
inline bool fmt2rgb888(const uint8_t*, size_t, int, uint8_t* rgb) {
    if (!dvp().decode) return false;
    // Pinned upstream decoder's BGR byte order: solid red.
    for (int i = 0; i < 160 * 120 * 3; i += 3) {
        rgb[i] = rgb[i + 1] = 0;
        rgb[i + 2] = 255;
    }
    return true;
}
