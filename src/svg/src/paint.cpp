#include <algorithm>
#include <cmath>

#include "internal.hpp"

namespace nordend::svg {
void composite(Canvas& canvas, int x, int y, int length, unsigned coverage, Colour paint) noexcept {
    if (y < 0 || y >= static_cast<int>(canvas.image.height)) {
        return;
    }
    const int end = std::min(x + length, static_cast<int>(canvas.image.width));
    x = std::max(x, 0);
    if (end <= x || canvas.exhausted) {
        return;
    }
    canvas.work += static_cast<std::size_t>(end - x);
    if (canvas.work > 64U * 1024U * 1024U) {
        canvas.exhausted = true;
        return;
    }
    const double alpha = paint[3] * static_cast<double>(coverage) / 255;
    for (; x < end; ++x) {
        const auto at =
            (static_cast<std::size_t>(y) * canvas.image.width + static_cast<unsigned>(x)) * 4;
        for (std::size_t c = 0; c < 4; ++c) {
            const double source = c == 3 ? alpha : paint[c] * alpha;
            canvas.image.rgba[at + c] = static_cast<std::uint8_t>(std::clamp(
                std::lround(source * 255 + canvas.image.rgba[at + c] * (1 - alpha)), 0L, 255L));
        }
    }
}

void straighten(picture::Image& image) {
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        const unsigned alpha = image.rgba[i + 3];
        if (alpha == 0) {
            continue;
        }
        for (std::size_t c = 0; c < 3; ++c) {
            image.rgba[i + c] = static_cast<std::uint8_t>(
                std::min(255U, (image.rgba[i + c] * 255U + alpha / 2U) / alpha));
        }
    }
}
}  // namespace nordend::svg
