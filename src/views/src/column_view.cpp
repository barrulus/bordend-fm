#include "nordend/views/column_view.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "nordend/fs/icon.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr float kPadding = 16.0F;
constexpr float kTitleHeight = 40.0F;
constexpr float kSmallTextSize = 12.0F;
constexpr float kDividerGrab = 4.0F;  // either side of a column edge
constexpr float kInfoIconSize = 64.0F;
constexpr float kScrollbarHeight = 4.0F;

float text_width(ui::Renderer& renderer, std::string_view text, float size) {
    return renderer.fonts().layout(text, size, renderer.scale()).width / renderer.scale();
}

void draw_right_aligned(ui::Renderer& renderer, std::string_view text, float right_x, float top_y,
                        float font_size, ui::Color color) {
    renderer.draw_text(text, right_x - text_width(renderer, text, font_size), top_y, font_size,
                       color);
}

// "1 file", "3 files".
std::string count_of(std::size_t count, std::string_view noun) {
    return std::format("{} {}{}", count, noun, count == 1 ? "" : "s");
}

// The selection mode of a click or a key: Shift extends from the anchor, Ctrl does what `ctrl`
// says (toggle for a click, cursor alone for a key), neither replaces.
SelectMode select_mode(const ui::Modifiers& modifiers, SelectMode ctrl) {
    if (modifiers.shift) {
        return SelectMode::extend;
    }
    return modifiers.ctrl ? ctrl : SelectMode::replace;
}

}  // namespace

ColumnView::ColumnView(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                       std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                       std::function<void(ui::CursorShape)> set_cursor)
    : loop_{loop},
      pool_{pool},
      watcher_{watcher},
      mime_{std::move(mime)},
      redraw_{std::move(redraw)},
      set_cursor_{std::move(set_cursor)} {}

ColumnView::~ColumnView() = default;

std::unique_ptr<Column> ColumnView::make_column(const std::filesystem::path& directory) {
    // The column reports its cursor changes with its own address, known once it exists.
    auto self = std::make_shared<const Column*>(nullptr);
    auto column = std::make_unique<Column>(loop_, pool_, watcher_, mime_, directory, redraw_,
                                           [this, self] { on_cursor_changed(*self); });
    *self = column.get();
    return column;
}

void ColumnView::open(const std::filesystem::path& directory) {
    reveal_.clear();
    columns_.clear();
    focus_ = 0;
    resizing_.reset();
    hovered_column_.reset();
    gesture_axis_ = Axis::none;
    scroller_.jump_to(0.0F);
    columns_.push_back({.column = make_column(directory)});
    redraw_();
}

void ColumnView::reveal(const std::filesystem::path& root, const std::filesystem::path& target) {
    open(root);
    const std::filesystem::path relative = target.lexically_relative(root);
    if (relative.empty() || relative == "." || *relative.begin() == "..") {
        return;  // not inside root
    }
    reveal_ = target;
    columns_.front().column->select_name(relative.begin()->string());
}

bool ColumnView::loading() const {
    return std::ranges::any_of(columns_, [](const Slot& slot) {
        return slot.column->model().loading() || slot.column->cursor_pending();
    });
}

bool ColumnView::animating() const noexcept {
    return scroller_.animating() ||
           std::ranges::any_of(columns_, [](const Slot& slot) { return slot.column->animating(); });
}

void ColumnView::on_cursor_changed(const Column* column) {
    const auto it = std::ranges::find_if(
        columns_, [column](const Slot& slot) { return slot.column.get() == column; });
    if (it == columns_.end()) {
        return;  // still being built
    }
    sync_after(static_cast<std::size_t>(it - columns_.begin()));
}

// Keeps column index + 1 showing what column index has selected, when that is one entry: the
// same folder (kept as it is), another folder (a new column, and nothing after it), or a file or
// nothing (no column after it: the info pane shows the file, or sums up a larger selection).
void ColumnView::sync_after(std::size_t index) {
    Column* const column = columns_.at(index).column.get();
    if (column->cursor_pending()) {
        return;  // keep what is there until the cursor lands
    }
    const fs::Entry* const entry = column->single_selection();
    if (!reveal_.empty() && entry != nullptr && column->path() / entry->name == reveal_) {
        reveal_.clear();  // arrived
        focus_ = index;
    }
    if (entry == nullptr || !entry->is_directory()) {
        truncate(index + 1);
        reveal_focus_ = true;
        redraw_();
        return;
    }
    const std::filesystem::path child = column->path() / entry->name;
    if (index + 1 < columns_.size() && columns_[index + 1].column->path() == child) {
        return;
    }
    const float width =
        index + 1 < columns_.size() ? columns_[index + 1].width : kDefaultColumnWidth;
    truncate(index + 1);
    columns_.push_back({.column = make_column(child), .width = width});
    reveal_focus_ = true;
    redraw_();
    if (!reveal_.empty()) {
        const std::filesystem::path rest = reveal_.lexically_relative(child);
        if (!rest.empty() && rest != "." && *rest.begin() != "..") {
            columns_.back().column->select_name(rest.begin()->string());  // may sync again
        } else {
            reveal_.clear();
        }
    }
}

void ColumnView::truncate(std::size_t count) {
    if (count >= columns_.size()) {
        return;
    }
    columns_.erase(columns_.begin() + static_cast<std::ptrdiff_t>(count), columns_.end());
    focus_ = std::min(focus_, count - 1);
    if (resizing_ && *resizing_ >= count) {
        resizing_.reset();
    }
    if (hovered_column_ && *hovered_column_ >= count) {
        hovered_column_.reset();
    }
    if (gesture_column_ && *gesture_column_ >= count) {
        gesture_column_.reset();
    }
}

void ColumnView::focus_on(std::size_t index) {
    if (index != focus_) {
        focus_ = index;
        redraw_();
    }
    reveal_focus_ = true;
}

void ColumnView::go_right(Clock::time_point now) {
    const fs::Entry* const entry = columns_[focus_].column->single_selection();
    if (entry == nullptr || !entry->is_directory() || focus_ + 1 >= columns_.size()) {
        return;
    }
    focus_on(focus_ + 1);
    Column& next = *columns_[focus_].column;
    if (!next.cursor() && !next.cursor_pending()) {
        next.set_cursor(0, now);
    }
}

void ColumnView::go_left(Clock::time_point now) {
    if (focus_ > 0) {
        columns_[focus_].column->set_cursor(std::nullopt, now);  // closes what it opened
        focus_on(focus_ - 1);
        return;
    }
    // From the first column: its parent becomes the new first column.
    const std::filesystem::path current = columns_.front().column->path();
    const std::filesystem::path parent = current.parent_path();
    if (parent == current || current.filename().empty()) {
        return;  // at the root
    }
    columns_.front().column->set_cursor(std::nullopt, now);
    columns_.insert(columns_.begin(), Slot{.column = make_column(parent)});
    if (resizing_) {
        ++*resizing_;
    }
    hovered_column_.reset();
    gesture_column_.reset();
    focus_ = 0;
    reveal_focus_ = true;
    // The horizontal offset now counts from the new column: keep the old ones where they were,
    // then glide the new one in.
    shift_ += columns_.front().width;
    columns_.front().column->select_name(current.filename().string());
    redraw_();
}

void ColumnView::layout() {
    float x = 0.0F;
    for (Slot& slot : columns_) {
        slot.x = x;
        x += slot.width;
    }
}

bool ColumnView::shows_info() const {
    if (columns_.empty()) {
        return false;
    }
    const Column& last = *columns_.back().column;
    const fs::Entry* const entry = last.single_selection();
    return entry != nullptr ? !entry->is_directory() : last.selected_count() > 1;
}

float ColumnView::content_width() const {
    float width = columns_.empty() ? 0.0F : columns_.back().x + columns_.back().width;
    if (shows_info()) {
        width += kInfoWidth;
    }
    return width;
}

std::optional<std::size_t> ColumnView::column_at(float x) const {
    const float content_x = x - area_.x + scroller_.offset();
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (content_x >= columns_[i].x && content_x < columns_[i].x + columns_[i].width) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> ColumnView::divider_at(float x, float y) const {
    if (y < area_.y || y >= area_.bottom()) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const float edge = area_.x + columns_[i].x + columns_[i].width - scroller_.offset();
        if (std::abs(x - edge) <= kDividerGrab) {
            return i;
        }
    }
    return std::nullopt;
}

void ColumnView::update_hover() {
    std::optional<std::size_t> column;
    std::optional<std::size_t> row;
    if (pointer_inside_ && !resizing_ && pointer_y_ >= area_.y && pointer_y_ < area_.bottom() &&
        !divider_at(pointer_x_, pointer_y_)) {
        column = column_at(pointer_x_);
        if (column) {
            row = columns_[*column].column->row_at(pointer_x_, pointer_y_);
        }
    }
    bool changed = false;
    if (hovered_column_ && hovered_column_ != column && *hovered_column_ < columns_.size()) {
        changed = columns_[*hovered_column_].column->set_hovered(std::nullopt);
    }
    hovered_column_ = column;
    if (column) {
        changed = columns_[*column].column->set_hovered(row) || changed;
    }
    if (changed) {
        redraw_();
    }
}

void ColumnView::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                      ui::IconSet& icons, Clock::time_point now) {
    draw_title(renderer, bounds, palette);
    area_ = {bounds.x, bounds.y + kTitleHeight, bounds.width,
             std::max(0.0F, bounds.height - kTitleHeight)};
    renderer.fill_rect({bounds.x, area_.y - 1.0F, bounds.width, 1.0F / renderer.scale()},
                       palette.outline_variant);

    layout();
    scroller_.set_extent(content_width(), area_.width);
    if (shift_ != 0.0F) {
        scroller_.jump_to(scroller_.offset() + shift_);
        shift_ = 0.0F;
    }
    if (reveal_focus_ && !columns_.empty()) {
        // Show the focused column and what it opens (the next column, or the info pane).
        const float left = columns_[focus_].x;
        const float right = focus_ + 1 < columns_.size()
                                ? columns_[focus_ + 1].x + columns_[focus_ + 1].width
                                : content_width();
        const float from = scroller_.destination();
        float to = from;
        if (right > to + area_.width) {
            to = right - area_.width;
        }
        to = std::min(to, left);
        if (to != from) {
            scroller_.glide_to(to, now);
        }
        reveal_focus_ = false;
    }
    scroller_.advance(now);
    const float offset = scroller_.offset();

    renderer.push_clip(area_);
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const Slot& slot = columns_[i];
        const float left = area_.x + slot.x - offset;
        if (left + slot.width <= area_.x || left >= area_.right()) {
            slot.column->scroller().advance(now);  // off screen: animations still end
            continue;
        }
        slot.column->draw(renderer, {left, area_.y, slot.width, area_.height}, palette, icons,
                          i == focus_, now);
        const bool grabbed =
            resizing_ == i || (over_divider_ && divider_at(pointer_x_, pointer_y_) == i);
        const float line = grabbed ? 2.0F : 1.0F / renderer.scale();
        renderer.fill_rect({left + slot.width - line, area_.y, line, area_.height},
                           grabbed ? palette.primary : palette.outline_variant);
    }
    if (shows_info()) {
        const float left = area_.x + content_width() - kInfoWidth - offset;
        draw_info(renderer,
                  {left, area_.y, std::max(kInfoWidth, area_.right() - left), area_.height},
                  palette, icons);
    }
    // Horizontal scrollbar, only when the columns do not fit.
    if (scroller_.max_offset() > 0.0F) {
        const float content = content_width();
        const float thumb = std::max(24.0F, area_.width * area_.width / content);
        const float thumb_x = area_.x + ((area_.width - thumb) * offset / scroller_.max_offset());
        renderer.draw_box(
            {thumb_x, area_.bottom() - kScrollbarHeight - 2.0F, thumb, kScrollbarHeight},
            {.fill = palette.on_surface_variant.with_alpha(0.45F),
             .radius = ui::Corners::all(kScrollbarHeight / 2.0F)});
    }
    renderer.pop_clip();

    update_hover();  // content may have moved under a still pointer
}

void ColumnView::draw_title(ui::Renderer& renderer, const ui::Rect& bounds,
                            const theme::Palette& palette) {
    if (columns_.empty()) {
        return;
    }
    const Column& column = *columns_[focus_].column;
    const fs::DirectoryModel& model = column.model();
    const std::size_t count = model.listing().entries().size();
    std::string status;
    if (model.error()) {
        status = model.error().message();
    } else if (column.selected_count() > 1) {
        status = std::format("{} of {} selected", column.selected_count(), count);
    } else if (model.loading()) {
        status = std::format("{} items, loading…", count);
    } else {
        status = std::format("{} items", count);
    }
    const float title_y = bounds.y + 12.0F;
    const float status_width = text_width(renderer, status, kSmallTextSize);
    draw_right_aligned(renderer, status, bounds.right() - kPadding, title_y + 2.0F, kSmallTextSize,
                       palette.on_surface_variant);
    const fs::Entry* const entry = column.cursor_entry();
    const std::filesystem::path shown =
        entry != nullptr ? column.path() / entry->name : column.path();
    renderer.draw_text(shown.string(), bounds.x + kPadding, title_y, 15.0F, palette.on_surface,
                       bounds.width - (3.0F * kPadding) - status_width, ui::Ellipsis::middle);
}

void ColumnView::draw_info(ui::Renderer& renderer, const ui::Rect& bounds,
                           const theme::Palette& palette, ui::IconSet& icons) {
    const Column& column = *columns_.back().column;
    const fs::Entry* const single = column.single_selection();
    const float width = std::min(bounds.width, kInfoWidth);  // content stays at the left
    float y = bounds.y + 32.0F;
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

    if (single == nullptr) {
        // Several entries: how many, of which kind, and how big.
        const SelectionSummary& summary = column.summary();
        icons.draw(renderer, "files", bounds.x + ((width - kInfoIconSize) / 2.0F), y, kInfoIconSize,
                   palette.on_surface_variant);
        y += kInfoIconSize + 20.0F;
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

    const fs::Entry& entry = *single;
    fs::Entry typed = entry;
    typed.mime = column.mime_of(entry);
    icons.draw(renderer, fs::icon_name(*mime_, typed), bounds.x + ((width - kInfoIconSize) / 2.0F),
               y, kInfoIconSize, palette.on_surface_variant);
    y += kInfoIconSize + 20.0F;

    centered(entry.name, 15.0F, palette.on_surface);
    y += 24.0F;
    const std::string_view type =
        typed.mime == fs::kUnknownMime ? std::string_view{"…"} : mime_->name(typed.mime);
    centered(type, kSmallTextSize, palette.on_surface_variant);
    y += 32.0F;
    separator();
    row("Size", format_size(entry.size));
    row("Modified", format_time(entry.modified_ns));
}

void ColumnView::pointer(const ui::PointerEvent& event, Clock::time_point now) {
    using Kind = ui::PointerEvent::Kind;
    switch (event.kind) {
        case Kind::enter:
        case Kind::motion: {
            pointer_inside_ = true;
            pointer_x_ = event.x;
            pointer_y_ = event.y;
            if (resizing_) {
                Slot& slot = columns_[*resizing_];
                const float left = area_.x + slot.x - scroller_.offset();
                slot.width =
                    std::clamp(event.x - resize_grab_ - left, kMinColumnWidth, kMaxColumnWidth);
                redraw_();
                break;
            }
            const bool over = divider_at(event.x, event.y).has_value();
            if (over != over_divider_) {
                over_divider_ = over;
                set_cursor_(over ? ui::CursorShape::ew_resize : ui::CursorShape::standard);
                redraw_();
            }
            update_hover();
            break;
        }
        case Kind::leave:
            pointer_inside_ = false;
            if (over_divider_) {
                over_divider_ = false;
                set_cursor_(ui::CursorShape::standard);
                redraw_();
            }
            update_hover();
            break;
        case Kind::scroll:
            scroll(event, now);
            break;
        case Kind::press:
            if (event.button == ui::PointerButton::back) {
                reveal_.clear();
                go_left(now);
                break;
            }
            if (event.button != ui::PointerButton::left) {
                break;
            }
            if (const std::optional<std::size_t> divider = divider_at(event.x, event.y)) {
                Slot& slot = columns_[*divider];
                if (event.click_count == 2) {
                    slot.width = kDefaultColumnWidth;  // double click: back to the default
                    redraw_();
                    break;
                }
                resizing_ = divider;
                resize_grab_ = event.x - (area_.x + slot.x + slot.width - scroller_.offset());
                update_hover();
                break;
            }
            if (event.y < area_.y || event.y >= area_.bottom()) {
                break;
            }
            if (const std::optional<std::size_t> index = column_at(event.x)) {
                reveal_.clear();
                Column& column = *columns_[*index].column;
                focus_on(*index);
                const std::optional<std::size_t> row = column.row_at(event.x, event.y);
                const SelectMode mode = select_mode(event.modifiers, SelectMode::toggle);
                if (row || mode == SelectMode::replace) {
                    column.set_cursor(row, now, mode);  // a plain click on nothing clears
                }
                redraw_();
            }
            break;
        case Kind::release:
            if (event.button == ui::PointerButton::left && resizing_) {
                resizing_.reset();
                reveal_focus_ = true;
                const bool over = divider_at(event.x, event.y).has_value();
                if (over != over_divider_) {
                    over_divider_ = over;
                    set_cursor_(over ? ui::CursorShape::ew_resize : ui::CursorShape::standard);
                }
                redraw_();
            }
            break;
    }
}

void ColumnView::scroll(const ui::PointerEvent& event, Clock::time_point now) {
    if (event.scroll_finger) {
        // Touchpad: the gesture follows the axis it starts on, and the column it starts over.
        if (gesture_axis_ == Axis::none && (event.scroll_x != 0.0F || event.scroll_y != 0.0F)) {
            gesture_axis_ = std::abs(event.scroll_x) > std::abs(event.scroll_y) ? Axis::horizontal
                                                                                : Axis::vertical;
            gesture_column_ = column_at(event.x);
        }
        if (gesture_axis_ == Axis::horizontal) {
            if (event.scroll_x != 0.0F) {
                scroller_.finger(event.scroll_x, now);
            }
            if (event.scroll_stop) {
                scroller_.release(now);
            }
        } else if (gesture_axis_ == Axis::vertical && gesture_column_) {
            ui::Scroller& scroller = columns_[*gesture_column_].column->scroller();
            if (event.scroll_y != 0.0F) {
                scroller.finger(event.scroll_y, now);
            }
            if (event.scroll_stop) {
                scroller.release(now);
            }
        }
        if (event.scroll_stop) {
            gesture_axis_ = Axis::none;
            gesture_column_.reset();
        }
        redraw_();
        return;
    }
    float horizontal_wheel = event.wheel_x;
    if (horizontal_wheel == 0.0F && event.modifiers.shift) {
        horizontal_wheel = event.wheel_y;  // Shift+wheel scrolls sideways
    }
    if (horizontal_wheel != 0.0F) {
        scroller_.wheel(horizontal_wheel, now);
    } else if (const std::optional<std::size_t> index = column_at(event.x)) {
        ui::Scroller& scroller = columns_[*index].column->scroller();
        if (event.wheel_y != 0.0F) {
            scroller.wheel(event.wheel_y, now);
        } else if (event.scroll_y != 0.0F) {
            scroller.finger(event.scroll_y, now);  // other continuous sources: 1:1, no inertia
        }
    }
    redraw_();
}

void ColumnView::key(const ui::KeyEvent& event, Clock::time_point now) {
    if (columns_.empty()) {
        return;
    }
    Column& column = *columns_[focus_].column;
    const auto page = static_cast<std::ptrdiff_t>(column.page_rows());
    // Shift extends the selection, Ctrl moves the cursor alone.
    const SelectMode mode = select_mode(event.modifiers, SelectMode::keep);
    const bool ctrl = event.modifiers.ctrl;
    switch (event.keysym) {
        case XKB_KEY_Down:
            column.move_cursor(1, now, mode);
            break;
        case XKB_KEY_Up:
            if (event.modifiers.alt) {
                go_left(now);
            } else {
                column.move_cursor(-1, now, mode);
            }
            break;
        case XKB_KEY_Page_Down:
            column.move_cursor(page, now, mode);
            break;
        case XKB_KEY_Page_Up:
            column.move_cursor(-page, now, mode);
            break;
        case XKB_KEY_Home:
            column.set_cursor(0, now, mode);
            break;
        case XKB_KEY_End:
            column.set_cursor(std::numeric_limits<std::size_t>::max(), now, mode);
            break;
        case XKB_KEY_space:
            if (!ctrl || !column.cursor()) {
                return;  // plain Space is for Quick Look (ROADMAP 5.3)
            }
            column.set_cursor(column.cursor(), now, SelectMode::toggle);
            break;
        case XKB_KEY_a:
        case XKB_KEY_A:
            if (!ctrl) {
                return;
            }
            column.select_all();
            break;
        case XKB_KEY_Escape:
            column.collapse_selection();
            break;
        case XKB_KEY_Right:
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            go_right(now);
            break;
        case XKB_KEY_Left:
        case XKB_KEY_BackSpace:
            go_left(now);
            break;
        default:
            return;
    }
    reveal_.clear();
    reveal_focus_ = true;
    redraw_();
}

}  // namespace nordend::views
