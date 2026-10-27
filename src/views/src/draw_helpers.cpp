#include "draw_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>

#include "nordend/views/folder.hpp"

namespace nordend::views {
namespace {

constexpr float kPadding = 16.0F;
constexpr float kSmallTextSize = 12.0F;

}  // namespace

float text_width(ui::Renderer& renderer, std::string_view text, float size) {
    return renderer.fonts().layout(text, size, renderer.scale()).width / renderer.scale();
}

void draw_centered(ui::Renderer& renderer, std::string_view text, const ui::Rect& bounds, float y,
                   float size, ui::Color color) {
    const float max = bounds.width - 24.0F;
    const float width = std::min(text_width(renderer, text, size), max);
    renderer.draw_text(text, bounds.x + ((bounds.width - width) / 2.0F), y, size, color,
                       std::max(1.0F, max), ui::Ellipsis::middle);
}

void draw_right_aligned(ui::Renderer& renderer, std::string_view text, float right_x, float top_y,
                        float font_size, ui::Color color) {
    renderer.draw_text(text, right_x - text_width(renderer, text, font_size), top_y, font_size,
                       color);
}

void draw_title_bar(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    const Folder& folder) {
    const fs::DirectoryModel& model = folder.model();
    const std::size_t count = folder.entries().size();
    std::string status;
    if (model.error()) {
        status = model.error().message();
    } else if (folder.selected_count() > 1) {
        status = std::format("{} of {} selected", folder.selected_count(), count);
    } else if (model.loading()) {
        status = std::format("{} items, loading…", count);
    } else {
        status = std::format("{} items", count);
    }
    const float title_y = bounds.y + 12.0F;
    const float status_width = text_width(renderer, status, kSmallTextSize);
    draw_right_aligned(renderer, status, bounds.right() - kPadding, title_y + 2.0F, kSmallTextSize,
                       palette.on_surface_variant);
    const fs::Entry* const entry = folder.cursor_entry();
    const std::filesystem::path shown =
        entry != nullptr ? folder.path() / entry->name : folder.path();
    renderer.draw_text(shown.string(), bounds.x + kPadding, title_y, 15.0F, palette.on_surface,
                       bounds.width - (3.0F * kPadding) - status_width, ui::Ellipsis::middle);
    renderer.fill_rect(
        {bounds.x, bounds.y + kTitleHeight - 1.0F, bounds.width, 1.0F / renderer.scale()},
        palette.outline_variant);
}

}  // namespace nordend::views
