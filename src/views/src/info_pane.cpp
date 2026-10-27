#include "info_pane.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>

#include "draw_helpers.hpp"
#include "nordend/fs/icon.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr float kPadding = 16.0F;
constexpr float kSmallTextSize = 12.0F;
constexpr float kIconSize = 64.0F;

// "1 file", "3 files".
std::string count_of(std::size_t count, std::string_view noun) {
    return std::format("{} {}{}", count, noun, count == 1 ? "" : "s");
}

}  // namespace

void draw_info_pane(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    ui::IconSet& icons, const Folder& folder) {
    const fs::MimeDatabase& mime = folder.mime();
    const float width = std::min(bounds.width, kInfoPaneWidth);  // content stays at the left
    float y = bounds.y + 32.0F;
    const auto icon = [&](std::string_view name, ui::Color color) {
        icons.draw(renderer, name, bounds.x + ((width - kIconSize) / 2.0F), y, kIconSize, color);
        y += kIconSize + 20.0F;
    };
    const auto centered = [&](std::string_view text, float size, ui::Color color) {
        const float max = width - (2.0F * kPadding);
        const float w = std::min(text_width(renderer, text, size), max);
        renderer.draw_text(text, bounds.x + ((width - w) / 2.0F), y, size, color, max,
                           ui::Ellipsis::middle);
    };
    const auto separator = [&] {
        renderer.fill_rect(
            {bounds.x + kPadding, y, width - (2.0F * kPadding), 1.0F / renderer.scale()},
            palette.outline_variant);
        y += 12.0F;
    };
    const auto row = [&](std::string_view label, const std::string& value) {
        renderer.draw_text(label, bounds.x + kPadding, y, 13.0F, palette.on_surface_variant);
        draw_right_aligned(renderer, value, bounds.x + width - kPadding, y, 13.0F,
                           palette.on_surface);
        y += 24.0F;
    };

    const fs::Entry* const single = folder.single_selection();
    if (single == nullptr && folder.selected_count() > 1) {
        // Several entries: how many, of which kind, and how big.
        const SelectionSummary& summary = folder.summary();
        icon("files", palette.on_surface_variant);
        centered(count_of(summary.folders + summary.files, "item"), 15.0F, palette.on_surface);
        y += 24.0F;
        std::string kinds;
        if (summary.folders > 0) {
            kinds = count_of(summary.folders, "folder");
        }
        if (summary.files > 0) {
            kinds += (kinds.empty() ? "" : ", ") + count_of(summary.files, "file");
        }
        centered(kinds, kSmallTextSize, palette.on_surface_variant);
        y += 32.0F;
        if (summary.files > 0) {
            separator();
            row(summary.folders > 0 ? "Size of the files" : "Size", format_size(summary.bytes));
        }
        return;
    }
    if (single == nullptr) {
        // Nothing selected: the folder itself.
        icon("folder", palette.primary);
        const std::string name = folder.path().filename().string();
        centered(name.empty() ? folder.path().string() : name, 15.0F, palette.on_surface);
        y += 24.0F;
        centered("Folder", kSmallTextSize, palette.on_surface_variant);
        y += 32.0F;
        separator();
        row("Items", std::format("{}", folder.entries().size()));
        return;
    }

    const fs::Entry& entry = *single;
    fs::Entry typed = entry;
    typed.mime = folder.mime_of(entry);
    icon(fs::icon_name(mime, typed),
         entry.is_directory() ? palette.primary : palette.on_surface_variant);
    centered(entry.name, 15.0F, palette.on_surface);
    y += 24.0F;
    const std::string_view type =
        typed.mime == fs::kUnknownMime ? std::string_view{"…"} : mime.name(typed.mime);
    centered(type, kSmallTextSize, palette.on_surface_variant);
    y += 32.0F;
    separator();
    if (!entry.is_directory()) {
        row("Size", format_size(entry.size));
    }
    row("Modified", format_time(entry.modified_ns));
}

}  // namespace nordend::views
