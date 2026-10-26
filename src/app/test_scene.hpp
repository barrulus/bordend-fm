#pragma once

#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"

namespace nordend::app {

// Visual check of every renderer primitive (ROADMAP 1.4): solid and fractional-position rects,
// radii, borders, shadows, blending, nested clipping and physical-pixel hairlines. It is the
// window content until the file views exist.
void draw_test_scene(ui::Renderer& renderer, const ui::Rect& bounds);

}  // namespace nordend::app
