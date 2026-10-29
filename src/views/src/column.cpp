#include "nordend/views/column.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "draw_helpers.hpp"
#include "item_style.hpp"
#include "nordend/fs/icon.hpp"
#include "nordend/ui/text_field.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr float kRowInset = 6.0F;  // highlight box inside the column
constexpr float kIconX = 14.0F;
constexpr float kIconSize = 18.0F;
constexpr float kIconGap = 10.0F;
constexpr float kChevronSize = 14.0F;
constexpr float kRightPadding = 12.0F;
constexpr float kTextSize = 14.0F;
constexpr float kScrollbarWidth = 4.0F;
constexpr float kScrollbarMinThumb = 24.0F;

}  // namespace

Column::Column(std::shared_ptr<Folder> folder) : folder_{std::move(folder)} {}

std::size_t Column::page_rows() const {
    const auto rows = static_cast<std::size_t>(rows_area_.height / kRowHeight);
    return std::max<std::size_t>(1, rows > 1 ? rows - 1 : rows);
}

void Column::reveal_cursor(Folder::Reveal reveal, Clock::time_point now) {
    const std::optional<std::size_t> cursor = folder_->cursor();
    const float viewport = rows_area_.height;
    if (reveal == Folder::Reveal::none || !cursor) {
        return;
    }
    if (viewport <= 0.0F) {
        folder_->request_jump();  // nowhere to show it yet
        return;
    }
    const float top = static_cast<float>(*cursor) * kRowHeight;
    const float bottom = top + kRowHeight;
    const float from = scroller_.destination();
    if (top >= from && bottom <= from + viewport) {
        return;
    }
    if (reveal == Folder::Reveal::jump) {
        scroller_.jump_to(top - ((viewport - kRowHeight) / 2.0F));  // centered
    } else if (top < from) {
        scroller_.glide_to(top, now);
    } else {
        scroller_.glide_to(bottom - viewport, now);
    }
}

void Column::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                  ui::IconSet& icons, bool focused, Clock::time_point now, EditSlot* edit) {
    const std::vector<fs::Entry>& entries = folder_->entries();
    rows_area_ = bounds;
    const float content = static_cast<float>(entries.size()) * kRowHeight;
    scroller_.set_extent(content, bounds.height);
    reveal_cursor(folder_->take_reveal(), now);
    scroller_.advance(now);
    const float offset = scroller_.offset();
    const RowRange range = visible_rows(entries.size(), kRowHeight, offset, bounds.height);

    renderer.push_clip(bounds);
    const float name_x = bounds.x + kIconX + kIconSize + kIconGap;
    const std::optional<std::size_t> cursor = folder_->cursor();
    const bool cursor_ring = folder_->single_selection() == nullptr;
    for (std::size_t i = range.first; i < range.last; ++i) {
        const fs::Entry& entry = entries[i];
        const float y = bounds.y + (static_cast<float>(i) * kRowHeight) - offset;
        const ItemStyle style = item_style(palette, entry,
                                           {.selected = folder_->is_selected(entry.name),
                                            .cursor = cursor == i,
                                            .ring = cursor_ring,
                                            .focused = focused,
                                            .hovered = hovered_ == i});
        if (style.fill || style.ring) {
            renderer.draw_box({bounds.x + kRowInset, y + 1.0F, bounds.width - (2.0F * kRowInset),
                               kRowHeight - 2.0F},
                              {.fill = style.fill.value_or(kTransparent),
                               .radius = ui::Corners::all(6.0F),
                               .border_width = style.ring ? 1.5F : 0.0F,
                               .border = style.ring.value_or(kTransparent)});
        }
        fs::Entry typed = entry;
        typed.mime = folder_->mime_of(entry);
        icons.draw(renderer, fs::icon_name(folder_->mime(), typed), bounds.x + kIconX,
                   y + ((kRowHeight - kIconSize) / 2.0F), kIconSize, style.icon);
        float name_right = bounds.right() - kRightPadding;
        if (entry.is_directory()) {
            name_right -= kChevronSize + 6.0F;
            icons.draw(renderer, "chevron-right", bounds.right() - kRightPadding - kChevronSize,
                       y + ((kRowHeight - kChevronSize) / 2.0F), kChevronSize, style.chevron);
        }
        if (edit != nullptr && edit->holds(*folder_, entry)) {
            edit->place({name_x - ui::TextField::kPadding, y + 3.0F,
                         std::max(1.0F, name_right - name_x) + (2.0F * ui::TextField::kPadding),
                         kRowHeight - 6.0F},
                        kTextSize);
            continue;
        }
        renderer.draw_text(entry.name, name_x, y + ((kRowHeight - kTextSize) / 2.0F) - 2.0F,
                           kTextSize, style.text, std::max(1.0F, name_right - name_x),
                           ui::Ellipsis::middle);
    }

    const fs::DirectoryModel& model = folder_->model();
    if (model.error()) {
        draw_centered(renderer, model.error().message(), bounds, bounds.y + 24.0F, kTextSize,
                      palette.error);
    } else if (entries.empty() && !model.loading()) {
        draw_centered(renderer, "Empty folder", bounds, bounds.y + 24.0F, kTextSize,
                      palette.on_surface_variant);
    }

    // Scrollbar: a thin thumb, only when there is something to scroll.
    if (scroller_.max_offset() > 0.0F) {
        const float thumb = std::max(kScrollbarMinThumb, bounds.height * bounds.height / content);
        const float travel = bounds.height - thumb;
        const float thumb_y = bounds.y + (travel * offset / scroller_.max_offset());
        renderer.draw_box(
            {bounds.right() - kScrollbarWidth - 2.0F, thumb_y, kScrollbarWidth, thumb},
            {.fill = palette.on_surface_variant.with_alpha(0.45F),
             .radius = ui::Corners::all(kScrollbarWidth / 2.0F)});
    }
    renderer.pop_clip();

    folder_->sniff(range.first, range.last);
}

std::optional<std::size_t> Column::row_at(float x, float y) const {
    if (x < rows_area_.x || x >= rows_area_.right() || y < rows_area_.y ||
        y >= rows_area_.bottom()) {
        return std::nullopt;
    }
    const float content_y = y - rows_area_.y + scroller_.offset();
    const auto row = static_cast<std::size_t>(content_y / kRowHeight);
    if (row >= folder_->entries().size()) {
        return std::nullopt;
    }
    return row;
}

bool Column::set_hovered(std::optional<std::size_t> row) {
    if (row == hovered_) {
        return false;
    }
    hovered_ = row;
    return true;
}

}  // namespace nordend::views
