#pragma once

#include <cstdint>
#include <span>

#include "nordend/picture/image.hpp"

namespace nordend::svg {
// Fit the longest side to pixels, retaining aspect ratio. Straight-alpha RGBA.
// Malformed input, exhausted limits and allocation failures return an empty image.
[[nodiscard]] picture::Image render(std::span<const std::uint8_t> data, int pixels) noexcept;
}  // namespace nordend::svg
