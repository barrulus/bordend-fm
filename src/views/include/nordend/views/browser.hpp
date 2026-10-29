#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/views/breadcrumb.hpp"
#include "nordend/views/column_view.hpp"
#include "nordend/views/details_view.hpp"
#include "nordend/views/grid_view.hpp"
#include "nordend/views/info_pane.hpp"
#include "nordend/views/quick_look.hpp"
#include "nordend/views/shortcuts.hpp"
#include "nordend/views/shortcuts_overlay.hpp"
#include "nordend/views/sidebar.hpp"
#include "nordend/views/status_bar.hpp"
#include "nordend/views/thumbnails.hpp"

namespace nordend::views {

// The window's content: the column view, the grid or the details table, one at a time, and Quick
// Look over them (Space opens it on the entry under the cursor; Space, Escape or a click close it;
// the other keys move the view underneath). Ctrl+1 shows the columns, Ctrl+2 the grid, Ctrl+3
// the details. The info pane shows what is selected in all three: after the last column, at the
// right of the grid and the details, where Ctrl+I shows or hides it. The grid and the details show
// the folder of the focused column, and the columns come back with that folder focused: the same
// folder object passes between them, so the cursor, the selection and the types sniffed so far
// stay. Above the view, the breadcrumb shows the folder's path, every step a way up. The sidebar at
// the left lists the places (set_places); Ctrl+B shows or hides it, F6 moves the keyboard into it
// and back, and a place chosen there opens in the view shown. Enter or a double click on a file
// hands the selected files to the open handler (their default applications); on a folder, the
// view goes into it.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class Browser {
public:
    using Clock = ui::Scroller::Clock;

    enum class Mode : std::uint8_t { columns, grid, details };

    // thumbnails: the cache the grid takes its thumbnails from; nullptr shows icons only.
    Browser(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
            std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
            std::function<void(ui::CursorShape)> set_cursor,
            std::shared_ptr<const thumbs::Cache> thumbnails = nullptr);

    // directory, no cursor, in the current mode.
    void open(const std::filesystem::path& directory);

    // folder in the current mode with the cursor on child (a name; empty: none), as when going
    // up: the columns focus the column showing folder when there is one.
    void go_to(const std::filesystem::path& folder, const std::string& child);

    // The columns from root down to target (see ColumnView::reveal); switches to the columns.
    void reveal(const std::filesystem::path& root, const std::filesystem::path& target);

    void set_mode(Mode mode);

    [[nodiscard]] Mode mode() const noexcept { return mode_; }

    // The info pane in the grid and the details, one setting for both (shown at start); the
    // columns always show it.
    void set_info_visible(bool visible);

    [[nodiscard]] bool info_visible() const noexcept { return info_visible_; }

    // The sidebar (shown at start; left out when the window is too narrow for it).
    void set_sidebar_visible(bool visible);

    [[nodiscard]] bool sidebar_visible() const noexcept { return sidebar_visible_; }

    // What the sidebar lists.
    void set_places(std::vector<fs::Place> places);

    // Opens files (directory's entries, never folders, types filled in where known). Without
    // one, Enter on a file does nothing.
    using OpenHandler =
        std::function<void(const std::filesystem::path& directory, std::vector<fs::Entry> files)>;

    void set_open_handler(OpenHandler open) { open_ = std::move(open); }

    // A line in the status bar for a few seconds (what opening a file did).
    void show_message(std::string text, bool error) {
        status_bar_.show_message(std::move(text), error);
    }

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    [[nodiscard]] bool animating() const;

    void pointer(const ui::PointerEvent& event, Clock::time_point now);
    // false when event did nothing: no shortcut (shortcuts()) where the keyboard is.
    bool key(const ui::KeyEvent& event, Clock::time_point now);

    // The view shown is still loading, or its thumbnails are being made (snapshots, tests).
    [[nodiscard]] bool loading() const;

    [[nodiscard]] ColumnView& columns() noexcept { return columns_; }

    [[nodiscard]] GridView& grid() noexcept { return grid_; }

    [[nodiscard]] DetailsView& details() noexcept { return details_; }

    [[nodiscard]] QuickLook& quick_look() noexcept { return quick_look_; }

    [[nodiscard]] InfoPane& info_pane() noexcept { return info_pane_; }

    [[nodiscard]] Sidebar& sidebar() noexcept { return sidebar_; }

    [[nodiscard]] Breadcrumb& breadcrumb() noexcept { return breadcrumb_; }

    [[nodiscard]] StatusBar& status_bar() noexcept { return status_bar_; }

    [[nodiscard]] ShortcutsOverlay& shortcuts_overlay() noexcept { return shortcuts_; }

private:
    // The folder the keyboard is in: the focused column's, or the grid's.
    [[nodiscard]] std::shared_ptr<Folder> current_folder() const;
    void run(Command command, Clock::time_point now);
    // Opens the selected files when the cursor is on a file (only the cursor's when it is not
    // selected); false when the cursor is on a folder or nowhere.
    bool open_selection();
    bool view_key(const ui::KeyEvent& event, Clock::time_point now);
    void dispatch_pointer(const ui::PointerEvent& event, Clock::time_point now);
    void view_pointer(const ui::PointerEvent& event, Clock::time_point now);

    // Who gets the pointer: the view, the sidebar or the breadcrumb above the view.
    enum class Target : std::uint8_t { view, sidebar, breadcrumb, status_bar };

    void send_pointer(Target target, const ui::PointerEvent& event, Clock::time_point now);
    void draw_view(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                   ui::IconSet& icons, Clock::time_point now);

    std::function<void()> redraw_;
    OpenHandler open_;
    std::unique_ptr<Thumbnails> thumbnails_;  // before the views that draw them
    InfoPane info_pane_;                      // the same: every view draws it
    ColumnView columns_;                      // before grid_: grid folders may report to it
    GridView grid_;
    DetailsView details_;
    QuickLook quick_look_;
    Sidebar sidebar_;
    Breadcrumb breadcrumb_;
    StatusBar status_bar_;
    ShortcutsOverlay shortcuts_;
    Mode mode_{Mode::columns};
    bool info_visible_{true};
    bool sidebar_visible_{true};
    bool sidebar_drawn_{false};  // in the last draw: the window had room for it
    Target pointer_target_{Target::view};
    bool button_held_{false};  // since a press: the pointer stays with its target

    // The last pointer position, handed to the view that comes on screen.
    float pointer_x_{0.0F};
    float pointer_y_{0.0F};
    bool pointer_inside_{false};
};

}  // namespace nordend::views
