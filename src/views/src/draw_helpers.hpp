#pragma once

// Small text and chrome helpers shared by the views.

#include <string_view>

#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"

namespace nordend::views {

class Folder;

// Height of the title bar the views draw above their content.
inline constexpr float kTitleHeight = 40.0F;

// Width of text in logical pixels.
float text_width(ui::Renderer& renderer, std::string_view text, float size);

// Text centered in bounds (with 12 px margins, ellipsized when longer), its top at y.
void draw_centered(ui::Renderer& renderer, std::string_view text, const ui::Rect& bounds, float y,
                   float size, ui::Color color);

void draw_right_aligned(ui::Renderer& renderer, std::string_view text, float right_x, float top_y,
                        float font_size, ui::Color color);

// The title bar at the top of bounds: the path of folder's cursor entry (or of folder) and a
// status ("12 items", "3 of 12 selected", the folder's error), with a line under it.
void draw_title_bar(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    const Folder& folder);

}  // namespace nordend::views
