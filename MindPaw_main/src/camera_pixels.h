#pragma once
#include <stddef.h>
#include <stdint.h>

namespace CameraPixels {
inline uint8_t gray(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t(r) * 77 + uint16_t(g) * 150 + uint16_t(b) * 29) >> 8;
}
inline uint8_t gray565(uint16_t pixel) {
    uint8_t r = (pixel >> 11) & 31, g = (pixel >> 5) & 63, b = pixel & 31;
    return gray((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
}
// ArduCAM can include padding around JPEG; only expose the complete JPEG.
inline bool jpegRange(const uint8_t* data, size_t len, size_t& start, size_t& count) {
    start = count = 0;
    if (!data || len < 4) return false;
    for (size_t i = 0; i + 1 < len; ++i) {
        if (data[i] != 0xff || data[i + 1] != 0xd8) continue;
        for (size_t j = i + 2; j + 1 < len; ++j) {
            if (data[j] == 0xff && data[j + 1] == 0xd9) {
                start = i; count = j + 2 - i;
                return true;
            }
        }
        return false;
    }
    return false;
}
}
