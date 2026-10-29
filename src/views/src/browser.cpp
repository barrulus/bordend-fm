#include "nordend/views/browser.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <string>
#include <utility>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "draw_helpers.hpp"
#include "nordend/fs/operations.hpp"
#include "nordend/views/format.hpp"

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
    : pool_{pool},
      redraw_{redraw},
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
      status_bar_{loop, pool, redraw,
                  [this](std::size_t view) { set_mode(static_cast<Mode>(view)); },
                  [this] { shortcuts_.open(); }},
      shortcuts_{redraw},
      editor_{redraw,
              [this](const std::shared_ptr<Folder>& folder, const std::string& from,
                     const std::string& to) { rename(folder, from, to); },
              [this](std::string problem) { show_message(std::move(problem), true); }},
      conflict_{std::move(redraw)},
      transfers_{loop,
                 [this](const fs::TransferRequest& request, const fs::TransferProgress& progress) {
                     transfer_progress(request, progress);
                 },
                 [this](const fs::Conflict& conflict, std::function<void(fs::Resolution)> answer) {
                     ask(conflict, std::move(answer));
                 },
                 [this](const fs::TransferRequest& request, const fs::TransferResult& result) {
                     transfer_done(request, result);
                 }} {
    columns_.set_open_handler([this] { open_selection(); });
    grid_.set_open_handler([this] { open_selection(); });
    details_.set_open_handler([this] { open_selection(); });
    columns_.set_edit_slot(&editor_.slot());
    grid_.set_edit_slot(&editor_.slot());
    details_.set_edit_slot(&editor_.slot());
    columns_.set_cut_marks(&cut_marks_);
    grid_.set_cut_marks(&cut_marks_);
    details_.set_cut_marks(&cut_marks_);
    status_bar_.set_cancel_handler([this] { cancel_transfer(); });
}

std::vector<std::string> Browser::memory_lines() const {
    const auto mb = [](std::size_t bytes) {
        return std::format("{:.1f} MB", static_cast<double>(bytes) / 1e6);
    };
    std::vector<std::string> lines;
    if (thumbnails_) {
        const Thumbnails::Usage usage = thumbnails_->usage();
        lines.push_back(
            std::format("thumbnails: {} slots, {} textures {} (budget {}), pixels "
                        "waiting {}",
                        usage.slots, usage.textures, mb(usage.texture_bytes),
                        mb(Thumbnails::kBudgetBytes), mb(usage.pixel_bytes)));
    }
    const QuickLook::Usage look = quick_look_.usage();
    lines.push_back(std::format("quick look: {} slots, textures {}, pixels kept {}", look.slots,
                                mb(look.texture_bytes), mb(look.pixel_bytes)));
    lines.push_back(std::format("glyph atlases: {}", mb(ui::Renderer::atlas_bytes())));
    std::size_t folders = 0;
    std::size_t entries = 0;
    for (std::size_t i = 0; i < columns_.column_count(); ++i) {
        ++folders;
        entries += columns_.column(i).folder().entries().size();
    }
    lines.push_back(std::format("columns: {} folders, {} entries", folders, entries));
    if (const std::shared_ptr<Folder> shown = current_folder(); shown && mode_ != Mode::columns) {
        lines.push_back(std::format("shown folder: {} entries", shown->entries().size()));
    }
    return lines;
}

void Browser::take_selection(bool cut) {
    const std::shared_ptr<Folder> folder = current_folder();
    if (!folder) {
        return;
    }
    std::vector<std::string> names = folder->selected_names();
    if (names.empty()) {
        if (const fs::Entry* const cursor = folder->cursor_entry()) {
            names.push_back(cursor->name);
        }
    }
    if (names.empty()) {
        return;
    }
    const std::size_t count = names.size();
    clipboard_ = {.directory = folder->path(), .names = std::move(names), .cut = cut};
    cut_marks_ = {};
    if (cut) {
        cut_marks_ = {.directory = clipboard_.directory,
                      .names = {clipboard_.names.begin(), clipboard_.names.end()}};
    }
    const std::string items =
        count == 1 ? std::format("“{}”", clipboard_.names.front()) : std::format("{} items", count);
    show_message(cut ? std::format("Ctrl+V moves {} where you want them", items)
                     : std::format("Ctrl+V copies {} where you want them", items),
                 false);
}

void Browser::paste() {
    const std::shared_ptr<Folder> folder = current_folder();
    if (!folder) {
        return;
    }
    if (clipboard_.names.empty()) {
        show_message("Nothing to paste: Ctrl+C or Ctrl+X first", false);
        return;
    }
    const bool cut = clipboard_.cut;
    transfers_.submit({.kind = cut ? fs::TransferKind::move : fs::TransferKind::copy,
                       .source = clipboard_.directory,
                       .names = clipboard_.names,
                       .destination = folder->path(),
                       .force_copy = false});
    if (cut) {
        clipboard_ = {};  // moved once: there is nothing left to move
        cut_marks_ = {};
    }
    if (!status_bar_.transfer()) {
        status_bar_.set_transfer(TransferStatus{
            .text = cut ? "Getting ready to move…" : "Getting ready to copy…", .fraction = -1.0F});
    }
    redraw_();
}

void Browser::cancel_transfer() {
    transfers_.cancel();
    conflict_.close();
}

void Browser::ask(const fs::Conflict& conflict, std::function<void(fs::Resolution)> answer) {
    if (for_the_rest_ && (*for_the_rest_ != fs::Resolution::replace ||
                          conflict.directory == conflict.target_directory)) {
        answer(*for_the_rest_);
        return;
    }
    copy_started_.reset();  // the time spent on the question does not count in the rate
    conflict_.open(conflict, conflict.destination.filename().string(),
                   [this, answer = std::move(answer)](fs::Resolution resolution, bool rest) {
                       if (rest) {
                           for_the_rest_ = resolution;
                       }
                       answer(resolution);
                   });
}

namespace {

// "8 s", "2 min", "1 h 5 min".
std::string format_duration(double seconds) {
    const long whole = std::lround(seconds);
    if (whole < 60) {
        return std::format("{} s", std::max(whole, 1L));
    }
    if (whole < 3600) {
        return std::format("{} min", (whole + 30) / 60);
    }
    return std::format("{} h {} min", whole / 3600, (whole % 3600) / 60);
}

}  // namespace

void Browser::transfer_progress(const fs::TransferRequest& request,
                                const fs::TransferProgress& progress) {
    const bool copy = request.kind == fs::TransferKind::copy;
    const std::size_t items = request.names.size();
    TransferStatus status;
    if (progress.preparing) {
        status.text = std::format("Getting ready to {} {} {}…", copy ? "copy" : "move", items,
                                  items == 1 ? "item" : "items");
    } else if (progress.bytes_total > 0) {
        const auto now = std::chrono::steady_clock::now();
        if (!copy_started_) {
            copy_started_ = RateStart{.time = now, .bytes = progress.bytes_done};
        }
        status.fraction = static_cast<float>(static_cast<double>(progress.bytes_done) /
                                             static_cast<double>(progress.bytes_total));
        status.text =
            std::format("{} {} {} · {} of {}", copy ? "Copying" : "Moving", progress.files_total,
                        progress.files_total == 1 ? "file" : "files",
                        format_size(progress.bytes_done), format_size(progress.bytes_total));
        const double seconds = std::chrono::duration<double>(now - copy_started_->time).count();
        if (seconds >= 1.0 && progress.bytes_done > copy_started_->bytes) {
            const double rate =
                static_cast<double>(progress.bytes_done - copy_started_->bytes) / seconds;
            status.text += std::format(
                " · {}/s · {} left", format_size(static_cast<std::uint64_t>(rate)),
                format_duration(static_cast<double>(progress.bytes_total - progress.bytes_done) /
                                rate));
        }
    } else {
        status.fraction = progress.files_total > 0 ? static_cast<float>(progress.files_done) /
                                                         static_cast<float>(progress.files_total)
                                                   : -1.0F;
        status.text =
            std::format("Moving {} of {} items", progress.files_done, progress.files_total);
    }
    if (transfers_.pending() > 1) {
        status.text += std::format(" · {} more waiting", transfers_.pending() - 1);
    }
    status_bar_.set_transfer(std::move(status));
}

void Browser::transfer_done(const fs::TransferRequest& request, const fs::TransferResult& result) {
    for_the_rest_.reset();
    copy_started_.reset();
    conflict_.close();
    if (transfers_.pending() == 0) {
        status_bar_.set_transfer(std::nullopt);
    }
    const bool copy = request.kind == fs::TransferKind::copy;
    const std::string_view done = copy ? "Copied" : "Moved";
    const std::size_t files = result.files_done;
    const std::string what = std::format("{} {}", files, files == 1 ? "file" : "files");
    if (!result.refused.empty()) {
        show_message(result.refused, true);
    } else if (result.cancelled) {
        show_message(std::format("Stopped: {} {} so far", what, copy ? "copied" : "moved"), false);
    } else if (!result.errors.empty()) {
        show_message(std::format("{} {}; {} could not be: {}", done, what, result.errors.size(),
                                 result.errors.front()),
                     true);
    } else if (result.placed.empty()) {
        show_message("Nothing to do: every name was skipped", false);
    } else if (result.bytes_done > 0) {
        show_message(std::format("{} {} ({})", done, what, format_size(result.bytes_done)), false);
    } else {
        show_message(std::format("{} {}", done, what), false);
    }
    // What arrived is selected, when its folder is the one shown.
    if (const std::shared_ptr<Folder> folder = current_folder();
        folder && folder->path() == request.destination && !result.placed.empty()) {
        folder->expect_names(result.placed);
    }
}

Browser::~Browser() { *alive_ = false; }

void Browser::start_rename() {
    const std::shared_ptr<Folder> folder = current_folder();
    const fs::Entry* const entry = folder ? folder->cursor_entry() : nullptr;
    if (entry == nullptr) {
        return;
    }
    editor_.start(folder, entry->name, entry->is_directory());
}

void Browser::new_folder() {
    std::shared_ptr<Folder> folder = current_folder();
    if (!folder) {
        return;
    }
    ++operations_;
    pool_.submit(
        core::Priority::visible,
        [directory = folder->path()](const core::CancelToken& /*token*/) {
            return fs::make_folder(directory, "New Folder");
        },
        [this, alive = alive_, folder](const fs::MadeFolder& made) {
            if (!*alive) {
                return;
            }
            --operations_;
            if (made.error) {
                show_message(std::format("Cannot make a folder in {}: {}",
                                         folder->path().filename().string(), made.error.message()),
                             true);
                return;
            }
            folder->expect_name(made.name);  // lands when the watcher reports it
            if (current_folder() == folder && !editor_.active()) {
                editor_.start(folder, made.name, true);
            }
        });
}

void Browser::rename(const std::shared_ptr<Folder>& folder, const std::string& from,
                     const std::string& to) {
    ++operations_;
    pool_.submit(
        core::Priority::visible,
        [directory = folder->path(), from, to](const core::CancelToken& /*token*/) {
            return fs::rename_entry(directory, from, to);
        },
        [this, alive = alive_, folder, to](const fs::Renamed& renamed) {
            if (!*alive) {
                return;
            }
            --operations_;
            if (renamed.error) {
                show_message(renamed.problem, true);
                editor_.finished(false);
                return;
            }
            // The cursor follows the new name by itself (the watcher reports a rename), unless it
            // still waits for the old one: a new folder renamed before the watcher saw it.
            if (folder->cursor_pending()) {
                folder->expect_name(to);
            }
            if (!renamed.problem.empty()) {
                show_message(renamed.problem, true);
            } else if (renamed.sidecars > 0) {
                show_message(std::format("Renamed {} with {} darktable sidecar{}", to,
                                         renamed.sidecars, renamed.sidecars == 1 ? "" : "s"),
                             false);
            }
            editor_.finished(true);
        });
}

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
    if (editor_.active() && !editor_.busy()) {
        // The entry went away meanwhile (another program): nothing left to rename. A new folder
        // not listed yet is still on its way: the cursor waits for it.
        const Folder& edited = *editor_.folder();
        if (!edited.cursor_pending() &&
            std::ranges::find(edited.entries(), editor_.slot().name, &fs::Entry::name) ==
                edited.entries().end()) {
            editor_.cancel();
        }
    }
    editor_.slot().rect.reset();  // the view places it again if it shows the name
    draw_view(renderer,
              {view.x, view.y + kTitleHeight, view.width,
               std::max(0.0F, view.height - kTitleHeight - bar)},
              palette, icons, now);
    editor_.draw(renderer, palette);
    if (quick_look_.is_open()) {
        quick_look_.follow(current_folder());  // entries may have moved or gone meanwhile
    }
    quick_look_.draw(renderer, bounds, palette, icons, now);
    shortcuts_.draw(renderer, bounds, palette);
    conflict_.draw(renderer, bounds, palette);  // last: over everything else
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
    if (operations_ > 0 || transfers_.pending() > 0 || quick_look_.loading() ||
        info_pane_.loading() || status_bar_.loading()) {
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
    if (conflict_.is_open()) {
        conflict_.pointer(event);  // the question first: nothing under it reacts
        return;
    }
    // While a name is edited, a press in its field is the field's; elsewhere it keeps the name,
    // then does what it does.
    if (editor_.pointer(event)) {
        return;
    }
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
    if (conflict_.is_open()) {
        return conflict_.key(event);  // a copy waits for the answer: every key is the card's
    }
    if (editor_.active()) {
        return editor_.key(event);  // every key is the name's meanwhile
    }
    const Command command = command_for(event);
    if (event.keysym == XKB_KEY_Escape && event.modifiers == ui::Modifiers{} &&
        !cut_marks_.names.empty() && !shortcuts_.is_open() && !quick_look_.is_open() &&
        !sidebar_.focused()) {
        clipboard_ = {};  // Escape keeps cut entries where they are; the view gets it too
        cut_marks_ = {};
        redraw_();
        view_key(event, now);
        return true;
    }
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
    // What is not listed for the sidebar is nothing there (Space: Quick Look shows the view's
    // entries; F2: the sidebar's places are not renamed).
    const bool ignored = sidebar_.focused() && !listed(event, scope::kSidebar);
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
        case Command::rename:
            quick_look_.close();
            start_rename();
            break;
        case Command::new_folder:
            quick_look_.close();
            new_folder();
            break;
        case Command::copy:
            take_selection(false);
            break;
        case Command::cut:
            take_selection(true);
            break;
        case Command::paste:
            quick_look_.close();
            paste();
            break;
    }
}

bool Browser::open_selection() {
    const std::shared_ptr<Folder> folder = current_folder();
    const fs::Entry* const cursor = folder ? folder->cursor_entry() : nullptr;
    if (cursor == nullptr || cursor->is_directory() || !open_) {
        return false;
    }
    const auto with_type = [&folder](fs::Entry entry) {
        entry.mime = folder->mime_of(entry);
        return entry;
    };
    std::vector<fs::Entry> files;
    if (!folder->is_selected(cursor->name)) {
        files.push_back(with_type(*cursor));
    } else {
        for (const fs::Entry& entry : folder->entries()) {
            if (!entry.is_directory() && folder->is_selected(entry.name)) {
                files.push_back(with_type(entry));
            }
        }
    }
    open_(folder->path(), std::move(files));
    return true;
}

bool Browser::view_key(const ui::KeyEvent& event, Clock::time_point now) {
    if ((event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) &&
        event.modifiers == ui::Modifiers{} && open_selection()) {
        return true;  // a file: its application; a folder goes on to the view
    }
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
