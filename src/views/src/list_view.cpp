#include "nordend/views/list_view.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "nordend/fs/icon.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

constexpr float kPadding = 16.0F;
constexpr float kTitleHeight = 40.0F;
constexpr float kHeaderHeight = 26.0F;
constexpr float kIconSize = 18.0F;
constexpr float kIconGap = 10.0F;
constexpr float kSizeWidth = 90.0F;
constexpr float kDateWidth = 140.0F;
constexpr float kColumnGap = 16.0F;
constexpr float kTextSize = 14.0F;
constexpr float kSmallTextSize = 12.0F;
constexpr float kScrollbarWidth = 4.0F;
constexpr float kScrollbarMinThumb = 24.0F;
// Sniffing batches stay small so a fast scroll does not queue minutes of reads.
constexpr std::size_t kMaxSniffBatch = 64;

// Text width in logical pixels.
float text_width(ui::Renderer& renderer, std::string_view text, float size) {
    return renderer.fonts().layout(text, size, renderer.scale()).width / renderer.scale();
}

void draw_right_aligned(ui::Renderer& renderer, std::string_view text, float right_x, float top_y,
                        float font_size, ui::Color color) {
    renderer.draw_text(text, right_x - text_width(renderer, text, font_size), top_y, font_size,
                       color);
}

}  // namespace

ListView::ListView(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                   std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw)
    : pool_{pool},
      mime_{std::move(mime)},
      redraw_{std::move(redraw)},
      model_{loop, pool, watcher, mime_} {
    model_.set_change_handler([this](const fs::ModelChange& change) { on_change(change); });
}

ListView::~ListView() { *alive_ = false; }

void ListView::open(const std::filesystem::path& directory) {
    ++generation_;
    sniffed_.clear();
    sniffing_.clear();
    hovered_.reset();
    scroller_.jump_to(0.0F);
    model_.open(directory);
}

void ListView::on_change(const fs::ModelChange& change) {
    for (const std::string& name : change.removed) {
        sniffed_.erase(name);
    }
    for (const std::string& name : change.updated) {
        sniffed_.erase(name);  // the content may be different now
    }
    for (const auto& [from, to] : change.renamed) {
        sniffed_.erase(from);
    }
    hovered_.reset();  // rows moved; the next motion finds the right one
    redraw_();
}

float ListView::content_height() const {
    return static_cast<float>(model_.listing().entries().size()) * kRowHeight;
}

fs::MimeId ListView::mime_of(const fs::Entry& entry) const {
    if (entry.mime != fs::kUnknownMime) {
        return entry.mime;
    }
    const auto it = sniffed_.find(entry.name);
    return it == sniffed_.end() ? fs::kUnknownMime : it->second;
}

void ListView::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    ui::IconSet& icons, Clock::time_point now) {
    const std::vector<fs::Entry>& entries = model_.listing().entries();

    // Title: the folder and its state.
    const float title_y = bounds.y + 12.0F;
    std::string status;
    if (model_.error()) {
        status = model_.error().message();
    } else if (model_.loading()) {
        status = std::format("{} items, loading…", entries.size());
    } else {
        status = std::format("{} items", entries.size());
    }
    const float status_width = text_width(renderer, status, kSmallTextSize);
    draw_right_aligned(renderer, status, bounds.right() - kPadding, title_y + 2.0F, kSmallTextSize,
                       palette.on_surface_variant);
    renderer.draw_text(model_.path().string(), bounds.x + kPadding, title_y, 15.0F,
                       palette.on_surface, bounds.width - (3.0F * kPadding) - status_width,
                       ui::Ellipsis::middle);

    // Column headers.
    const float header_y = bounds.y + kTitleHeight;
    const float date_column_end = bounds.right() - kPadding;
    const float bytes_column_end = date_column_end - kDateWidth - kColumnGap;
    const float name_x = bounds.x + kPadding + kIconSize + kIconGap;
    const float name_width = bytes_column_end - kSizeWidth - kColumnGap - name_x;
    renderer.draw_text("Name", name_x, header_y + 6.0F, kSmallTextSize, palette.on_surface_variant);
    draw_right_aligned(renderer, "Size", bytes_column_end, header_y + 6.0F, kSmallTextSize,
                       palette.on_surface_variant);
    renderer.draw_text("Modified", date_column_end - kDateWidth, header_y + 6.0F, kSmallTextSize,
                       palette.on_surface_variant);
    renderer.fill_rect({bounds.x + kPadding, header_y + kHeaderHeight - 1.0F,
                        bounds.width - (2.0F * kPadding), 1.0F / renderer.scale()},
                       palette.outline_variant);

    // Rows: only the visible ones.
    rows_area_ = {bounds.x, header_y + kHeaderHeight, bounds.width,
                  std::max(0.0F, bounds.bottom() - header_y - kHeaderHeight)};
    scroller_.set_extent(content_height(), rows_area_.height);
    scroller_.advance(now);
    if (pointer_inside_) {
        hovered_ = row_at(pointer_x_, pointer_y_);  // content moves under a still pointer
    }
    const float offset = scroller_.offset();
    const RowRange range = visible_rows(entries.size(), kRowHeight, offset, rows_area_.height);

    renderer.push_clip(rows_area_);
    for (std::size_t i = range.first; i < range.last; ++i) {
        const fs::Entry& entry = entries[i];
        const float y = rows_area_.y + (static_cast<float>(i) * kRowHeight) - offset;
        if (hovered_ == i) {
            renderer.draw_box(
                {bounds.x + (kPadding / 2.0F), y + 1.0F, bounds.width - kPadding,
                 kRowHeight - 2.0F},
                {.fill = palette.surface_container_high, .radius = ui::Corners::all(6.0F)});
        }
        fs::Entry typed = entry;
        typed.mime = mime_of(entry);
        const std::string_view icon = fs::icon_name(*mime_, typed);
        icons.draw(renderer, icon, bounds.x + kPadding, y + ((kRowHeight - kIconSize) / 2.0F),
                   kIconSize, entry.is_directory() ? palette.primary : palette.on_surface_variant);
        const float text_y = y + ((kRowHeight - kTextSize) / 2.0F) - 2.0F;
        renderer.draw_text(entry.name, name_x, text_y, kTextSize,
                           entry.hidden() ? palette.outline : palette.on_surface, name_width,
                           ui::Ellipsis::middle);
        if (!entry.is_directory()) {
            draw_right_aligned(renderer, format_size(entry.size), bytes_column_end, text_y + 1.0F,
                               kSmallTextSize + 1.0F, palette.on_surface_variant);
        }
        renderer.draw_text(format_time(entry.modified_ns), date_column_end - kDateWidth,
                           text_y + 1.0F, kSmallTextSize + 1.0F, palette.on_surface_variant);
    }
    renderer.pop_clip();

    if (entries.empty() && !model_.loading() && !model_.error()) {
        const std::string_view empty = "Empty folder";
        renderer.draw_text(
            empty,
            rows_area_.x + ((rows_area_.width - text_width(renderer, empty, kTextSize)) / 2.0F),
            rows_area_.y + 40.0F, kTextSize, palette.on_surface_variant);
    }

    // Scrollbar: a thin thumb, only when there is something to scroll.
    if (scroller_.max_offset() > 0.0F) {
        const float content = content_height();
        const float thumb =
            std::max(kScrollbarMinThumb, rows_area_.height * rows_area_.height / content);
        const float travel = rows_area_.height - thumb;
        const float thumb_y = rows_area_.y + (travel * offset / scroller_.max_offset());
        renderer.draw_box(
            {bounds.right() - kScrollbarWidth - 3.0F, thumb_y, kScrollbarWidth, thumb},
            {.fill = palette.on_surface_variant.with_alpha(0.45F),
             .radius = ui::Corners::all(kScrollbarWidth / 2.0F)});
    }

    sniff(range.first, range.last);
}

void ListView::sniff(std::size_t first, std::size_t last) {
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
        [this, alive = alive_,
         generation = generation_](std::vector<std::pair<std::string, fs::MimeId>> types) {
            if (!*alive || generation != generation_) {
                return;  // another folder is open now
            }
            for (auto& [name, type] : types) {
                sniffing_.erase(name);
                sniffed_[std::move(name)] = type;
            }
            redraw_();
        });
}

std::optional<std::size_t> ListView::row_at(float x, float y) const {
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

void ListView::pointer(const ui::PointerEvent& event, Clock::time_point now) {
    using Kind = ui::PointerEvent::Kind;
    switch (event.kind) {
        case Kind::enter:
        case Kind::motion: {
            pointer_inside_ = true;
            pointer_x_ = event.x;
            pointer_y_ = event.y;
            const std::optional<std::size_t> row = row_at(event.x, event.y);
            if (row != hovered_) {
                hovered_ = row;
                redraw_();
            }
            break;
        }
        case Kind::leave:
            pointer_inside_ = false;
            if (hovered_) {
                hovered_.reset();
                redraw_();
            }
            break;
        case Kind::scroll:
            if (event.scroll_finger || event.wheel_y == 0.0F) {
                // Touchpad (and other continuous sources): follow 1:1, inertia on release.
                if (event.scroll_y != 0.0F) {
                    scroller_.finger(event.scroll_y, now);
                }
                if (event.scroll_stop && event.scroll_finger) {
                    scroller_.release(now);
                }
            } else {
                scroller_.wheel(event.wheel_y, now);
            }
            redraw_();
            break;
        case Kind::press:
            if (event.button == ui::PointerButton::left && event.click_count == 2) {
                const std::optional<std::size_t> row = row_at(event.x, event.y);
                if (row && model_.listing().entries()[*row].is_directory()) {
                    open(model_.path() / model_.listing().entries()[*row].name);
                }
            } else if (event.button == ui::PointerButton::back) {
                open(model_.path().parent_path());
            }
            break;
        case Kind::release:
            break;
    }
}

void ListView::key(const ui::KeyEvent& event, Clock::time_point now) {
    const float page = std::max(kRowHeight, rows_area_.height - kRowHeight);
    // Keys glide like the wheel; scroller steps are in notches.
    const float notch = ui::Scroller::kWheelStep;
    switch (event.keysym) {
        case XKB_KEY_Down:
            scroller_.wheel(kRowHeight / notch, now);
            break;
        case XKB_KEY_Up:
            if (event.modifiers.alt) {
                open(model_.path().parent_path());
                return;
            }
            scroller_.wheel(-kRowHeight / notch, now);
            break;
        case XKB_KEY_Page_Down:
            scroller_.wheel(page / notch, now);
            break;
        case XKB_KEY_Page_Up:
            scroller_.wheel(-page / notch, now);
            break;
        case XKB_KEY_Home:
            scroller_.jump_to(0.0F);
            break;
        case XKB_KEY_End:
            scroller_.jump_to(scroller_.max_offset());
            break;
        case XKB_KEY_BackSpace:
            open(model_.path().parent_path());
            return;
        default:
            return;
    }
    redraw_();
}

}  // namespace nordend::views
