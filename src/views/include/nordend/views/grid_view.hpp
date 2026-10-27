#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

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
#include "nordend/views/folder.hpp"
#include "nordend/views/info_pane.hpp"
#include "nordend/views/thumbnails.hpp"

namespace nordend::views {

// One folder as a grid of cells (a large icon, the name under it), filled row by row and scrolled
// vertically. Only the rows inside the viewport are drawn. The cells stretch to fill the width;
// their size follows the icon size, which Ctrl+plus/minus (or Ctrl+wheel) change and Ctrl+0
// resets. Images and raw files show their thumbnail in the icon's place once it is ready.
//
// Keys: arrows move the cursor (Up/Down by a row), Page Up/Down by a screenful, Home/End to the
// ends; Shift, Ctrl, Ctrl+Space, Ctrl+A and Escape select as in the column view. Enter (or
// Alt+Down, or a double click) opens the folder under the cursor; Backspace, Alt+Up and the mouse
// back button open the parent, with the cursor on where we were.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class GridView {
public:
    using Clock = ui::Scroller::Clock;

    static constexpr std::array<float, 6> kIconSizes{48.0F, 64.0F, 96.0F, 128.0F, 192.0F, 256.0F};
    static constexpr std::size_t kDefaultSize = 2;  // 96 px

    // thumbnails may be nullptr: icons only.
    GridView(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
             std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
             std::function<void(ui::CursorShape)> set_cursor, Thumbnails* thumbnails = nullptr,
             InfoPane* info_pane = nullptr);

    // Shows folder as it is (cursor and selection), the cursor in sight.
    void show(std::shared_ptr<Folder> folder);

    // Shows directory, no cursor.
    void open(const std::filesystem::path& directory);

    // The folder shown; nullptr before show() or open().
    [[nodiscard]] const std::shared_ptr<Folder>& folder() const noexcept { return folder_; }

    // Lays out and draws into bounds at time now (advancing the scroll animation).
    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    // A scroll animation is running: the window should keep drawing frames.
    [[nodiscard]] bool animating() const noexcept { return scroller_.animating(); }

    void pointer(const ui::PointerEvent& event, Clock::time_point now);
    void key(const ui::KeyEvent& event, Clock::time_point now);

    // The folder has loaded and its cursor landed (snapshots, tests).
    [[nodiscard]] bool loading() const;

    [[nodiscard]] float icon_size() const { return kIconSizes.at(size_); }

    // Cells per row in the last layout (1 before the first draw).
    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }

    [[nodiscard]] ui::Scroller& scroller() noexcept { return scroller_; }

    // Height of a cell at the current icon size.
    [[nodiscard]] float cell_height() const;

    // Shows or hides the info pane (Ctrl+I in the browser).
    void set_info_visible(bool visible);

    [[nodiscard]] bool info_visible() const noexcept { return info_visible_; }

private:
    void enter();
    void go_up();
    void zoom(std::size_t size);
    void keep_place();
    void layout(float width);
    void reveal_cursor(Folder::Reveal reveal, Clock::time_point now);
    [[nodiscard]] std::size_t rows() const;
    [[nodiscard]] float row_top(std::size_t row) const;  // content coordinates
    [[nodiscard]] std::optional<std::size_t> cell_at(float x, float y) const;
    void update_hover();
    [[nodiscard]] bool on_pane_edge(float x, float y) const;

    core::EventLoop& loop_;
    core::WorkerPool& pool_;
    fs::DirectoryWatcher& watcher_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    Thumbnails* thumbnails_;
    bool info_visible_{true};
    InfoPane* info_pane_;
    std::function<void(ui::CursorShape)> set_cursor_;

    // Dragging the info pane's edge: where the pointer and the width started.
    struct PaneDrag {
        float start_x;
        float start_width;
    };

    std::optional<PaneDrag> pane_drag_;
    bool over_pane_edge_{false};
    float pane_edge_{-1.0F};  // x of the info pane's edge as last drawn; < 0 without a pane

    std::shared_ptr<Folder> folder_;
    ui::Scroller scroller_;
    std::size_t size_{kDefaultSize};
    std::size_t columns_{1};
    float cell_width_{0.0F};

    // A zoom keeps this entry where it was on screen (its row's top, viewport coordinates).
    struct Anchor {
        std::size_t index;
        float screen_y;
    };

    std::optional<Anchor> anchor_;
    float zoom_wheel_{0.0F};  // Ctrl+wheel notches not yet turned into a step

    ui::Rect area_{};  // where the cells were last drawn
    float pointer_x_{0.0F};
    float pointer_y_{0.0F};
    bool pointer_inside_{false};
    std::optional<std::size_t> hovered_;
};

}  // namespace nordend::views
