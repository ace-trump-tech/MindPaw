// Driver contract simulation: frame lifetime, allocation failures, BGR conversion.
#include "../MindPaw_main/src/ov2640.h"
#include "camera_stubs/esp_camera.h"
#include <cassert>
int main() {
    dvp().psram = false;
    OV2640_Camera noRam;
    assert(!noRam.begin());
    dvp() = DvpFixture{};
    dvp().initError = 1;
    OV2640_Camera absent;
    assert(!absent.begin());
    dvp() = DvpFixture{};
    dvp().sensor.id.PID = 0;
    OV2640_Camera wrong;
    assert(!wrong.begin() && dvp().deinitialized == 1);
    dvp() = DvpFixture{};
    OV2640_Camera camera;
    assert(camera.begin());
    uint8_t jpeg[] = {0xff, 0xd8, 0xff, 0xd9};
    dvp().frame = {jpeg, sizeof jpeg, 160, 120, PIXFORMAT_JPEG};
    uint8_t out[1202] = {};
    size_t len = 99;
    assert(!camera.captureJpeg(out, 2, len) && len == 0 && dvp().returned == 1);
    assert(camera.captureJpeg(out, sizeof out, len) && len == 4 && dvp().returned == 2);
    dvp().allocation = false;
    assert(!camera.captureGrayscale(out) && dvp().returned == 3);
    dvp().allocation = true; dvp().decode = false;
    assert(!camera.captureGrayscale(out) && dvp().returned == 4);
    dvp().decode = true;
    out[0] = 17; out[1201] = 19;
    assert(camera.captureGrayscale(out + 1) && dvp().returned == 5);
    assert(out[0] == 17 && out[1201] == 19);
    for (int i = 1; i <= 1200; ++i) assert(out[i] == 76);
    dvp().frame.width = 320;
    assert(!camera.captureGrayscale(out) && dvp().returned == 6);
    dvp().hasFrame = false;
    assert(!camera.captureJpeg(out, sizeof out, len) && len == 0 && dvp().returned == 6);
}
