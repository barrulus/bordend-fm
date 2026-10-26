#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_model.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/scroller.hpp"

namespace nordend::views {

// One directory in the column view: a virtualized, scrolling list of icon and name, with a
// cursor (the entry the next column shows). Each frame draws only the rows inside the viewport.
// Entries whose type the name does not decide are sniffed on a worker when they become visible.
//
// The cursor is kept by name, so it follows its entry when rows move, and it can name an entry
// that has not been loaded yet (pending): it lands when the entry arrives, or is dropped when the
// load ends without it.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class Column {
public:
    using Clock = ui::Scroller::Clock;

    static constexpr float kRowHeight = 28.0F;

    // cursor_changed runs when the cursor moves, lands, is dropped, or its entry changes.
    Column(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
           std::shared_ptr<const fs::MimeDatabase> mime, std::filesystem::path directory,
           std::function<void()> redraw, std::function<void()> cursor_changed);

    ~Column();

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;
    Column(Column&&) = delete;
    Column& operator=(Column&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return model_.path(); }

    [[nodiscard]] const fs::DirectoryModel& model() const noexcept { return model_; }

    // The entry under the cursor; nullptr when there is none or it is still pending.
    [[nodiscard]] const fs::Entry* cursor_entry() const;

    [[nodiscard]] std::optional<std::size_t> cursor() const noexcept { return cursor_index_; }

    // A cursor was asked for by name and its entry has not arrived yet.
    [[nodiscard]] bool cursor_pending() const noexcept {
        return !cursor_name_.empty() && !cursor_index_;
    }

    // Puts the cursor on a row (clamped), or removes it; glides to keep it in view.
    void set_cursor(std::optional<std::size_t> row, Clock::time_point now);

    // Moves the cursor by rows (from the first row when there is none).
    void move_cursor(std::ptrdiff_t rows, Clock::time_point now);

    // Puts the cursor on the entry called name, now or when it arrives; jumps to show it.
    void select_name(std::string name);

    // Rows a page key moves: what fits in the last drawn viewport, less one.
    [[nodiscard]] std::size_t page_rows() const;

    // Lays out and draws into bounds at time now (advancing the scroll animation). The cursor is
    // strong in the focused column, quiet in the others (the path that leads to it).
    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, bool focused, Clock::time_point now);

    // A scroll animation is running: the window should keep drawing frames.
    [[nodiscard]] bool animating() const noexcept { return scroller_.animating(); }

    [[nodiscard]] ui::Scroller& scroller() noexcept { return scroller_; }

    // Row at a window point, from the last drawn bounds; nullopt outside the rows.
    [[nodiscard]] std::optional<std::size_t> row_at(float x, float y) const;

    // Hover highlight; returns true if it changed.
    bool set_hovered(std::optional<std::size_t> row);

    // The entry's type: from the name, else sniffed from the content, else unknown (yet).
    [[nodiscard]] fs::MimeId mime_of(const fs::Entry& entry) const;

private:
    void on_change(const fs::ModelChange& change);
    void resolve_cursor();
    void sniff(std::size_t first, std::size_t last);
    void reveal_cursor(Clock::time_point now, bool animate);

    core::WorkerPool& pool_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    std::function<void()> cursor_changed_;
    fs::DirectoryModel model_;
    ui::Scroller scroller_;

    ui::Rect rows_area_{};  // where the rows were last drawn, for hit testing
    std::optional<std::size_t> hovered_;

    std::string cursor_name_;                  // empty: no cursor
    std::optional<std::size_t> cursor_index_;  // nullopt while pending
    bool jump_to_cursor_{false};               // at the next draw (the viewport is known there)

    // Types sniffed from content, by name.
    std::unordered_map<std::string, fs::MimeId> sniffed_;
    std::unordered_set<std::string> sniffing_;
    // False once destroyed: sniff results still on their way find out here.
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

}  // namespace nordend::views
