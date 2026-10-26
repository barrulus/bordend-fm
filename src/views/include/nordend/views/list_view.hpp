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
#include "nordend/ui/seat.hpp"

namespace nordend::views {

// A directory as a scrolling list: icon, name, size, date. Virtualized: each frame draws only
// the rows inside the viewport, so a 10 000-entry folder costs what a 30-entry one does.
// Entries whose type the name does not decide are sniffed on a worker when they become visible.
// Scrolling: wheel glide, touchpad with inertia (ui::Scroller); Page Up/Down, Home/End, arrows.
// A double click on a folder opens it, Backspace or Alt+Up goes to the parent. Selection comes
// with ROADMAP 4.3.
//
// Loop thread only. Must not outlive the loop, pool or watcher.
class ListView {
public:
    using Clock = ui::Scroller::Clock;

    static constexpr float kRowHeight = 28.0F;

    ListView(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
             std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw);

    ~ListView();

    ListView(const ListView&) = delete;
    ListView& operator=(const ListView&) = delete;
    ListView(ListView&&) = delete;
    ListView& operator=(ListView&&) = delete;

    void open(const std::filesystem::path& directory);

    // Jumps to a scroll position (benchmarks, tests); clamped at the next draw.
    void scroll_to(float offset) { scroller_.jump_to(offset); }

    // Lays out and draws into bounds at time now (advancing the scroll animation).
    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons, Clock::time_point now);

    // A scroll animation is running: the window should keep drawing frames.
    [[nodiscard]] bool animating() const noexcept { return scroller_.animating(); }

    void pointer(const ui::PointerEvent& event, Clock::time_point now);
    void key(const ui::KeyEvent& event, Clock::time_point now);

    [[nodiscard]] const fs::DirectoryModel& model() const noexcept { return model_; }

    [[nodiscard]] float scroll_offset() const noexcept { return scroller_.offset(); }

private:
    void on_change(const fs::ModelChange& change);
    void sniff(std::size_t first, std::size_t last);
    [[nodiscard]] std::optional<std::size_t> row_at(float x, float y) const;
    [[nodiscard]] fs::MimeId mime_of(const fs::Entry& entry) const;
    [[nodiscard]] float content_height() const;

    core::WorkerPool& pool_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    fs::DirectoryModel model_;
    ui::Scroller scroller_;

    ui::Rect rows_area_{};  // where the rows were last drawn, for hit testing
    std::optional<std::size_t> hovered_;
    float pointer_x_{0.0F};
    float pointer_y_{0.0F};
    bool pointer_inside_{false};

    // Types sniffed from content, by name, for the open directory.
    std::unordered_map<std::string, fs::MimeId> sniffed_;
    std::unordered_set<std::string> sniffing_;
    std::uint64_t generation_{0};  // bumped on open: late sniff results are dropped
    // False once destroyed: sniff results still on their way find out here.
    std::shared_ptr<bool> alive_{std::make_shared<bool>(true)};
};

}  // namespace nordend::views
