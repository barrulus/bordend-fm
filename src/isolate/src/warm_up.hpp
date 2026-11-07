#pragma once

// The pictures the helper decodes before closing its cage (generated from the test fixtures by
// src/isolate/CMakeLists.txt).

#include <cstddef>

namespace nordend::isolate {

struct WarmUpPicture {
    const char* name;
    const unsigned char* data;
    std::size_t size;
};

extern const WarmUpPicture kWarmUp[];  // NOLINT(*-avoid-c-arrays): generated
extern const std::size_t kWarmUpCount;

}  // namespace nordend::isolate
