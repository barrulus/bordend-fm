#include "nordend/views/details_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "draw_helpers.hpp"
#include "item_style.hpp"
#include "nordend/fs/icon.hpp"
#include "nordend/ui/text_field.hpp"
#include "nordend/views/format.hpp"

namespace nordend::views {
namespace {

using Field = DetailsView::Field;

constexpr float kPadding = 8.0F;       // either side of the table
constexpr float kCellPadding = 10.0F;  // inside each column
constexpr float kRowInset = 2.0F;      // the highlight box, from the row's edges
constexpr float kIconSize = 18.0F;
constexpr float kIconGap = 10.0F;
constexpr float kNameSize = 14.0F;
constexpr float kDetailSize = 13.0F;
constexpr float kTitleSize = 12.0F;
constexpr float kSortIcon = 12.0F;
constexpr float kDividerGrab = 4.0F;  // either side of a divider, for the pointer
constexpr float kScrollbarWidth = 4.0F;
constexpr float kScrollbarMinThumb = 24.0F;
// The least a column can be made: the name keeps room for a short file name.
constexpr std::array<float, 4> kMinWidths{160.0F, 64.0F, 96.0F, 80.0F};
constexpr std::array<std::string_view, 4> kTitles{"Name", "Size", "Modified", "Type"};

constexpr std::size_t index(Field field) { return static_cast<std::size_t>(field); }

fs::SortKey sort_key(Field field) {
    switch (field) {
        case Field::size:
            return fs::SortKey::size;
        case Field::modified:
            return fs::SortKey::modified;
        case Field::type:
            return fs::SortKey::type;
        case Field::name:
            break;
    }
    return fs::SortKey::name;
}

bool same_order(const fs::SortOrder& a, const fs::SortOrder& b) {
    return a.key == b.key && a.descending == b.descending &&
           a.directories_first == b.directories_first && a.mime == b.mime;
}

}  // namespace

DetailsView::DetailsView(core::EventLoop& loop, core::WorkerPool& pool,
                         fs::DirectoryWatcher& watcher,
                         std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                         std::function<void(ui::CursorShape)> set_cursor, InfoPane* info_pane)
    : loop_{loop},
      pool_{pool},
      watcher_{watcher},
      mime_{std::move(mime)},
      redraw_{std::move(redraw)},
      set_cursor_{std::move(set_cursor)},
      info_pane_{info_pane} {
    order_.mime = mime_.get();
}

void DetailsView::show(std::shared_ptr<Folder> folder) {
    folder_ = std::move(folder);
    scroller_.jump_to(0.0F);
    hovered_.reset();
    drag_.reset();
    if (folder_) {
        apply_order();
        folder_->request_jump();
    }
    redraw_();
}

void DetailsView::open(const std::filesystem::path& directory) {
    show(std::make_shared<Folder>(loop_, pool_, watcher_, mime_, directory, redraw_, show_hidden_));
}

void DetailsView::set_show_hidden(bool show) {
    show_hidden_ = show;
    if (folder_) {
        folder_->set_show_hidden(show);
    }
}

bool DetailsView::loading() const {
    return folder_ && (folder_->model().loading() || folder_->cursor_pending());
}

void DetailsView::sort_by(Field field) {
    const fs::SortKey key = sort_key(field);
    if (order_.key == key) {
        order_.descending = !order_.descending;
    } else {
        order_.key = key;
        order_.descending = false;
    }
    apply_order();
    redraw_();
}

void DetailsView::apply_order() {
    if (folder_ && !same_order(folder_->order(), order_)) {
        folder_->set_order(order_);
    }
}

float DetailsView::width(Field field) const {
    return field == Field::name ? layout_.width.at(index(Field::name)) : widths_.at(index(field));
}

void DetailsView::set_info_visible(bool visible) {
    info_visible_ = visible;
    redraw_();
}

void DetailsView::lay_out() {
    // What fits: the fixed columns are left out, last first, while the name would be narrower
    // than its minimum.
    float fixed = 0.0F;
    for (const Field field : {Field::size, Field::modified, Field::type}) {
        layout_.shown.at(index(field)) = true;
        fixed += widths_.at(index(field));
    }
    const float table = rows_.width - (2.0F * kPadding);
    for (const Field field : {Field::type, Field::modified, Field::size}) {
        if (table - fixed >= kMinWidths.at(index(Field::name))) {
            break;
        }
        layout_.shown.at(index(field)) = false;
        fixed -= widths_.at(index(field));
    }
    // The fixed columns from the right edge; the name takes what is left.
    float right = rows_.right() - kPadding;
    for (const Field field : {Field::type, Field::modified, Field::size}) {
        const std::size_t i = index(field);
        if (!layout_.shown.at(i)) {
            layout_.width.at(i) = 0.0F;
            layout_.left.at(i) = right;
            continue;
        }
        layout_.width.at(i) = widths_.at(i);
        layout_.left.at(i) = right - widths_.at(i);
        right = layout_.left.at(i);
    }
    layout_.left.at(index(Field::name)) = rows_.x + kPadding;
    layout_.width.at(index(Field::name)) = std::max(1.0F, right - (rows_.x + kPadding));
}

void DetailsView::enter() {
    const fs::Entry* const entry = folder_->single_selection();
    if (entry == nullptr || !entry->is_directory()) {
        return;
    }
    open(folder_->path_of(*entry));
}

void DetailsView::go_up() {
    const std::filesystem::path current = folder_->path();
    const std::filesystem::path parent = current.parent_path();
    if (parent == current || current.filename().empty()) {
        return;  // at the root
    }
    open(parent);
    folder_->select_name(current.filename().string());
}

void DetailsView::reveal_cursor(Folder::Reveal reveal, Clock::time_point now) {
    const std::optional<std::size_t> cursor = folder_->cursor();
    const float viewport = rows_.height;
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

void DetailsView::draw(ui::Renderer& renderer, const ui::Rect& bounds,
                       const theme::Palette& palette, ui::IconSet& icons, Clock::time_point now) {
    if (!folder_) {
        return;
    }
    const float top = bounds.y;
    const float pane_width =
        info_visible_ && info_pane_ != nullptr ? info_pane_->width_in(bounds.width) : 0.0F;
    const bool pane = pane_width > 0.0F;
    const float view_width = bounds.width - pane_width;
    pane_edge_ = pane ? bounds.x + view_width : -1.0F;
    header_ = {bounds.x, top, view_width, kHeaderHeight};
    rows_ = {bounds.x, header_.bottom(), view_width,
             std::max(0.0F, bounds.bottom() - header_.bottom())};
    lay_out();

    // The header: titles, the sort direction, the dividers.
    renderer.fill_rect({header_.x, header_.bottom() - 1.0F, header_.width, 1.0F},
                       palette.outline_variant);
    const fs::SortOrder& order = folder_->order();
    for (const Field field : {Field::name, Field::size, Field::modified, Field::type}) {
        const std::size_t i = index(field);
        if (!layout_.shown.at(i)) {
            continue;
        }
        const float left = layout_.left.at(i);
        const float width = layout_.width.at(i);
        const bool sorted = sort_key(field) == order.key;
        const ui::Color color = sorted ? palette.on_surface : palette.on_surface_variant;
        const std::string_view title = kTitles.at(i);
        const float title_width = text_width(renderer, title, kTitleSize);
        const float text_y = header_.y + ((kHeaderHeight - kTitleSize) / 2.0F) - 2.0F;
        // Sizes are numbers: right-aligned, title included.
        const float title_x =
            field == Field::size ? left + width - kCellPadding - title_width : left + kCellPadding;
        renderer.draw_text(title, title_x, text_y, kTitleSize, color);
        if (sorted) {
            const float icon_x =
                field == Field::size ? title_x - 4.0F - kSortIcon : title_x + title_width + 4.0F;
            icons.draw(renderer, order.descending ? "chevron-down" : "chevron-up", icon_x,
                       header_.y + ((kHeaderHeight - kSortIcon) / 2.0F), kSortIcon, color);
        }
        if (field != Field::name) {
            const bool active = drag_ && drag_->field == field;
            renderer.fill_rect({left, header_.y + 8.0F, 1.0F, kHeaderHeight - 16.0F},
                               active ? palette.primary : palette.outline_variant);
        }
    }

    // The rows.
    const std::vector<fs::Entry>& entries = folder_->entries();
    const float content = static_cast<float>(entries.size()) * kRowHeight;
    scroller_.set_extent(content, rows_.height);
    reveal_cursor(folder_->take_reveal(), now);
    scroller_.advance(now);
    const float offset = scroller_.offset();
    const RowRange range = visible_rows(entries.size(), kRowHeight, offset, rows_.height);

    renderer.push_clip(rows_);
    const std::optional<std::size_t> cursor = folder_->cursor();
    const bool cursor_ring = folder_->single_selection() == nullptr;
    const float table_left = rows_.x + kPadding;
    const float table_width = rows_.width - (2.0F * kPadding);
    for (std::size_t i = range.first; i < range.last; ++i) {
        const fs::Entry& entry = entries[i];
        const float y = rows_.y + (static_cast<float>(i) * kRowHeight) - offset;
        const bool selected = folder_->is_selected(entry.name);
        const ItemStyle style =
            item_style(palette, entry,
                       {.selected = selected,
                        .cursor = cursor == i,
                        .ring = cursor_ring,
                        .focused = true,
                        .hovered = hovered_ == i,
                        .cut = cut_ != nullptr && cut_->holds(folder_->path(), entry)});
        if (style.fill || style.ring) {
            renderer.draw_box(
                {table_left, y + kRowInset, table_width, kRowHeight - (2.0F * kRowInset)},
                {.fill = style.fill.value_or(kTransparent),
                 .radius = ui::Corners::all(6.0F),
                 .border_width = style.ring ? 1.5F : 0.0F,
                 .border = style.ring.value_or(kTransparent)});
        }
        // The details are quieter than the name, except on the strong selection.
        const ui::Color detail = selected ? style.text : palette.on_surface_variant;
        fs::Entry typed = entry;
        typed.mime = folder_->mime_of(entry);

        const float name_left = layout_.left.at(index(Field::name)) + kCellPadding;
        icons.draw(renderer, fs::icon_name(*mime_, typed), name_left,
                   y + ((kRowHeight - kIconSize) / 2.0F), kIconSize, style.icon);
        const float name_x = name_left + kIconSize + kIconGap;
        const float name_room = layout_.left.at(index(Field::name)) +
                                layout_.width.at(index(Field::name)) - kCellPadding - name_x;
        if (edit_ != nullptr && edit_->holds(*folder_, entry)) {
            edit_->place(
                {name_x - ui::TextField::kPadding, y + 3.0F,
                 std::max(1.0F, name_room) + (2.0F * ui::TextField::kPadding), kRowHeight - 6.0F},
                kNameSize);
        } else {
            renderer.draw_text(entry.name, name_x, y + ((kRowHeight - kNameSize) / 2.0F) - 2.0F,
                               kNameSize, style.text, std::max(1.0F, name_room),
                               ui::Ellipsis::middle);
        }

        const float detail_y = y + ((kRowHeight - kDetailSize) / 2.0F) - 2.0F;
        const auto cell = [&](Field field, const std::string& text, bool right) {
            if (!layout_.shown.at(index(field))) {
                return;
            }
            const float left = layout_.left.at(index(field)) + kCellPadding;
            const float room =
                std::max(1.0F, layout_.width.at(index(field)) - (2.0F * kCellPadding));
            if (right) {
                const float width = std::min(text_width(renderer, text, kDetailSize), room);
                renderer.draw_text(text, left + room - width, detail_y, kDetailSize, detail, room);
            } else {
                renderer.draw_text(text, left, detail_y, kDetailSize, detail, room);
            }
        };
        cell(Field::size, entry.is_directory() ? std::string{"—"} : format_size(entry.size), true);
        cell(Field::modified, format_time(entry.modified_ns), false);
        cell(Field::type,
             typed.mime == fs::kUnknownMime ? std::string{"…"}
                                            : std::string{mime_->name(typed.mime)},
             false);
    }

    const fs::DirectoryModel& model = folder_->model();
    if (model.error()) {
        draw_centered(renderer, model.error().message(), rows_, rows_.y + 24.0F, kNameSize,
                      palette.error);
    } else if (entries.empty() && !model.loading()) {
        draw_centered(renderer, "Empty folder", rows_, rows_.y + 24.0F, kNameSize,
                      palette.on_surface_variant);
    }

    // Scrollbar: a thin thumb, only when there is something to scroll.
    if (scroller_.max_offset() > 0.0F) {
        const float thumb = std::max(kScrollbarMinThumb, rows_.height * rows_.height / content);
        const float travel = rows_.height - thumb;
        const float thumb_y = rows_.y + (travel * offset / scroller_.max_offset());
        renderer.draw_box({rows_.right() - kScrollbarWidth - 2.0F, thumb_y, kScrollbarWidth, thumb},
                          {.fill = palette.on_surface_variant.with_alpha(0.45F),
                           .radius = ui::Corners::all(kScrollbarWidth / 2.0F)});
    }
    renderer.pop_clip();
    if (pane) {
        const float pane_top = header_.y;
        const float pane_height = bounds.bottom() - pane_top;
        info_pane_->draw(renderer, {header_.right(), pane_top, pane_width, pane_height}, palette,
                         icons, *folder_, now);
        const bool grabbed = pane_drag_ || (over_divider_ && on_pane_edge(pointer_x_, pointer_y_));
        const float line = grabbed ? 2.0F : 1.0F / renderer.scale();
        renderer.fill_rect({header_.right() - (grabbed ? 1.0F : 0.0F), pane_top, line, pane_height},
                           grabbed ? palette.primary : palette.outline_variant);
    }

    folder_->sniff(range.first, range.last);
    update_hover();  // content may have moved under a still pointer
}

std::optional<std::size_t> DetailsView::row_at(float x, float y) const {
    if (!folder_ || x < rows_.x || x >= rows_.right() || y < rows_.y || y >= rows_.bottom()) {
        return std::nullopt;
    }
    const auto row = static_cast<std::size_t>((y - rows_.y + scroller_.offset()) / kRowHeight);
    if (row >= folder_->entries().size()) {
        return std::nullopt;
    }
    return row;
}

std::optional<Field> DetailsView::divider_at(float x, float y) const {
    if (y < header_.y || y >= header_.bottom()) {
        return std::nullopt;
    }
    for (const Field field : {Field::size, Field::modified, Field::type}) {
        if (layout_.shown.at(index(field)) &&
            std::abs(x - layout_.left.at(index(field))) <= kDividerGrab) {
            return field;
        }
    }
    return std::nullopt;
}

std::optional<Field> DetailsView::title_at(float x, float y) const {
    if (y < header_.y || y >= header_.bottom()) {
        return std::nullopt;
    }
    for (const Field field : {Field::name, Field::size, Field::modified, Field::type}) {
        const float left = layout_.left.at(index(field));
        if (layout_.shown.at(index(field)) && x >= left &&
            x < left + layout_.width.at(index(field))) {
            return field;
        }
    }
    return std::nullopt;
}

bool DetailsView::on_pane_edge(float x, float y) const {
    return pane_edge_ >= 0.0F && std::abs(x - pane_edge_) <= InfoPane::kEdgeGrab &&
           y >= header_.y && y < rows_.bottom();
}

void DetailsView::update_hover() {
    const bool dragging = drag_ || pane_drag_;
    const std::optional<std::size_t> row =
        pointer_inside_ && !dragging ? row_at(pointer_x_, pointer_y_) : std::nullopt;
    if (row != hovered_) {
        hovered_ = row;
        redraw_();
    }
    const bool over = dragging || (pointer_inside_ && (divider_at(pointer_x_, pointer_y_) ||
                                                       on_pane_edge(pointer_x_, pointer_y_)));
    if (over != over_divider_) {
        over_divider_ = over;
        set_cursor_(over ? ui::CursorShape::ew_resize : ui::CursorShape::standard);
        redraw_();
    }
}

void DetailsView::pointer(const ui::PointerEvent& event, Clock::time_point now) {
    if (!folder_) {
        return;
    }
    using Kind = ui::PointerEvent::Kind;
    switch (event.kind) {
        case Kind::enter:
        case Kind::motion:
            pointer_inside_ = true;
            pointer_x_ = event.x;
            pointer_y_ = event.y;
            if (pane_drag_ && info_pane_ != nullptr) {
                info_pane_->set_width(pane_drag_->start_width + (pane_drag_->start_x - event.x));
            }
            if (drag_) {
                // The divider follows the pointer: the column on its right grows to the left.
                const std::size_t i = index(drag_->field);
                float others = 0.0F;
                for (const Field field : {Field::size, Field::modified, Field::type}) {
                    others += field == drag_->field ? 0.0F : widths_.at(index(field));
                }
                const float most = rows_.width - (2.0F * kPadding) - others - kMinWidths.at(0);
                widths_.at(i) = std::clamp(drag_->start_width + (drag_->start_x - event.x),
                                           kMinWidths.at(i), std::max(kMinWidths.at(i), most));
                redraw_();
            }
            update_hover();
            break;
        case Kind::leave:
            pointer_inside_ = false;
            update_hover();
            break;
        case Kind::scroll:
            if (event.x >= header_.right() && info_pane_ != nullptr &&
                info_pane_->scroll(event, now)) {
                break;
            }
            if (event.wheel_y != 0.0F) {
                scroller_.wheel(event.wheel_y, now);
            } else if (event.scroll_y != 0.0F) {
                scroller_.finger(event.scroll_y, now);
            }
            if (event.scroll_finger && event.scroll_stop) {
                scroller_.release(now);
            }
            redraw_();
            break;
        case Kind::press: {
            if (event.button == ui::PointerButton::right) {
                right_click(event.x, event.y);
                break;
            }
            if (event.button != ui::PointerButton::left) {
                break;
            }
            if (info_pane_ != nullptr && on_pane_edge(event.x, event.y)) {
                if (event.click_count == 2) {
                    info_pane_->set_width(InfoPane::kWidth);  // double click: the default
                } else {
                    pane_drag_ = PaneDrag{.start_x = event.x, .start_width = info_pane_->width()};
                }
                update_hover();
                break;
            }
            if (event.x >= header_.right()) {
                if (info_pane_ != nullptr) {
                    static_cast<void>(info_pane_->click(event.x, event.y));
                }
                break;  // the pane, not a row
            }
            if (const std::optional<Field> divider = divider_at(event.x, event.y)) {
                drag_ = Drag{.field = *divider,
                             .start_x = event.x,
                             .start_width = widths_.at(index(*divider))};
                redraw_();
                break;
            }
            if (const std::optional<Field> title = title_at(event.x, event.y)) {
                sort_by(*title);
                break;
            }
            if (event.y < rows_.y || event.y >= rows_.bottom()) {
                break;
            }
            const std::optional<std::size_t> row = row_at(event.x, event.y);
            const SelectMode mode = select_mode(event.modifiers, SelectMode::toggle);
            if (event.click_count == 2 && row && mode == SelectMode::replace) {
                folder_->set_cursor(row, mode);
                const fs::Entry* const entry = folder_->cursor_entry();
                if (entry != nullptr && !entry->is_directory() && open_) {
                    open_();  // a double click opens a file with its application
                } else {
                    enter();  // ... and a folder in the view
                }
                break;
            }
            if (row || mode == SelectMode::replace) {
                folder_->set_cursor(row, mode);  // a plain click on nothing clears
            }
            break;
        }
        case Kind::release:
            if ((drag_ || pane_drag_) && event.button == ui::PointerButton::left) {
                drag_.reset();
                pane_drag_.reset();
                update_hover();
                redraw_();
            }
            break;
    }
}

bool DetailsView::key(const ui::KeyEvent& event, Clock::time_point /*now*/) {
    if (!folder_) {
        return false;
    }
    Folder& folder = *folder_;
    const auto visible = static_cast<std::ptrdiff_t>(rows_.height / kRowHeight);
    const std::ptrdiff_t page = std::max<std::ptrdiff_t>(1, visible > 1 ? visible - 1 : visible);
    const std::optional<std::size_t> cursor = folder.cursor();
    const SelectMode mode = select_mode(event.modifiers, SelectMode::keep);
    const bool ctrl = event.modifiers.ctrl;
    switch (event.keysym) {
        case XKB_KEY_Up:
            if (event.modifiers.alt) {
                go_up();
            } else {
                folder.move_cursor(-1, mode);
            }
            break;
        case XKB_KEY_Down:
            if (event.modifiers.alt) {
                enter();
            } else {
                folder.move_cursor(1, mode);
            }
            break;
        case XKB_KEY_Left:
            go_up();
            break;
        case XKB_KEY_Right:
            enter();
            break;
        case XKB_KEY_Page_Down:
            folder.move_cursor(page, mode);
            break;
        case XKB_KEY_Page_Up:
            folder.move_cursor(-page, mode);
            break;
        case XKB_KEY_Home:
            folder.set_cursor(0, mode);
            break;
        case XKB_KEY_End:
            folder.set_cursor(std::numeric_limits<std::size_t>::max(), mode);
            break;
        case XKB_KEY_space:
            if (!ctrl || !cursor) {
                return false;  // plain Space is for Quick Look
            }
            folder.set_cursor(cursor, SelectMode::toggle);
            break;
        case XKB_KEY_a:
        case XKB_KEY_A:
            if (!ctrl) {
                return false;
            }
            folder.select_all();
            break;
        case XKB_KEY_Escape:
            folder.collapse_selection();
            break;
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            enter();
            break;
        case XKB_KEY_BackSpace:
            go_up();
            break;
        default:
            return false;
    }
    redraw_();
    return true;
}

void DetailsView::right_click(float x, float y) {
    if (!folder_) {
        return;
    }
    if (x < header_.right()) {  // the pane keeps the selection it shows
        if (y < rows_.y || y >= rows_.bottom()) {
            return;  // the titles
        }
        const std::optional<std::size_t> row = row_at(x, y);
        if (!row) {
            folder_->set_cursor(std::nullopt);  // empty space: the folder's own menu
        } else {
            folder_->set_cursor(row, folder_->is_selected(folder_->entries()[*row].name)
                                         ? SelectMode::keep
                                         : SelectMode::replace);
        }
    }
    if (menu_) {
        menu_(x, y);
    }
}

std::optional<std::pair<float, float>> DetailsView::cursor_anchor() const {
    const std::optional<std::size_t> cursor = folder_ ? folder_->cursor() : std::nullopt;
    if (!cursor) {
        return std::nullopt;
    }
    const float bottom =
        rows_.y + (static_cast<float>(*cursor + 1) * kRowHeight) - scroller_.offset();
    if (bottom - kRowHeight < rows_.y || bottom > rows_.bottom()) {
        return std::nullopt;
    }
    return std::pair{rows_.x + 48.0F, bottom};
}

}  // namespace nordend::views
