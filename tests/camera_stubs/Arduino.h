#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#define OUTPUT 1
#define HIGH 1
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline unsigned long& fakeClock() { static unsigned long now = 0; return now; }
inline unsigned long millis() { return fakeClock(); }
inline void delay(unsigned long ms) { fakeClock() += ms; }
inline void yield() {}
struct SerialStub {
    void println(const char*) {}
    template<typename... Args> void printf(const char*, Args...) {}
};
static SerialStub Serial;
