// Driver-level tests against a fake SPI/FIFO, not a claim of physical testing.
#include "../MindPaw_main/src/ov2640.h"
#include <cassert>
#include <climits>

int main() {
    fixture().spiOk = false;
    OV2640_Camera absent;
    assert(!absent.begin() && !absent.isAvailable());
    fixture() = CameraFixture{};
    fixture().sensorOk = false;
    OV2640_Camera wrongSensor;
    assert(!wrongSensor.begin());
    fixture() = CameraFixture{};
    OV2640_Camera camera;
    assert(camera.begin() && camera.isAvailable());
    uint8_t out[1202] = {};
    size_t len = 99;
    fixture().bytes = {0, 0xff, 0xd8, 0x12, 0xff, 0xd9, 0};
    assert(!camera.captureJpeg(out, 2, len) && len == 0);
    assert(!fixture().selected);
    assert(camera.captureJpeg(out, sizeof out, len) && len == 5);
    assert(out[0] == 0xff && out[1] == 0xd8 && out[4] == 0xd9);
    fixture().captureDone = false;
    fakeClock() = ULONG_MAX - 20; // Timeout must work across millis wraparound.
    assert(!camera.captureJpeg(out, sizeof out, len) && len == 0);
    fixture().captureDone = true;
    fixture().bytes.assign(320 * 240 * 2, 0xff);
    out[0] = 17; out[1201] = 19;
    assert(camera.captureGrayscale(out + 1));
    assert(out[0] == 17 && out[1201] == 19);
    for (int i = 1; i <= 1200; ++i) assert(out[i] == 255);
    assert(fixture().offset == fixture().bytes.size() && !fixture().selected);
    const int calls = fixture().initCalls;
    fixture().bytes = {0xff, 0xd8, 0xff, 0xd9};
    assert(camera.captureJpeg(out, sizeof out, len));
    assert(fixture().initCalls == calls + 1 && fixture().format == JPEG);
    fixture().bytes = {0xff, 0xd8, 1, 2};
    assert(!camera.captureJpeg(out, sizeof out, len) && len == 0);
}
