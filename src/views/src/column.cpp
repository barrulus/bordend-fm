#include "nordend/views/column.hpp"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

#include "nordend/fs/icon.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr ui::Color kTransparent{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F};
constexpr float kRowInset = 6.0F;  // highlight box inside the column
constexpr float kIconX = 14.0F;
constexpr float kIconSize = 18.0F;
constexpr float kIconGap = 10.0F;
constexpr float kChevronSize = 14.0F;
constexpr float kRightPadding = 12.0F;
constexpr float kTextSize = 14.0F;
constexpr float kScrollbarWidth = 4.0F;
constexpr float kScrollbarMinThumb = 24.0F;
// Sniffing batches stay small so a fast scroll does not queue minutes of reads.
constexpr std::size_t kMaxSniffBatch = 64;

float text_width(ui::Renderer& renderer, std::string_view text, float size) {
    return renderer.fonts().layout(text, size, renderer.scale()).width / renderer.scale();
}

void draw_centered(ui::Renderer& renderer, std::string_view text, const ui::Rect& bounds, float y,
                   ui::Color color) {
    const float width = std::min(text_width(renderer, text, kTextSize), bounds.width - 24.0F);
    renderer.draw_text(text, bounds.x + ((bounds.width - width) / 2.0F), y, kTextSize, color,
                       bounds.width - 24.0F);
}

// Colors of a row: the selection is strong (primary) in the focused column and quiet in the
// others, where it marks the path to the focused one. When the cursor is not the whole selection
// (several rows selected, or the cursor on an unselected row), a ring shows where it is.
struct RowStyle {
    std::optional<ui::Color> fill;
    std::optional<ui::Color> ring;
    ui::Color icon;
    ui::Color chevron;
    ui::Color text;
};

struct RowState {
    bool selected{false};
    bool cursor{false};
    bool ring{false};  // the cursor needs its own mark
    bool focused{false};
    bool hovered{false};
};

RowStyle row_style(const theme::Palette& palette, const fs::Entry& entry, const RowState& row) {
    RowStyle style{.fill = std::nullopt,
                   .ring = std::nullopt,
                   .icon = entry.is_directory() ? palette.primary : palette.on_surface_variant,
                   .chevron = palette.on_surface_variant,
                   .text = entry.hidden() ? palette.outline : palette.on_surface};
    if (row.selected && row.focused) {
        style.fill = palette.primary;
        style.icon = palette.on_primary;
        style.chevron = palette.on_primary;
        style.text = palette.on_primary;
    } else if (row.selected) {
        style.fill = palette.surface_container_highest;
    } else if (row.hovered) {
        style.fill = palette.surface_container_high;
    }
    if (row.cursor && row.ring && row.focused) {
        style.ring = row.selected ? palette.on_primary : palette.primary;
    }
    return style;
}

}  // namespace

Column::Column(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
               std::shared_ptr<const fs::MimeDatabase> mime, std::filesystem::path directory,
               std::function<void()> redraw, std::function<void()> changed)
    : pool_{pool},
      mime_{std::move(mime)},
      redraw_{std::move(redraw)},
      changed_{std::move(changed)},
      model_{loop, pool, watcher, mime_} {
    model_.set_change_handler([this](const fs::ModelChange& change) { on_change(change); });
    model_.open(std::move(directory));
}

Column::~Column() { *alive_ = false; }

const fs::Entry* Column::cursor_entry() const {
    return cursor_index_ ? &model_.listing().entries()[*cursor_index_] : nullptr;
}

const fs::Entry* Column::single_selection() const {
    const fs::Entry* const entry = cursor_entry();
    if (entry == nullptr || selected_.size() != 1 || !selected_.contains(entry->name)) {
        return nullptr;
    }
    return entry;
}

std::vector<std::string> Column::selected_names() const {
    std::vector<std::string> names;
    names.reserve(selected_.size());
    for (const fs::Entry& entry : model_.listing().entries()) {
        if (selected_.contains(entry.name)) {
            names.push_back(entry.name);
        }
    }
    return names;
}

const SelectionSummary& Column::summary() const {
    if (!summary_) {
        SelectionSummary summary;
        for (const fs::Entry& entry : model_.listing().entries()) {
            if (!selected_.contains(entry.name)) {
                continue;
            }
            if (entry.is_directory()) {
                ++summary.folders;
            } else {
                ++summary.files;
                summary.bytes += entry.size;
            }
        }
        summary_ = summary;
    }
    return *summary_;
}

void Column::selection_changed() {
    summary_.reset();
    redraw_();
    changed_();
}

void Column::on_change(const fs::ModelChange& change) {
    for (const std::string& name : change.removed) {
        sniffed_.erase(name);
        selected_.erase(name);
    }
    for (const std::string& name : change.updated) {
        sniffed_.erase(name);  // the content may be different now
    }
    for (const auto& [from, to] : change.renamed) {
        sniffed_.erase(from);
        if (selected_.erase(from) > 0) {
            selected_.insert(to);
        }
        if (from == cursor_name_) {
            cursor_name_ = to;
        }
        if (from == anchor_) {
            anchor_ = to;
        }
    }
    hovered_.reset();  // rows moved; the next motion finds the right one

    const bool had_cursor = !cursor_name_.empty();
    const bool had_selection = !selected_.empty();
    // A reset (reload after lost events) is followed by a new load: wait for it.
    const bool final_listing = model_.error() || (!change.reset && !model_.loading());
    const std::vector<fs::Entry>& entries = model_.listing().entries();
    const std::optional<std::size_t> before = cursor_index_;
    cursor_index_.reset();
    if (had_cursor) {
        const auto it = std::ranges::find(entries, cursor_name_, &fs::Entry::name);
        if (it != entries.end()) {
            cursor_index_ = static_cast<std::size_t>(it - entries.begin());
            if (!before) {
                jump_to_cursor_ = true;  // it just landed
            }
        } else if (final_listing) {
            cursor_name_.clear();  // gone, or it never came
        }
    }
    if (had_selection && final_listing && (change.loaded || model_.error())) {
        // After a (re)load, names that did not come back are gone.
        std::unordered_set<std::string> present;
        for (const fs::Entry& entry : entries) {
            if (selected_.contains(entry.name)) {
                present.insert(entry.name);
            }
        }
        selected_ = std::move(present);
    }
    if (had_cursor || had_selection) {
        selection_changed();
    } else {
        redraw_();
    }
}

void Column::set_cursor(std::optional<std::size_t> row, Clock::time_point now, SelectMode mode) {
    const std::vector<fs::Entry>& entries = model_.listing().entries();
    jump_to_cursor_ = false;
    if (!row || entries.empty()) {
        if (cursor_name_.empty() && selected_.empty()) {
            return;
        }
        cursor_name_.clear();
        cursor_index_.reset();
        anchor_.clear();
        selected_.clear();
        selection_changed();
        return;
    }
    const std::size_t index = std::min(*row, entries.size() - 1);
    const std::string& name = entries[index].name;
    switch (mode) {
        case SelectMode::replace:
            selected_ = {name};
            anchor_ = name;
            break;
        case SelectMode::extend: {
            const auto anchor = std::ranges::find(entries, anchor_, &fs::Entry::name);
            const std::size_t from = anchor != entries.end()
                                         ? static_cast<std::size_t>(anchor - entries.begin())
                                         : cursor_index_.value_or(index);
            anchor_ = entries[from].name;
            selected_.clear();
            for (std::size_t i = std::min(from, index); i <= std::max(from, index); ++i) {
                selected_.insert(entries[i].name);
            }
            break;
        }
        case SelectMode::keep:
            break;
        case SelectMode::toggle:
            if (selected_.erase(name) == 0) {
                selected_.insert(name);
            }
            anchor_ = name;
            break;
    }
    cursor_name_ = name;
    cursor_index_ = index;
    reveal_cursor(now, true);
    selection_changed();
}

void Column::move_cursor(std::ptrdiff_t rows, Clock::time_point now, SelectMode mode) {
    const std::size_t count = model_.listing().entries().size();
    if (count == 0) {
        return;
    }
    if (!cursor_index_) {
        set_cursor(0, now, mode == SelectMode::keep ? SelectMode::keep : SelectMode::replace);
        return;
    }
    const auto last = static_cast<std::ptrdiff_t>(count) - 1;
    const std::ptrdiff_t target =
        std::clamp(static_cast<std::ptrdiff_t>(*cursor_index_) + rows, std::ptrdiff_t{0}, last);
    set_cursor(static_cast<std::size_t>(target), now, mode);
}

void Column::select_name(std::string name) {
    cursor_name_ = std::move(name);
    cursor_index_.reset();
    const std::vector<fs::Entry>& entries = model_.listing().entries();
    const auto it = std::ranges::find(entries, cursor_name_, &fs::Entry::name);
    if (it != entries.end()) {
        cursor_index_ = static_cast<std::size_t>(it - entries.begin());
    } else if (!model_.loading()) {
        cursor_name_.clear();
    }
    selected_.clear();
    if (!cursor_name_.empty()) {
        selected_.insert(cursor_name_);
    }
    anchor_ = cursor_name_;
    jump_to_cursor_ = cursor_index_.has_value();
    selection_changed();
}

void Column::select_all() {
    const std::vector<fs::Entry>& entries = model_.listing().entries();
    if (entries.empty()) {
        return;
    }
    for (const fs::Entry& entry : entries) {
        selected_.insert(entry.name);
    }
    if (!cursor_index_) {
        cursor_index_ = 0;
        cursor_name_ = entries.front().name;
        anchor_ = cursor_name_;
    }
    selection_changed();
}

void Column::collapse_selection() {
    selected_.clear();
    if (cursor_index_) {
        selected_.insert(cursor_name_);
        anchor_ = cursor_name_;
    }
    selection_changed();
}

std::size_t Column::page_rows() const {
    const auto rows = static_cast<std::size_t>(rows_area_.height / kRowHeight);
    return std::max<std::size_t>(1, rows > 1 ? rows - 1 : rows);
}

void Column::reveal_cursor(Clock::time_point now, bool animate) {
    const float viewport = rows_area_.height;
    if (!cursor_index_) {
        return;
    }
    if (viewport <= 0.0F) {
        jump_to_cursor_ = true;  // not drawn yet
        return;
    }
    const float top = static_cast<float>(*cursor_index_) * kRowHeight;
    const float bottom = top + kRowHeight;
    const float from = scroller_.destination();
    if (top >= from && bottom <= from + viewport) {
        return;
    }
    if (!animate) {
        scroller_.jump_to(top - ((viewport - kRowHeight) / 2.0F));  // centered
    } else if (top < from) {
        scroller_.glide_to(top, now);
    } else {
        scroller_.glide_to(bottom - viewport, now);
    }
}

fs::MimeId Column::mime_of(const fs::Entry& entry) const {
    if (entry.mime != fs::kUnknownMime) {
        return entry.mime;
    }
    const auto it = sniffed_.find(entry.name);
    return it == sniffed_.end() ? fs::kUnknownMime : it->second;
}

void Column::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                  ui::IconSet& icons, bool focused, Clock::time_point now) {
    const std::vector<fs::Entry>& entries = model_.listing().entries();
    rows_area_ = bounds;
    const float content = static_cast<float>(entries.size()) * kRowHeight;
    scroller_.set_extent(content, bounds.height);
    if (jump_to_cursor_ && cursor_index_) {
        reveal_cursor(now, false);
        jump_to_cursor_ = false;
    }
    scroller_.advance(now);
    const float offset = scroller_.offset();
    const RowRange range = visible_rows(entries.size(), kRowHeight, offset, bounds.height);

    renderer.push_clip(bounds);
    const float name_x = bounds.x + kIconX + kIconSize + kIconGap;
    const bool cursor_ring = single_selection() == nullptr;
    for (std::size_t i = range.first; i < range.last; ++i) {
        const fs::Entry& entry = entries[i];
        const float y = bounds.y + (static_cast<float>(i) * kRowHeight) - offset;
        const RowStyle style = row_style(palette, entry,
                                         {.selected = selected_.contains(entry.name),
                                          .cursor = cursor_index_ == i,
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
        typed.mime = mime_of(entry);
        icons.draw(renderer, fs::icon_name(*mime_, typed), bounds.x + kIconX,
                   y + ((kRowHeight - kIconSize) / 2.0F), kIconSize, style.icon);
        float name_right = bounds.right() - kRightPadding;
        if (entry.is_directory()) {
            name_right -= kChevronSize + 6.0F;
            icons.draw(renderer, "chevron-right", bounds.right() - kRightPadding - kChevronSize,
                       y + ((kRowHeight - kChevronSize) / 2.0F), kChevronSize, style.chevron);
        }
        renderer.draw_text(entry.name, name_x, y + ((kRowHeight - kTextSize) / 2.0F) - 2.0F,
                           kTextSize, style.text, std::max(1.0F, name_right - name_x),
                           ui::Ellipsis::middle);
    }

    if (model_.error()) {
        draw_centered(renderer, model_.error().message(), bounds, bounds.y + 24.0F, palette.error);
    } else if (entries.empty() && !model_.loading()) {
        draw_centered(renderer, "Empty folder", bounds, bounds.y + 24.0F,
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

    sniff(range.first, range.last);
}

void Column::sniff(std::size_t first, std::size_t last) {
    const std::vector<fs::Entry>& entries = model_.listing().entries();
    std::vector<fs::Entry> batch;
    for (std::size_t i = first; i < last && batch.size() < kMaxSniffBatch; ++i) {
        const fs::Entry& entry = entries[i];
        if (entry.mime == fs::kUnknownMime && !sniffed_.contains(entry.name) &&
            !sniffing_.contains(entry.name)) {
            sniffing_.insert(entry.name);
            batch.push_back(entry);
        }
    }
    if (batch.empty()) {
        return;
    }
    pool_.submit(
        core::Priority::visible,
        [directory = model_.path(), mime = mime_, batch](const core::CancelToken& token) {
            std::vector<std::pair<std::string, fs::MimeId>> types;
            types.reserve(batch.size());
            for (const fs::Entry& entry : batch) {
                if (token.cancelled()) {
                    break;
                }
                types.emplace_back(entry.name, mime->detect(directory, entry));
            }
            return types;
        },
        [this, alive = alive_](std::vector<std::pair<std::string, fs::MimeId>> types) {
            if (!*alive) {
                return;  // the column was closed
            }
            for (auto& [name, type] : types) {
                sniffing_.erase(name);
                sniffed_[std::move(name)] = type;
            }
            redraw_();
        });
}

std::optional<std::size_t> Column::row_at(float x, float y) const {
    if (x < rows_area_.x || x >= rows_area_.right() || y < rows_area_.y ||
        y >= rows_area_.bottom()) {
        return std::nullopt;
    }
    const float content_y = y - rows_area_.y + scroller_.offset();
    const auto row = static_cast<std::size_t>(content_y / kRowHeight);
    if (row >= model_.listing().entries().size()) {
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
