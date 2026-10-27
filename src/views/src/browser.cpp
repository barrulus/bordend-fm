#include "nordend/views/browser.hpp"

#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace nordend::views {

Browser::Browser(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                 std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                 std::function<void(ui::CursorShape)> set_cursor,
                 std::shared_ptr<const thumbs::Cache> thumbnails)
    : redraw_{redraw},
      thumbnails_{thumbnails ? std::make_unique<Thumbnails>(pool, thumbnails, redraw) : nullptr},
      columns_{loop, pool, watcher, mime, redraw, std::move(set_cursor)},
      grid_{loop, pool, watcher, mime, redraw, thumbnails_.get()},
      quick_look_{pool, std::move(mime), std::move(thumbnails), std::move(redraw)} {}

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
    if (quick_look_.is_open()) {
        quick_look_.follow(current_folder());  // entries may have moved or gone meanwhile
    }
    quick_look_.draw(renderer, bounds, palette, icons, now);
    if (thumbnails_) {
        thumbnails_->end_frame();
    }
}

bool Browser::animating() const {
    return (mode_ == Mode::grid ? grid_.animating() : columns_.animating()) ||
           quick_look_.animating(Clock::now());
}

bool Browser::loading() const {
    if (quick_look_.loading()) {
        return true;
    }
    if (mode_ == Mode::grid) {
        return grid_.loading() || (thumbnails_ && thumbnails_->loading());
    }
    return columns_.loading();
}

void Browser::pointer(const ui::PointerEvent& event, Clock::time_point now) {
    if (quick_look_.is_open()) {
        // Over the overlay: a click closes it, the rest waits until it is gone.
        if (event.kind == ui::PointerEvent::Kind::press) {
            quick_look_.close();
        }
        if (event.kind != ui::PointerEvent::Kind::leave) {
            pointer_x_ = event.x;
            pointer_y_ = event.y;
        }
        return;
    }
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

std::shared_ptr<Folder> Browser::current_folder() const {
    return mode_ == Mode::grid ? grid_.folder() : columns_.focused_folder();
}

void Browser::key(const ui::KeyEvent& event, Clock::time_point now) {
    const bool plain = event.modifiers == ui::Modifiers{};
    if (quick_look_.is_open()) {
        if ((plain && event.keysym == XKB_KEY_space) || event.keysym == XKB_KEY_Escape) {
            quick_look_.close();
            return;
        }
        // Everything else moves the view underneath; Quick Look shows where it lands.
        view_key(event, now);
        quick_look_.follow(current_folder());
        return;
    }
    if (plain && event.keysym == XKB_KEY_space) {
        quick_look_.open(current_folder(), now);
        return;
    }
    view_key(event, now);
}

void Browser::view_key(const ui::KeyEvent& event, Clock::time_point now) {
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
