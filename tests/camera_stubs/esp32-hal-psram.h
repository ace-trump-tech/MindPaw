#pragma once
#include "esp_camera.h"
#include <cstdlib>
inline bool psramFound() { return dvp().psram; }
inline void* ps_malloc(size_t n) { return dvp().allocation ? malloc(n) : nullptr; }
