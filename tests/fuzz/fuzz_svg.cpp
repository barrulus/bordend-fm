#include <cstddef>
#include <cstdint>

#include "nordend/svg/svg.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // Keep seeds as literal SVGs. Larger sizes remain covered by unit/corpus tests.
    static_cast<void>(nordend::svg::render({data, size}, 16));
    return 0;
}
