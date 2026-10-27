#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/sort.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/views/folder.hpp"

namespace nordend::views {

// One folder as a table: a row per entry with its name, size, date and type, under a header.
// Only the rows inside the viewport are drawn. A click on a column's title sorts by it, a second
// click reverses the order; dragging the line on a column's left edge makes it wider or narrower
// (the name takes what is left). The order is the view's: every folder it shows is sorted that
// way, and keeps it in the other views.
//
// Keys: Up/Down move the cursor, Page Up/Down by a screenful, Home/End to the ends; Shift, Ctrl,
// Ctrl+Space, Ctrl+A and Escape select as in the other views. Right, Enter, Alt+Down or a double
// click open the folder under the cursor; Left, Backspace, Alt+Up and the mouse back button open
// the parent, with the cursor on where we were.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class DetailsView {
public:
    using Clock = ui::Scroller::Clock;

    enum class Field : std::uint8_t { name, size, modified, type };

    static constexpr float kRowHeight = 28.0F;
    static constexpr float kHeaderHeight = 30.0F;

    DetailsView(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                std::function<void(ui::CursorShape)> set_cursor);

    // Shows folder as it is (cursor and selection), sorted as this view sorts, the cursor in
    // sight.
    void show(std::shared_ptr<Folder> folder);

    // Shows directory, no cursor.
    void open(const std::filesystem::path& directory);

    // The folder shown; nullptr before show() or open().
    [[nodiscard]] const std::shared_ptr<Folder>& folder() const noexcept { return folder_; }

    // Sorts by field: ascending, or reversed when it is the field already sorted by.
    void sort_by(Field field);

    [[nodiscard]] const fs::SortOrder& order() const noexcept { return order_; }

    // Lays out and draws into bounds at time now (advancing the scroll animation).
    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    // A scroll animation is running: the window should keep drawing frames.
    [[nodiscard]] bool animating() const noexcept { return scroller_.animating(); }

    void pointer(const ui::PointerEvent& event, Clock::time_point now);
    void key(const ui::KeyEvent& event, Clock::time_point now);

    // The folder has loaded and its cursor landed (snapshots, tests).
    [[nodiscard]] bool loading() const;

    [[nodiscard]] ui::Scroller& scroller() noexcept { return scroller_; }

    // A column's width in logical pixels (the name's as last laid out).
    [[nodiscard]] float width(Field field) const;

    // Shows or hides the info pane (Ctrl+I in the browser).
    void set_info_visible(bool visible);

    [[nodiscard]] bool info_visible() const noexcept { return info_visible_; }

private:
    // Where each column starts, from the last layout.
    struct Layout {
        std::array<float, 4> left{};
        std::array<float, 4> width{};
    };

    void enter();
    void go_up();
    void apply_order();
    void lay_out();
    void reveal_cursor(Folder::Reveal reveal, Clock::time_point now);
    [[nodiscard]] std::optional<std::size_t> row_at(float x, float y) const;
    // The column whose left edge is under x in the header (not the name: it has none).
    [[nodiscard]] std::optional<Field> divider_at(float x, float y) const;
    [[nodiscard]] std::optional<Field> title_at(float x, float y) const;
    void update_hover();

    core::EventLoop& loop_;
    core::WorkerPool& pool_;
    fs::DirectoryWatcher& watcher_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    std::function<void(ui::CursorShape)> set_cursor_;
    bool info_visible_{true};

    std::shared_ptr<Folder> folder_;
    fs::SortOrder order_;
    ui::Scroller scroller_;
    // Widths of size, modified and type; the name takes the rest.
    std::array<float, 4> widths_{0.0F, 96.0F, 150.0F, 220.0F};
    Layout layout_;

    struct Drag {
        Field field;
        float start_x;
        float start_width;
    };

    std::optional<Drag> drag_;
    bool over_divider_{false};

    ui::Rect header_{};  // where the header and the rows were last drawn
    ui::Rect rows_{};
    float pointer_x_{0.0F};
    float pointer_y_{0.0F};
    bool pointer_inside_{false};
    std::optional<std::size_t> hovered_;
};

}  // namespace nordend::views
