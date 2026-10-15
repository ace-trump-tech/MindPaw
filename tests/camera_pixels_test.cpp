#include "../MindPaw_main/src/camera_pixels.h"
#include <cassert>

int main() {
    using namespace CameraPixels;
    assert(gray565(0) == 0);
    assert(gray565(0xffff) == 255);
    assert(gray565(0xf800) == gray(255, 0, 0));
    assert(gray565(0x07e0) == gray(0, 255, 0));
    assert(gray565(0x001f) == gray(0, 0, 255));
    size_t start = 99, count = 99;
    assert(!jpegRange(nullptr, 12, start, count) && count == 0);
    const uint8_t padded[] = {0, 0xff, 0xd8, 1, 0xff, 0x00, 2, 0xff, 0xd9, 0};
    assert(jpegRange(padded, sizeof padded, start, count));
    assert(start == 1 && count == 8);
    assert(!jpegRange(padded, 8, start, count) && count == 0);
    const uint8_t missingStart[] = {0xff, 0xd9, 0, 0};
    assert(!jpegRange(missingStart, sizeof missingStart, start, count));
    const uint8_t tiny[] = {0xff, 0xd8, 0xff, 0xd9};
    assert(jpegRange(tiny, sizeof tiny, start, count) && start == 0 && count == 4);
}
