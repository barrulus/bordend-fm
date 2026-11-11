#include "nordend/views/shortcuts_overlay.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "nordend/views/shortcuts.hpp"

namespace nordend::views {
namespace {

constexpr float kMaxCardWidth = 1120.0F;
constexpr float kMargin = 32.0F;     // around the card, at least
constexpr float kPadding = 28.0F;    // inside the card
constexpr float kColumnGap = 40.0F;  // between the columns
constexpr float kTitleSize = 16.0F;
constexpr float kHeadHeight = 64.0F;
constexpr float kGroupSize = 11.0F;
constexpr float kGroupHeight = 30.0F;
constexpr float kGroupGap = 10.0F;  // before every group but a column's first
constexpr float kRowHeight = 44.0F;
constexpr float kTextSize = 13.0F;

using Column = std::array<std::string_view, 8>;
using Layout = std::array<Column, 3>;

// Related groups stay together; narrow windows use one column.
constexpr Layout kTwoColumns{{
    {"General", "Moving", "Zoom", "Quick Look", ""},
    {"Selecting", "Files", "Sidebar", "Tabs", ""},
    {"", "", "", "", ""},
}};
constexpr Layout kOneColumn{{
    {"General", "Moving", "Selecting", "Files", "Tabs", "Sidebar", "Zoom", "Quick Look"},
    {},
    {},
}};

std::string upper(std::string_view text) {
    std::string out{text};
    std::ranges::transform(out, out.begin(),
                           [](char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// The height of a column of groups.
float column_height(const Column& groups) {
    float height = 0.0F;
    bool first = true;
    for (const std::string_view group : groups) {
        if (group.empty()) {
            continue;
        }
        height += (first ? 0.0F : kGroupGap) + kGroupHeight;
        first = false;
        for (const Shortcut& shortcut : shortcuts()) {
            if (shortcut.group == group && !shortcut.keys.empty()) {
                height += kRowHeight;
            }
        }
    }
    return height;
}

}  // namespace

std::vector<std::string_view> overlay_groups() {
    std::vector<std::string_view> groups;
    for (const Column& column : kTwoColumns) {
        for (const std::string_view group : column) {
            if (!group.empty()) {
                groups.push_back(group);
            }
        }
    }
    return groups;
}

ShortcutsOverlay::ShortcutsOverlay(std::function<void()> redraw) : redraw_{std::move(redraw)} {}

void ShortcutsOverlay::open() {
    if (!open_) {
        open_ = true;
        offset_ = 0.0F;
        dragging_ = false;
        redraw_();
    }
}

void ShortcutsOverlay::close() {
    if (open_) {
        open_ = false;
        dragging_ = false;
        redraw_();
    }
}

void ShortcutsOverlay::layout(const ui::Rect& bounds) {
    const float width = std::max(0.0F, std::min(kMaxCardWidth, bounds.width - 2.0F * kMargin));
    narrow_ = width < 900.0F;
    const Layout& groups = narrow_ ? kOneColumn : kTwoColumns;
    content_ = 0.0F;
    for (const Column& column : groups) {
        content_ = std::max(content_, column_height(column));
    }
    const float height = std::max(
        0.0F, std::min(bounds.height - 2.0F * kMargin, kHeadHeight + content_ + 2.0F * kPadding));
    card_ = {bounds.x + (bounds.width - width) / 2.0F, bounds.y + (bounds.height - height) / 2.0F,
             width, height};
    viewport_ = {card_.x + kPadding, card_.y + kPadding + kHeadHeight,
                 std::max(0.0F, width - 2.0F * kPadding - 16.0F),
                 std::max(0.0F, height - 2.0F * kPadding - kHeadHeight)};
    close_button_ = {card_.right() - kPadding - 32.0F, card_.y + 18.0F, 32.0F, 32.0F};
    track_ = {viewport_.right() + 8.0F, viewport_.y, 8.0F, viewport_.height};
    max_scroll_ = std::max(0.0F, content_ - viewport_.height);
    offset_ = std::clamp(offset_, 0.0F, max_scroll_);
}

ui::Rect ShortcutsOverlay::thumb() const {
    const float height =
        content_ > 0.0F
            ? std::min(track_.height, std::max(28.0F, track_.height * viewport_.height / content_))
            : track_.height;
    const float y = max_scroll_ > 0.0F ? offset_ / max_scroll_ * (track_.height - height) : 0.0F;
    return {track_.x, track_.y + y, track_.width, height};
}

void ShortcutsOverlay::scroll_to(float offset) {
    offset_ = std::clamp(offset, 0.0F, max_scroll_);
    redraw_();
}

void ShortcutsOverlay::pointer(const ui::PointerEvent& event) {
    if (!open_) {
        return;
    }
    if (event.kind == ui::PointerEvent::Kind::release) {
        dragging_ = false;
    }
    if (event.kind == ui::PointerEvent::Kind::scroll) {
        scroll_to(offset_ + (event.wheel_y != 0.0F ? event.wheel_y * 48.0F : event.scroll_y));
    } else if (event.kind == ui::PointerEvent::Kind::press &&
               event.button == ui::PointerButton::left) {
        if (!card_.contains(event.x, event.y) || close_button_.contains(event.x, event.y)) {
            close();
        } else if (max_scroll_ > 0.0F && track_.contains(event.x, event.y)) {
            const auto handle = thumb();
            if (handle.contains(event.x, event.y)) {
                dragging_ = true;
                drag_y_ = event.y - handle.y;
            } else {
                scroll_to(offset_ + (event.y < handle.y ? -viewport_.height : viewport_.height));
            }
        }
    } else if (event.kind == ui::PointerEvent::Kind::motion && dragging_) {
        const float travel = track_.height - thumb().height;
        if (travel > 0.0F) {
            scroll_to((event.y - track_.y - drag_y_) / travel * max_scroll_);
        }
    }
}

void ShortcutsOverlay::key(const ui::KeyEvent& event) {
    if (!open_ || event.modifiers != ui::Modifiers{}) {
        return;
    }
    switch (event.keysym) {
        case XKB_KEY_Up:
            scroll_to(offset_ - kRowHeight);
            break;
        case XKB_KEY_Down:
            scroll_to(offset_ + kRowHeight);
            break;
        case XKB_KEY_Page_Up:
            scroll_to(offset_ - viewport_.height);
            break;
        case XKB_KEY_Page_Down:
            scroll_to(offset_ + viewport_.height);
            break;
        case XKB_KEY_Home:
            scroll_to(0.0F);
            break;
        case XKB_KEY_End:
            scroll_to(max_scroll_);
            break;
        default:
            break;
    }
}

void ShortcutsOverlay::draw(ui::Renderer& renderer, const ui::Rect& bounds,
                            const theme::Palette& palette) {
    if (!open_) {
        return;
    }
    layout(bounds);
    renderer.fill_rect(bounds, palette.scrim.with_alpha(0.55F));
    const auto& card = card_;
    const Layout& layout = narrow_ ? kOneColumn : kTwoColumns;
    const std::size_t columns = narrow_ ? 1 : 2;
    renderer.draw_box(card, {.fill = palette.surface_container_high,
                             .radius = ui::Corners::all(14.0F),
                             .border_width = 1.0F,
                             .border = palette.outline_variant});
    renderer.push_clip(card);
    renderer.draw_text("Keyboard shortcuts", card.x + kPadding, card.y + kPadding, kTitleSize,
                       palette.on_surface,
                       std::max(1.0F, close_button_.x - card.x - kPadding - 8.0F));
    renderer.draw_text("×", close_button_.x + 9.0F, close_button_.y + 4.0F, 20.0F,
                       palette.on_surface);
    renderer.draw_text("Scroll · ↑ ↓ · PgUp / PgDn · Home / End", card.x + kPadding,
                       card.y + kPadding + 28.0F, 11.0F, palette.on_surface_variant,
                       std::max(1.0F, viewport_.width));
    renderer.push_clip(viewport_);

    const float column_width = (viewport_.width - (kColumnGap * static_cast<float>(columns - 1))) /
                               static_cast<float>(columns);
    for (std::size_t c = 0; c < columns; ++c) {
        const float left =
            card.x + kPadding + (static_cast<float>(c) * (column_width + kColumnGap));
        float y = viewport_.y - offset_;
        bool first = true;
        for (const std::string_view group : layout.at(c)) {
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
                if (shortcut.group != group || shortcut.keys.empty()) {
                    continue;
                }
                renderer.draw_text(shortcut.keys, left, y, kTextSize, palette.primary,
                                   std::max(1.0F, column_width));
                renderer.draw_text(shortcut.action, left, y + 19.0F, kTextSize, palette.on_surface,
                                   std::max(1.0F, column_width));
                y += kRowHeight;
            }
        }
    }
    renderer.pop_clip();
    if (max_scroll_ > 0.0F) {
        renderer.draw_box(track_,
                          {.fill = palette.outline_variant, .radius = ui::Corners::all(4.0F)});
        renderer.draw_box(thumb(), {.fill = palette.primary, .radius = ui::Corners::all(4.0F)});
    }
    renderer.pop_clip();
}

}  // namespace nordend::views
