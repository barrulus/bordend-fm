#pragma once

#include <cstdint>

#include "nordend/image/decode.hpp"

namespace nordend::image {

// The image scaled down to fit in max_width x max_height, proportions kept, each output pixel
// the average of the area of input it covers (alpha weighted, so transparent pixels do not
// darken the edges). An image that already fits is returned as it is; it is never enlarged.
[[nodiscard]] Image scale_to_fit(const Image& image, std::uint32_t max_width,
                                 std::uint32_t max_height);

}  // namespace nordend::image
