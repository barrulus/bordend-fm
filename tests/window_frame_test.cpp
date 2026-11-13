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
#include "nordend/ui/window.hpp"

namespace ui = nordend::ui;
using ui::WindowFrame;

TEST_CASE("window frame: only around a window drawing its own decorations, free on every side") {
    CHECK(WindowFrame::margin({}, true, true) == WindowFrame::kMargin);
    CHECK(WindowFrame::margin({.activated = false}, true, true) == WindowFrame::kMargin);
    CHECK(WindowFrame::margin({}, false, true) == 0);  // the compositor's decorations
    CHECK(WindowFrame::margin({}, true, false) == 0);  // not asked for
    CHECK(WindowFrame::margin({.maximized = true}, true, true) == 0);
    CHECK(WindowFrame::margin({.fullscreen = true}, true, true) == 0);
    CHECK(WindowFrame::margin({.tiled_left = true}, true, true) == 0);
    CHECK(WindowFrame::margin({.tiled_bottom = true}, true, true) == 0);
}

namespace {

constexpr std::int32_t kWidth = 200;
constexpr std::int32_t kHeight = 140;
constexpr float kMargin = 24.0F;

struct Pixels {
    std::vector<std::uint8_t> rgba = std::vector<std::uint8_t>(
        static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight) * 4);

    // (x, y) from the top-left, as the window is laid out.
    [[nodiscard]] std::array<std::uint8_t, 4> at(std::int32_t x, std::int32_t y) const {
        const std::size_t i =
            ((static_cast<std::size_t>(kHeight - 1 - y) * kWidth) + static_cast<std::size_t>(x)) *
            4;
        return {rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]};
    }
};

std::unique_ptr<ui::Offscreen> offscreen() {
    try {
        return std::make_unique<ui::Offscreen>(kWidth, kHeight);
    } catch (const std::runtime_error& error) {
        MESSAGE("no offscreen OpenGL: " << error.what());
        return nullptr;
    }
}

}  // namespace

TEST_CASE("window frame: the content inside the margin, rounded, a shadow behind") {
    const std::unique_ptr<ui::Offscreen> gl = offscreen();
    if (!gl) {
        return;
    }
    ui::FontSystem fonts;
    ui::Renderer renderer{fonts};
    renderer.begin_frame(kWidth, kHeight, 1.0F, ui::Color::rgb(0xFF0000), kMargin);
    // (0, 0) is the content's top-left: a green square there lands inside the margin.
    renderer.fill_rect({20.0F, 20.0F, 10.0F, 10.0F}, ui::Color::rgb(0x00FF00));
    // A clip is the content's too.
    renderer.push_clip({0.0F, 50.0F, 4.0F, 4.0F});
    renderer.fill_rect({0.0F, 0.0F, 200.0F, 200.0F}, ui::Color::rgb(0x0000FF));
    renderer.pop_clip();
    WindowFrame::draw(renderer, true);
    renderer.end_frame();
    Pixels pixels;
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.rgba.data());

    const auto m = static_cast<std::int32_t>(kMargin);
    // The content: opaque, red, with the square and the clipped fill moved by the margin.
    CHECK(pixels.at(m + 40, m + 40) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    CHECK(pixels.at(m + 25, m + 25) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixels.at(m + 2, m + 52) == std::array<std::uint8_t, 4>{0, 0, 255, 255});
    CHECK(pixels.at(m + 2, m + 56) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    // Mid-edge, the content keeps its very first pixel.
    CHECK(pixels.at(m, kHeight / 2) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    // The rounded corner: its very corner pixel is no longer red, but shadow.
    const auto corner = pixels.at(m, m);
    CHECK(corner[0] < 40);
    CHECK(corner[3] < 128);
    // Outside the edge: the outline and the shadow, dark and translucent.
    const auto outside = pixels.at(m - 1, kHeight / 2);
    CHECK(outside[3] > 30);
    CHECK(outside[3] < 255);
    CHECK(outside[0] == outside[2]);  // black, premultiplied: grey levels only
    // Below the window the shadow falls further than above it.
    CHECK(pixels.at(kWidth / 2, kHeight - m + 6)[3] > pixels.at(kWidth / 2, m - 6)[3]);
    // The buffer's own corner: clear.
    CHECK(pixels.at(0, 0)[3] == 0);
}

TEST_CASE("window frame: none without a margin") {
    const std::unique_ptr<ui::Offscreen> gl = offscreen();
    if (!gl) {
        return;
    }
    ui::FontSystem fonts;
    ui::Renderer renderer{fonts};
    renderer.begin_frame(kWidth, kHeight, 1.0F, ui::Color::rgb(0xFF0000));
    WindowFrame::draw(renderer, true);
    renderer.end_frame();
    Pixels pixels;
    glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.rgba.data());
    CHECK(pixels.at(0, 0) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    CHECK(pixels.at(kWidth - 1, kHeight - 1) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
}
