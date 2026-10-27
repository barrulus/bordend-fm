#include "nordend/views/browser.hpp"

#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace nordend::views {

Browser::Browser(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                 std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                 std::function<void(ui::CursorShape)> set_cursor)
    : redraw_{redraw},
      columns_{loop, pool, watcher, mime, redraw, std::move(set_cursor)},
      grid_{loop, pool, watcher, std::move(mime), std::move(redraw)} {}

void Browser::open(const std::filesystem::path& directory) {
    columns_.open(directory);
    if (mode_ == Mode::grid) {
        columns_.suspend();
        grid_.show(columns_.focused_folder());
    }
}

void Browser::reveal(const std::filesystem::path& root, const std::filesystem::path& target) {
    set_mode(Mode::columns);
    columns_.reveal(root, target);
}

void Browser::set_mode(Mode mode) {
    if (mode == mode_) {
        return;
    }
    // The view that leaves forgets the pointer; the one that comes finds it where it is.
    const auto now = Clock::now();
    const ui::PointerEvent leave{.kind = ui::PointerEvent::Kind::leave};
    const ui::PointerEvent motion{
        .kind = ui::PointerEvent::Kind::motion, .x = pointer_x_, .y = pointer_y_};
    if (mode == Mode::grid) {
        std::shared_ptr<Folder> folder = columns_.focused_folder();
        if (!folder) {
            return;
        }
        columns_.pointer(leave, now);
        columns_.suspend();
        grid_.show(std::move(folder));
        if (pointer_inside_) {
            grid_.pointer(motion, now);
        }
    } else {
        grid_.pointer(leave, now);
        if (grid_.folder()) {
            columns_.show(grid_.folder());
        }
        if (pointer_inside_) {
            columns_.pointer(motion, now);
        }
    }
    mode_ = mode;
    redraw_();
}

void Browser::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                   ui::IconSet& icons, Clock::time_point now) {
    if (mode_ == Mode::grid) {
        grid_.draw(renderer, bounds, palette, icons, now);
    } else {
        columns_.draw(renderer, bounds, palette, icons, now);
    }
}

bool Browser::animating() const {
    return mode_ == Mode::grid ? grid_.animating() : columns_.animating();
}

bool Browser::loading() const { return mode_ == Mode::grid ? grid_.loading() : columns_.loading(); }

void Browser::pointer(const ui::PointerEvent& event, Clock::time_point now) {
    if (event.kind == ui::PointerEvent::Kind::leave) {
        pointer_inside_ = false;
    } else {
        pointer_inside_ = true;
        pointer_x_ = event.x;
        pointer_y_ = event.y;
    }
    if (mode_ == Mode::grid) {
        grid_.pointer(event, now);
    } else {
        columns_.pointer(event, now);
    }
}

void Browser::key(const ui::KeyEvent& event, Clock::time_point now) {
    if (event.modifiers.ctrl && !event.modifiers.shift && !event.modifiers.alt) {
        if (event.keysym == XKB_KEY_1 || event.keysym == XKB_KEY_KP_1) {
            set_mode(Mode::columns);
            return;
        }
        if (event.keysym == XKB_KEY_2 || event.keysym == XKB_KEY_KP_2) {
            set_mode(Mode::grid);
            return;
        }
    }
    if (mode_ == Mode::grid) {
        grid_.key(event, now);
    } else {
        columns_.key(event, now);
    }
}

}  // namespace nordend::views
