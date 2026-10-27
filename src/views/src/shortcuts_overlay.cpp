#include "nordend/views/shortcuts_overlay.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nordend/views/shortcuts.hpp"

namespace nordend::views {
namespace {

constexpr float kMaxCardWidth = 820.0F;
constexpr float kMargin = 32.0F;     // around the card, at least
constexpr float kPadding = 28.0F;    // inside the card
constexpr float kColumnGap = 40.0F;  // between the two columns
constexpr float kTitleSize = 16.0F;
constexpr float kHeadHeight = 44.0F;
constexpr float kGroupSize = 11.0F;
constexpr float kGroupHeight = 30.0F;
constexpr float kGroupGap = 10.0F;  // before every group but a column's first
constexpr float kRowHeight = 22.0F;
constexpr float kTextSize = 13.0F;
constexpr float kKeysShare = 0.38F;  // of a column, for the keys

// The groups in each column of the card.
constexpr std::array<std::array<std::string_view, 4>, 2> kColumns{{
    {"General", "Moving", "", ""},
    {"Selecting", "Grid", "Sidebar", "Quick Look"},
}};

std::string upper(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(),
                           [](char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// The height of a column of groups.
float column_height(const std::array<std::string_view, 4>& groups) {
    float height = 0.0F;
    bool first = true;
    for (const std::string_view group : groups) {
        if (group.empty()) {
            continue;
        }
        height += (first ? 0.0F : kGroupGap) + kGroupHeight;
        first = false;
        for (const Shortcut& shortcut : shortcuts()) {
            if (shortcut.group == group) {
                height += kRowHeight;
            }
        }
    }
    return height;
}

}  // namespace

ShortcutsOverlay::ShortcutsOverlay(std::function<void()> redraw) : redraw_{std::move(redraw)} {}

void ShortcutsOverlay::open() {
    if (!open_) {
        open_ = true;
        redraw_();
    }
}

void ShortcutsOverlay::close() {
    if (open_) {
        open_ = false;
        redraw_();
    }
}

void ShortcutsOverlay::draw(ui::Renderer& renderer, const ui::Rect& bounds,
                            const theme::Palette& palette) const {
    if (!open_) {
        return;
    }
    renderer.fill_rect(bounds, palette.scrim.with_alpha(0.55F));
    const float width = std::min(kMaxCardWidth, bounds.width - (2.0F * kMargin));
    const float content = std::max(column_height(kColumns[0]), column_height(kColumns[1]));
    const float height =
        std::min(bounds.height - (2.0F * kMargin), kHeadHeight + content + (2.0F * kPadding));
    const ui::Rect card{bounds.x + ((bounds.width - width) / 2.0F),
                        bounds.y + ((bounds.height - height) / 2.0F), width, height};
    renderer.draw_box(card, {.fill = palette.surface_container_high,
                             .radius = ui::Corners::all(14.0F),
                             .border_width = 1.0F,
                             .border = palette.outline_variant});
    renderer.push_clip(card);
    renderer.draw_text("Keyboard shortcuts", card.x + kPadding, card.y + kPadding, kTitleSize,
                       palette.on_surface);

    const float column_width = (width - (2.0F * kPadding) - kColumnGap) / 2.0F;
    const float keys_width = column_width * kKeysShare;
    for (std::size_t c = 0; c < kColumns.size(); ++c) {
        const float left =
            card.x + kPadding + (static_cast<float>(c) * (column_width + kColumnGap));
        float y = card.y + kPadding + kHeadHeight;
        bool first = true;
        for (const std::string_view group : kColumns.at(c)) {
            if (group.empty()) {
                continue;
            }
            y += first ? 0.0F : kGroupGap;
            first = false;
            renderer.fill_rect({left, y, column_width, 1.0F / renderer.scale()},
                               palette.outline_variant);
            renderer.draw_text(upper(group), left, y + 10.0F, kGroupSize,
                               palette.on_surface_variant);
            y += kGroupHeight;
            for (const Shortcut& shortcut : shortcuts()) {
                if (shortcut.group != group) {
                    continue;
                }
                renderer.draw_text(shortcut.keys, left, y, kTextSize, palette.primary,
                                   keys_width - 8.0F);
                renderer.draw_text(shortcut.action, left + keys_width, y, kTextSize,
                                   palette.on_surface, column_width - keys_width);
                y += kRowHeight;
            }
        }
    }
    renderer.pop_clip();
}

}  // namespace nordend::views
