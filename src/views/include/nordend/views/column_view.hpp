#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/views/column.hpp"
#include "nordend/views/info_pane.hpp"

namespace nordend::views {

// Miller columns, as in Finder: each column is a folder, and the entry under its cursor is shown
// in the column to its right (a folder) or in an info pane (a file). One column has the keyboard
// focus.
//
// Keys: Up/Down, Page Up/Down, Home/End move the cursor in the focused column; Right or Enter
// enter the folder under the cursor; Left goes back one column, and from the first column opens
// the parent folder as a new first column (Backspace, Alt+Up and the mouse back button do the
// same). A click puts the cursor on a row. Columns are resized by dragging their right edge.
//
// Selection (in the focused column): a click or an arrow selects one entry; Shift+click and
// Shift+arrows/Page/Home/End select a range from the anchor; Ctrl+click adds or removes one
// entry; Ctrl+arrows move the cursor alone and Ctrl+Space adds or removes the entry under it;
// Ctrl+A selects everything, Escape goes back to the cursor's entry. With more than one entry
// selected no column opens after it: the info pane sums the selection up.
//
// Scrolling: each column scrolls vertically (wheel glide, touchpad with inertia); the row of
// columns scrolls horizontally (touchpad, tilt wheel, Shift+wheel) and glides by itself to keep
// the focused column and the one it opens in view. A touchpad gesture is locked to the axis it
// starts on.
//
// The grid can take over the focused column's folder (focused_folder(), then suspend()) and hand
// a folder back (show()), so the cursor and the selection survive the switch.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class ColumnView {
public:
    using Clock = ui::Scroller::Clock;

    static constexpr float kDefaultColumnWidth = 260.0F;
    static constexpr float kMinColumnWidth = 140.0F;
    static constexpr float kMaxColumnWidth = 800.0F;
    static constexpr float kInfoWidth = InfoPane::kWidth;

    ColumnView(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
               std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
               std::function<void(ui::CursorShape)> set_cursor, InfoPane* info_pane = nullptr);

    ~ColumnView();

    ColumnView(const ColumnView&) = delete;
    ColumnView& operator=(const ColumnView&) = delete;
    ColumnView(ColumnView&&) = delete;
    ColumnView& operator=(ColumnView&&) = delete;

    // One column showing directory, focused, no cursor.
    void open(const std::filesystem::path& directory);

    // Columns from root down to target, with the cursor on each step of the way (they unfold
    // as each folder loads). target must be inside root.
    void reveal(const std::filesystem::path& root, const std::filesystem::path& target);

    // Focuses the column showing directory, if one does (true then), and goes back there: the
    // folder under its cursor shows again without a cursor, the columns after it close.
    bool focus_folder(const std::filesystem::path& directory);

    // The folder of the focused column; nullptr before open().
    [[nodiscard]] std::shared_ptr<Folder> focused_folder() const;

    // Another view shows the folders for now: changes to their selections wait for show().
    void suspend() noexcept { suspended_ = true; }

    // Back on screen, with folder in the focused column: the column that holds it, or columns
    // from the deepest one that leads to it (its parent's, when none does), down to folder
    // itself, with its cursor and selection as they are.
    void show(std::shared_ptr<Folder> folder);

    // Lays out and draws into bounds at time now (advancing every animation).
    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    // An animation is running: the window should keep drawing frames.
    [[nodiscard]] bool animating() const noexcept;

    void pointer(const ui::PointerEvent& event, Clock::time_point now);
    // false when event is not one of its keys.
    bool key(const ui::KeyEvent& event, Clock::time_point now);

    // Called on a double click on a file (not a folder): the browser opens the selection.
    void set_open_handler(std::function<void()> open) { open_ = std::move(open); }

    [[nodiscard]] std::size_t column_count() const noexcept { return columns_.size(); }

    [[nodiscard]] Column& column(std::size_t index) { return *columns_.at(index).column; }

    [[nodiscard]] std::size_t focus() const noexcept { return focus_; }

    // Every column has loaded (snapshots, tests).
    [[nodiscard]] bool loading() const;

    [[nodiscard]] float scroll_offset() const noexcept { return scroller_.offset(); }

private:
    struct Slot {
        std::unique_ptr<Column> column;
        float width{kDefaultColumnWidth};
        float x{0.0F};  // left edge in content coordinates, set by layout()
    };

    enum class Axis : std::uint8_t { none, vertical, horizontal };

    std::unique_ptr<Column> make_column(const std::filesystem::path& directory);
    std::unique_ptr<Column> make_column(std::shared_ptr<Folder> folder);
    void on_cursor_changed(const Folder* folder);
    void sync_after(std::size_t index);
    void truncate(std::size_t count);
    void go_left();
    void go_right();
    void focus_on(std::size_t index);
    void layout();
    [[nodiscard]] bool shows_info() const;
    [[nodiscard]] float content_width() const;
    [[nodiscard]] std::optional<std::size_t> column_at(float x) const;
    [[nodiscard]] std::optional<std::size_t> divider_at(float x, float y) const;
    void scroll(const ui::PointerEvent& event, Clock::time_point now);
    void update_hover();

    core::EventLoop& loop_;
    core::WorkerPool& pool_;
    fs::DirectoryWatcher& watcher_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    std::function<void()> open_;
    std::function<void(ui::CursorShape)> set_cursor_;
    InfoPane* info_pane_;
    ui::Rect info_area_{};  // where the info pane was last drawn; empty when not shown

    std::vector<Slot> columns_;
    std::size_t focus_{0};
    std::filesystem::path reveal_;   // where reveal() is heading; empty when done
    std::shared_ptr<Folder> adopt_;  // the folder show() unfolds to, used as its column
    bool suspended_{false};
    ui::Scroller scroller_;     // horizontal
    bool reveal_focus_{false};  // glide to show the focused column at the next draw
    float shift_{0.0F};         // added to the offset at the next draw (a column prepended)

    ui::Rect area_{};  // where the columns were last drawn
    float pointer_x_{0.0F};
    float pointer_y_{0.0F};
    bool pointer_inside_{false};
    std::optional<std::size_t> hovered_column_;
    std::optional<std::size_t> resizing_;  // column whose right edge is being dragged
    float resize_grab_{0.0F};              // pointer x minus the edge when the drag began
    bool over_divider_{false};
    Axis gesture_axis_{Axis::none};  // touchpad gesture lock
    std::optional<std::size_t> gesture_column_;
};

}  // namespace nordend::views
