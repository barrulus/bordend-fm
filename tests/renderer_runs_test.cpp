#include <GLES3/gl3.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/ui/color.hpp"
#include "nordend/ui/offscreen.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"

namespace ui = nordend::ui;

namespace {

constexpr std::int32_t kWidth = 240;
constexpr std::int32_t kHeight = 40;

std::unique_ptr<ui::Offscreen> offscreen() {
    try {
        return std::make_unique<ui::Offscreen>(kWidth, kHeight);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return nullptr;
    }
}

}  // namespace

TEST_CASE("renderer: a line in several colours, by the bytes each glyph comes from") {
    const std::unique_ptr<ui::Offscreen> gl = offscreen();
    if (!gl) {
        return;
    }
    ui::FontSystem fonts;
    ui::Renderer renderer{fonts};
    renderer.begin_frame(kWidth, kHeight, 1.0F, ui::Color::rgb(0x000000));
    // "MMMM" white, "éééé" (two bytes each: bytes 4 to 12) red.
    const std::array runs{ui::ColorRun{.start = 4, .end = 12, .color = ui::Color::rgb(0xFF0000)}};
    const float width =
        renderer.draw_text_runs("MMMMéééé", 4.0F, 4.0F, 24.0F, ui::Color::rgb(0xFFFFFF), runs, 0.0F,
                                ui::Ellipsis::end, ui::Typeface::monospace);
    renderer.end_frame();
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth) * kHeight * 4);
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Monospace: the colour changes halfway along the text.
    const float half = 4.0F + (width / 2.0F);
    int white_left = 0;
    int white_right = 0;
    int red_left = 0;
    int red_right = 0;
    for (std::int32_t y = 0; y < kHeight; ++y) {
        for (std::int32_t x = 0; x < kWidth; ++x) {
            const std::size_t at =
                ((static_cast<std::size_t>(y) * kWidth) + static_cast<std::size_t>(x)) * 4;
            const bool white = pixels[at] > 128 && pixels[at + 1] > 128;
            const bool red = pixels[at] > 128 && pixels[at + 1] < 64;
            const bool left = static_cast<float>(x) < half - 2.0F;
            const bool right = static_cast<float>(x) > half + 2.0F;
            white_left += white && left ? 1 : 0;
            white_right += white && right ? 1 : 0;
            red_left += red && left ? 1 : 0;
            red_right += red && right ? 1 : 0;
        }
    }
    CHECK(white_left > 50);
    CHECK(red_right > 50);
    CHECK(white_right == 0);
    CHECK(red_left == 0);
}
