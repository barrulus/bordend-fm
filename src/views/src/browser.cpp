#include "nordend/views/browser.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "draw_helpers.hpp"

namespace nordend::views {
namespace {

std::filesystem::path home_directory() {
    const char* const home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe): at start
    return home != nullptr && *home == '/' ? std::filesystem::path{home} : std::filesystem::path{};
}

}  // namespace

Browser::Browser(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                 std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                 std::function<void(ui::CursorShape)> set_cursor,
                 std::shared_ptr<const thumbs::Cache> thumbnails)
    : redraw_{redraw},
      thumbnails_{thumbnails ? std::make_unique<Thumbnails>(pool, thumbnails, redraw) : nullptr},
      info_pane_{pool, thumbnails_.get(), redraw},
      columns_{loop, pool, watcher, mime, redraw, set_cursor, &info_pane_},
      grid_{loop, pool, watcher, mime, redraw, set_cursor, thumbnails_.get(), &info_pane_},
      details_{loop, pool, watcher, mime, redraw, set_cursor, &info_pane_},
      quick_look_{pool, std::move(mime), std::move(thumbnails), redraw},
      sidebar_{redraw, std::move(set_cursor),
               [this](const std::filesystem::path& place) { open(place); }},
      breadcrumb_{redraw,
                  [this](const std::filesystem::path& folder, const std::string& child) {
                      go_to(folder, child);
                  },
                  home_directory()},
      status_bar_{pool, redraw, [this](std::size_t view) { set_mode(static_cast<Mode>(view)); },
                  [this] { shortcuts_.open(); }},
      shortcuts_{std::move(redraw)} {}

void Browser::open(const std::filesystem::path& directory) {
    columns_.open(directory);
    if (mode_ == Mode::grid) {
        columns_.suspend();
        grid_.show(columns_.focused_folder());
    } else if (mode_ == Mode::details) {
        columns_.suspend();
        details_.show(columns_.focused_folder());
    }
}

void Browser::reveal(const std::filesystem::path& root, const std::filesystem::path& target) {
    set_mode(Mode::columns);
    columns_.reveal(root, target);
}

void Browser::set_info_visible(bool visible) {
    info_visible_ = visible;
    grid_.set_info_visible(visible);
    details_.set_info_visible(visible);
}

void Browser::set_sidebar_visible(bool visible) {
    if (visible == sidebar_visible_) {
        return;
    }
    sidebar_visible_ = visible;
    if (!visible) {
        sidebar_.set_focused(false);
        if (pointer_target_ == Target::sidebar) {
            send_pointer(Target::sidebar, {.kind = ui::PointerEvent::Kind::leave}, Clock::now());
            pointer_target_ = Target::view;
            button_held_ = false;
        }
    }
    redraw_();
}

void Browser::set_places(std::vector<fs::Place> places) { sidebar_.set_places(std::move(places)); }

void Browser::set_mode(Mode mode) {
    if (mode == mode_) {
        return;
    }
    std::shared_ptr<Folder> folder = current_folder();
    if (!folder) {
        return;
    }
    // The view that leaves forgets the pointer; the one that comes finds it where it is.
    const auto now = Clock::now();
    const ui::PointerEvent leave{.kind = ui::PointerEvent::Kind::leave};
    const ui::PointerEvent motion{
        .kind = ui::PointerEvent::Kind::motion, .x = pointer_x_, .y = pointer_y_};
    if (pointer_target_ == Target::view) {
        view_pointer(leave, now);
    }
    if (mode_ == Mode::columns) {
        columns_.suspend();
    }
    mode_ = mode;
    switch (mode_) {
        case Mode::columns:
            columns_.show(std::move(folder));
            break;
        case Mode::grid:
            grid_.show(std::move(folder));
            break;
        case Mode::details:
            details_.show(std::move(folder));
            break;
    }
    if (pointer_inside_) {
        dispatch_pointer(motion, now);
    }
    redraw_();
}

void Browser::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                   ui::IconSet& icons, Clock::time_point now) {
    const float sidebar_width = sidebar_visible_ ? sidebar_.width_in(bounds.width) : 0.0F;
    sidebar_drawn_ = sidebar_width > 0.0F;
    if (!sidebar_drawn_ && sidebar_.focused()) {
        sidebar_.set_focused(false);  // no room for it: the keyboard goes back to the view
    }
    const std::shared_ptr<Folder> folder = current_folder();
    if (sidebar_drawn_) {
        sidebar_.draw(renderer, {bounds.x, bounds.y, sidebar_width, bounds.height}, palette, icons,
                      folder ? folder->path() : std::filesystem::path{}, now);
    } else {
        sidebar_.not_drawn();
    }
    const ui::Rect view{bounds.x + sidebar_width, bounds.y, bounds.width - sidebar_width,
                        bounds.height};
    const float bar = std::min(StatusBar::kHeight, std::max(0.0F, view.height - kTitleHeight));
    if (folder) {
        breadcrumb_.draw(renderer, {view.x, view.y, view.width, kTitleHeight}, palette, icons,
                         *folder);
        status_bar_.draw(renderer, {view.x, view.bottom() - bar, view.width, bar}, palette, icons,
                         *folder, static_cast<std::size_t>(mode_));
    }
    draw_view(renderer,
              {view.x, view.y + kTitleHeight, view.width,
               std::max(0.0F, view.height - kTitleHeight - bar)},
              palette, icons, now);
    if (quick_look_.is_open()) {
        quick_look_.follow(current_folder());  // entries may have moved or gone meanwhile
    }
    quick_look_.draw(renderer, bounds, palette, icons, now);
    shortcuts_.draw(renderer, bounds, palette);
    if (thumbnails_) {
        thumbnails_->end_frame();
    }
}

void Browser::draw_view(ui::Renderer& renderer, const ui::Rect& bounds,
                        const theme::Palette& palette, ui::IconSet& icons, Clock::time_point now) {
    switch (mode_) {
        case Mode::columns:
            columns_.draw(renderer, bounds, palette, icons, now);
            break;
        case Mode::grid:
            grid_.draw(renderer, bounds, palette, icons, now);
            break;
        case Mode::details:
            details_.draw(renderer, bounds, palette, icons, now);
            break;
    }
}

bool Browser::animating() const {
    bool view = false;
    switch (mode_) {
        case Mode::columns:
            view = columns_.animating();
            break;
        case Mode::grid:
            view = grid_.animating();
            break;
        case Mode::details:
            view = details_.animating();
            break;
    }
    return view || info_pane_.animating() || sidebar_.animating() ||
           quick_look_.animating(Clock::now());
}

bool Browser::loading() const {
    if (quick_look_.loading() || info_pane_.loading() || status_bar_.loading()) {
        return true;
    }
    switch (mode_) {
        case Mode::grid:
            return grid_.loading() || (thumbnails_ && thumbnails_->loading());
        case Mode::details:
            return details_.loading();
        case Mode::columns:
            break;
    }
    return columns_.loading();
}

void Browser::pointer(const ui::PointerEvent& event, Clock::time_point now) {
    if (shortcuts_.is_open()) {
        // A click anywhere closes the shortcuts; nothing under them reacts meanwhile.
        if (event.kind == ui::PointerEvent::Kind::press) {
            shortcuts_.close();
        }
        if (event.kind != ui::PointerEvent::Kind::leave) {
            pointer_x_ = event.x;
            pointer_y_ = event.y;
        }
        return;
    }
    if (quick_look_.is_open()) {
        // Over the overlay: a click closes it, scrolling moves a text page, the rest waits
        // until it is gone.
        if (event.kind == ui::PointerEvent::Kind::press) {
            quick_look_.close();
        } else {
            quick_look_.pointer(event, now);
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
    dispatch_pointer(event, now);
}

void Browser::dispatch_pointer(const ui::PointerEvent& event, Clock::time_point now) {
    using Kind = ui::PointerEvent::Kind;
    if (event.kind == Kind::leave) {
        send_pointer(pointer_target_, event, now);
        pointer_target_ = Target::view;
        button_held_ = false;
        return;
    }
    // While a button is held (a drag) the pointer stays with whoever took the press.
    if (!button_held_) {
        Target target = Target::view;
        if (sidebar_drawn_ && sidebar_.claims(event.x, event.y)) {
            target = Target::sidebar;
        } else if (breadcrumb_.claims(event.x, event.y)) {
            target = Target::breadcrumb;
        } else if (status_bar_.claims(event.x, event.y)) {
            target = Target::status_bar;
        }
        if (target != pointer_target_) {
            send_pointer(pointer_target_, {.kind = Kind::leave}, now);
            pointer_target_ = target;
        }
    }
    if (event.kind == Kind::press) {
        button_held_ = true;
        if (pointer_target_ != Target::sidebar && sidebar_.focused()) {
            sidebar_.set_focused(false);  // a click elsewhere takes the keyboard back
        }
    } else if (event.kind == Kind::release) {
        button_held_ = false;
    }
    send_pointer(pointer_target_, event, now);
}

void Browser::send_pointer(Target target, const ui::PointerEvent& event, Clock::time_point now) {
    switch (target) {
        case Target::view:
            view_pointer(event, now);
            break;
        case Target::sidebar:
            sidebar_.pointer(event, now);
            break;
        case Target::breadcrumb:
            breadcrumb_.pointer(event);
            break;
        case Target::status_bar:
            status_bar_.pointer(event);
            break;
    }
}

void Browser::go_to(const std::filesystem::path& folder, const std::string& child) {
    if (mode_ == Mode::columns) {
        // The column that shows folder already has its cursor on child.
        if (!columns_.focus_folder(folder)) {
            columns_.reveal(folder, child.empty() ? folder : folder / child);
        }
        redraw_();
        return;
    }
    open(folder);
    if (const std::shared_ptr<Folder> shown = current_folder(); shown && !child.empty()) {
        shown->select_name(child);
    }
}

void Browser::view_pointer(const ui::PointerEvent& event, Clock::time_point now) {
    switch (mode_) {
        case Mode::columns:
            columns_.pointer(event, now);
            break;
        case Mode::grid:
            grid_.pointer(event, now);
            break;
        case Mode::details:
            details_.pointer(event, now);
            break;
    }
}

std::shared_ptr<Folder> Browser::current_folder() const {
    switch (mode_) {
        case Mode::grid:
            return grid_.folder();
        case Mode::details:
            return details_.folder();
        case Mode::columns:
            break;
    }
    return columns_.focused_folder();
}

bool Browser::key(const ui::KeyEvent& event, Clock::time_point now) {
    const Command command = command_for(event);
    if (shortcuts_.is_open()) {
        if (command == Command::show_shortcuts ||
            (event.keysym == XKB_KEY_Escape && event.modifiers == ui::Modifiers{})) {
            shortcuts_.close();
        }
        return true;  // nothing under them reacts meanwhile
    }
    if (quick_look_.is_open()) {
        if (command == Command::quick_look ||
            (event.keysym == XKB_KEY_Escape && event.modifiers == ui::Modifiers{})) {
            quick_look_.close();
            return true;
        }
        if (command != Command::none) {
            run(command, now);
            return true;
        }
        // The rest moves the view underneath; Quick Look shows where it lands.
        const bool handled = view_key(event, now);
        quick_look_.follow(current_folder());
        return handled;
    }
    // Space in the sidebar is nothing: Quick Look shows the view's entries.
    const bool ignored = command == Command::quick_look && sidebar_.focused();
    if (command != Command::none && !ignored) {
        run(command, now);
        return true;
    }
    if (sidebar_.focused()) {
        // The view does not see keys meanwhile.
        return listed(event, scope::kSidebar) && sidebar_.key(event);
    }
    return view_key(event, now);
}

void Browser::run(Command command, Clock::time_point now) {
    switch (command) {
        case Command::none:
            break;
        case Command::columns_view:
            set_mode(Mode::columns);
            break;
        case Command::grid_view:
            set_mode(Mode::grid);
            break;
        case Command::details_view:
            set_mode(Mode::details);
            break;
        case Command::toggle_info:
            set_info_visible(!info_visible_);
            break;
        case Command::toggle_sidebar:
            set_sidebar_visible(!sidebar_visible_);
            break;
        case Command::focus_sidebar:
            if (!sidebar_.focused()) {
                set_sidebar_visible(true);
            }
            sidebar_.set_focused(!sidebar_.focused());
            break;
        case Command::quick_look:
            if (quick_look_.is_open()) {
                quick_look_.close();
            } else {
                quick_look_.open(current_folder(), now);
            }
            break;
        case Command::show_shortcuts:
            shortcuts_.open();
            break;
    }
}

bool Browser::view_key(const ui::KeyEvent& event, Clock::time_point now) {
    // Only what the shortcuts list reaches the view.
    switch (mode_) {
        case Mode::columns:
            return listed(event, scope::kColumns) && columns_.key(event, now);
        case Mode::grid:
            return listed(event, scope::kGrid) && grid_.key(event, now);
        case Mode::details:
            return listed(event, scope::kDetails) && details_.key(event, now);
    }
    return false;
}

}  // namespace nordend::views
