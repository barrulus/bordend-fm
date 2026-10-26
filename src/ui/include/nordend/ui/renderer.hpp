#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "nordend/ui/color.hpp"
#include "nordend/ui/geometry.hpp"

namespace nordend::ui {

struct BoxStyle {
    Color fill{};
    Corners radius{};
    float border_width{0.0F};  // drawn inside the box
    Color border{};
};

// Batched 2D renderer for one GL context. Coordinates are logical pixels; the renderer maps
// them to physical pixels with the frame's scale and snaps box edges to the pixel grid so
// they stay sharp at fractional scales.
//
// Usage per frame: begin_frame, any number of draw/clip calls, end_frame. Draw order is
// paint order. Requires the GL context to be current for every call, including construction
// and destruction. Throws std::runtime_error if the shaders fail to build.
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void begin_frame(std::int32_t buffer_width, std::int32_t buffer_height, float scale,
                     Color clear);
    void end_frame();

    [[nodiscard]] float scale() const noexcept { return scale_; }

    void draw_box(const Rect& rect, const BoxStyle& style);

    void fill_rect(const Rect& rect, Color color) { draw_box(rect, {.fill = color}); }

    // Soft shadow of a rounded box; blur is the Gaussian sigma in logical pixels. Offset the
    // rect to move the shadow.
    void draw_shadow(const Rect& rect, const Corners& radius, float blur, Color color);

    // Restricts drawing to rect, intersected with the current clip, until pop_clip.
    void push_clip(const Rect& rect);
    void pop_clip();

private:
    struct Gl;
    struct Instance;

    void flush();
    void apply_clip();

    std::unique_ptr<Gl> gl_;
    std::vector<Instance> instances_;
    std::vector<Rect> clips_;  // logical; empty = whole buffer
    std::int32_t buffer_width_{0};
    std::int32_t buffer_height_{0};
    float scale_{1.0F};
};

}  // namespace nordend::ui
