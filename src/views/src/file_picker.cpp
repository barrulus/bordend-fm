#include "rummage/views/file_picker.hpp"

#include <algorithm>
#include <fnmatch.h>
#include <set>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "draw_helpers.hpp"

namespace rummage::views {
namespace {
using Mode = core::PickerRequest::Mode;

bool valid_name(const std::string& name) {
    return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos &&
           name.find('\0') == std::string::npos;
}
}  // namespace

bool picker_matches(const core::PickerFilter& filter, const fs::Entry& entry,
                    const fs::MimeDatabase& mime) {
    if (entry.is_directory()) {
        return true;
    }
    return std::ranges::any_of(filter.patterns, [&](const auto& pattern) {
        if (pattern.kind == 0) {
            return fnmatch(pattern.value.c_str(), entry.name.c_str(), FNM_CASEFOLD) == 0;
        }
        if (pattern.value == "*" || pattern.value == "*/*") {
            return true;
        }
        const auto type =
            entry.mime == fs::kUnknownMime ? mime.by_name(entry.name).mime : entry.mime;
        if (pattern.value.ends_with("/*")) {
            const auto media = std::string_view{pattern.value}.substr(0, pattern.value.size() - 1);
            std::vector<fs::MimeId> waiting{type};
            std::set<fs::MimeId> seen;
            // Bound malformed or cyclic MIME subclass graphs.
            while (!waiting.empty() && seen.size() < 64) {
                const auto next = waiting.back();
                waiting.pop_back();
                if (!seen.insert(next).second) {
                    continue;
                }
                if (mime.name(next).starts_with(media)) {
                    return true;
                }
                const auto parents = mime.parents(next);
                waiting.insert(waiting.end(), parents.begin(), parents.end());
            }
            return false;
        }
        const auto parent = mime.find(pattern.value);
        return parent != fs::kUnknownMime && mime.is_a(type, parent);
    });
}

PickerSelection picker_selection(const core::PickerRequest& request,
                                 const std::filesystem::path& folder,
                                 const std::vector<std::string>& selected,
                                 const std::string& typed) {
    PickerSelection out;
    const auto fail = [&](std::string error) {
        out.paths.clear();
        out.error = std::move(error);
        return out;
    };
    if (!folder.is_absolute() || folder == fs::trash_path()) {
        return fail("Choose a folder on disk.");
    }
    std::vector<std::filesystem::path> paths;
    if (request.mode == Mode::save) {
        if (typed.empty()) {
            return fail("Enter a filename to save.");
        }
        if (!valid_name(typed)) {
            return fail("Use a filename other than “.” or “..”, without slashes.");
        }
        paths.push_back(folder / typed);
    } else if (request.mode == Mode::save_files) {
        if (request.files.empty()) {
            return fail("No filenames were supplied.");
        }
        const auto target = selected.size() == 1 ? folder / selected.front() : folder;
        if (selected.size() > 1) {
            return fail("Choose one destination folder.");
        }
        for (const auto& name : request.files) {
            if (!valid_name(name)) {
                return fail("The application supplied an invalid filename.");
            }
            paths.push_back(target / name);
        }
    } else {
        if (!typed.empty()) {
            paths.push_back(folder / typed);
        } else {
            for (const auto& name : selected) {
                paths.push_back(folder / name);
            }
        }
        if (paths.empty() && request.directory) {
            paths.push_back(folder);
        }
        if (paths.empty()) {
            return fail("Select a file first.");
        }
        if (!request.multiple && paths.size() != 1) {
            return fail(request.directory ? "Select just one folder." : "Select just one file.");
        }
    }
    for (auto path : paths) {
        path = path.lexically_normal();
        std::error_code ec;
        const auto status = std::filesystem::status(path, ec);
        const bool exists = std::filesystem::exists(status);
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return fail("Cannot access “" + path.string() + "”: " + ec.message() + ".");
        }
        if (request.mode == Mode::open) {
            if (!exists) {
                const auto link = std::filesystem::symlink_status(path, ec);
                if (std::filesystem::is_symlink(link)) {
                    return fail("“" + path.filename().string() + "” is a broken symbolic link.");
                }
                const std::string kind = request.directory ? "Folder" : "File";
                if (!typed.empty()) {
                    return fail(kind + " “" + typed + "” does not exist." +
                                (request.directory
                                     ? " Use New Folder to create it first."
                                     : " Check the name or choose an existing file."));
                }
                return fail("The selected " + std::string{request.directory ? "folder" : "file"} +
                            " “" + path.filename().string() +
                            "” no longer exists. Choose another.");
            }
            if (request.directory != std::filesystem::is_directory(status)) {
                return fail("“" + path.filename().string() + "” " +
                            (request.directory ? "is not a folder. Choose a folder."
                                               : "is a folder. Open it to choose a file."));
            }
        } else {
            if (!std::filesystem::is_directory(path.parent_path(), ec)) {
                return fail("The destination folder does not exist.");
            }
            if (access(path.parent_path().c_str(), W_OK | X_OK) != 0) {
                return fail("The destination folder is not writable.");
            }
            if (exists && !std::filesystem::is_regular_file(status)) {
                return fail("The destination is not a regular file.");
            }
            // lstat catches a dangling symlink too: it must never silently become a new file.
            const auto link = std::filesystem::symlink_status(path, ec);
            if (std::filesystem::is_symlink(link) && !exists) {
                return fail("The destination is a broken symbolic link.");
            }
            if (exists && access(path.c_str(), W_OK) != 0) {
                return fail("The destination file is not writable.");
            }
            out.overwrite = out.overwrite || exists;
        }
        out.paths.push_back(path.string());
    }
    return out;
}

FilePicker::FilePicker(Browser& browser, core::PickerRequest request,
                       std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw,
                       Done done)
    : browser_{browser},
      request_{std::move(request)},
      mime_{std::move(mime)},
      redraw_{std::move(redraw)},
      done_{std::move(done)},
      name_{request_.name},
      menu_{redraw_},
      confirm_{redraw_},
      controls_(5 + request_.choices.size()) {
    browser_.set_panes_enabled(false);
    focus_ = request_.mode == Mode::save ? 1 : 0;
    if (focus_ == 1) {
        set_name(request_.name);
    }
    for (auto& choice : request_.choices) {
        if (choice.options.empty()) {
            if (choice.selected != "true") {
                choice.selected = "false";
            }
        } else if (std::ranges::none_of(choice.options,
                                        [&](const auto& o) { return o.id == choice.selected; })) {
            choice.selected = choice.options.front().id;
        }
    }
    set_filter(request_.filter);
    browser_.set_open_handler([this](const auto&, const auto& files) {
        if (request_.mode == Mode::save && !files.empty()) {
            set_name(files.front().name);
        }
        if (request_.mode == Mode::open) {
            name_.set_text({});
        }
        accept();
    });
    sync_selection();
}

FilePicker::~FilePicker() {
    browser_.set_open_handler({});
    browser_.set_entry_filter({});
}

void FilePicker::set_name(std::string name) {
    const auto dot = name.rfind('.');
    const auto stem = dot == std::string::npos || dot == 0 ? name.size() : dot;
    name_.set_text(std::move(name));
    if (request_.mode == Mode::save) {
        name_.select(0, stem);
    }
    error_.clear();
    redraw_();
}

void FilePicker::set_filter(std::uint32_t index) {
    if (index >= request_.filters.size() && !request_.filters.empty()) {
        return;
    }
    request_.filter = index;
    const auto filter =
        request_.filters.empty() ? std::optional<core::PickerFilter>{} : request_.filters[index];
    const bool directories = request_.directory || request_.mode == Mode::save_files;
    browser_.set_entry_filter([filter, directories, mime = mime_](const fs::Entry& entry) {
        return entry.is_directory() ||
               (!directories && (!filter || picker_matches(*filter, entry, *mime)));
    });
    redraw_();
}

void FilePicker::finish(std::vector<std::string> paths) {
    if (!done_) {
        return;
    }
    core::PickerResult result;
    result.response = 0;
    result.paths = std::move(paths);
    result.filter = request_.filter;
    for (const auto& choice : request_.choices) {
        result.choices.push_back(choice.selected);
    }
    auto done = std::move(done_);
    done(std::move(result));
}

void FilePicker::accept() {
    sync_selection();
    const auto folder = browser_.picker_folder();
    if (!folder || folder->model().loading()) {
        return;
    }
    if (request_.mode == Mode::save && valid_name(name_.text()) && folder->path().is_absolute() &&
        !folder->is_trash()) {
        std::error_code ec;
        const auto target = folder->path() / name_.text();
        if (std::filesystem::is_directory(target, ec)) {
            browser_.open(target);
            set_name(request_.name);
            error_.clear();
            return;
        }
    }
    auto selected = folder->selected_names();
    if (selected.empty() && folder->cursor_entry()) {
        selected.push_back(folder->cursor_entry()->name);
    }
    const auto result = picker_selection(request_, folder->path(), selected, name_.text());
    error_ = result.error;
    if (error_.empty()) {
        if (result.overwrite) {
            confirm_.open(
                result.paths.size() == 1
                    ? "Replace “" +
                          std::filesystem::path{result.paths.front()}.filename().string() + "”?"
                    : "Replace existing files?",
                folder->path().string(), "Replace",
                [this, paths = result.paths](bool yes) {
                    if (yes) {
                        finish(paths);
                    }
                },
                "The application will overwrite the selected file when it saves.");
        } else {
            finish(result.paths);
        }
    }
    redraw_();
}

void FilePicker::cancel() {
    if (done_) {
        auto done = std::move(done_);
        done(core::PickerResult{});
    }
}

void FilePicker::sync_selection() {
    const auto folder = browser_.picker_folder();
    if (!folder) {
        return;
    }
    std::vector<std::string> selection{folder->path().string()};
    const auto* cursor = folder->cursor_entry();
    selection.push_back(cursor ? cursor->name : std::string{});
    const auto names = folder->selected_names();
    selection.insert(selection.end(), names.begin(), names.end());
    if (selection == last_selection_) {
        return;
    }
    const bool same_folder =
        !last_selection_.empty() && last_selection_.front() == selection.front();
    last_selection_ = std::move(selection);
    error_.clear();
    if (request_.mode == Mode::open) {
        name_.set_text({});
    }
    if (request_.mode == Mode::save && same_folder && cursor && !cursor->is_directory()) {
        set_name(cursor->name);
    }
}

ui::CursorShape FilePicker::cursor_at(float x, float y) const {
    if (confirm_.is_open()) {
        return confirm_.clickable_at(x, y) ? ui::CursorShape::pointer : ui::CursorShape::standard;
    }
    if (!footer_.contains(x, y)) {
        return browser_.cursor_at(x, y);
    }
    if (controls_[1].contains(x, y)) {
        return ui::CursorShape::text;
    }
    for (std::size_t i = 2; i < controls_.size(); ++i) {
        if (controls_[i].contains(x, y) && (i < 5 || choices_viewport_.contains(x, y))) {
            return ui::CursorShape::pointer;
        }
    }
    return ui::CursorShape::standard;
}

std::string FilePicker::copy_name(bool cut) {
    const auto [first, last] = name_.selection();
    auto text = name_.text().substr(first, last - first);
    if (cut && first != last) {
        name_.insert({});
        error_.clear();
        redraw_();
    }
    return text;
}

void FilePicker::paste(std::string_view text) {
    const auto end = text.find_first_of("\r\n");
    name_.insert(text.substr(0, end));
    error_.clear();
    redraw_();
}

void FilePicker::filters_menu(bool keyboard) {
    std::vector<MenuItem> items;
    for (std::size_t i = 0; i < request_.filters.size(); ++i) {
        MenuItem item;
        item.label = request_.filters[i].label;
        item.radio = true;
        item.checked = i == request_.filter;
        item.action = [this, i] { set_filter(static_cast<std::uint32_t>(i)); };
        items.push_back(std::move(item));
    }
    menu_.open(controls_[2].x, controls_[2].y, std::move(items), keyboard);
}

void FilePicker::choice_control(std::size_t index, bool keyboard) {
    auto& choice = request_.choices.at(index);
    if (choice.options.empty()) {
        choice.selected = choice.selected == "true" ? "false" : "true";
        redraw_();
        return;
    }
    std::vector<MenuItem> items;
    for (const auto& option : choice.options) {
        MenuItem item;
        item.label = option.label;
        item.radio = true;
        item.checked = option.id == choice.selected;
        item.action = [this, index, value = option.id] {
            request_.choices[index].selected = value;
            redraw_();
        };
        items.push_back(std::move(item));
    }
    const auto& control = controls_.at(5 + index);
    menu_.open(control.x, control.y, std::move(items), keyboard);
}

bool FilePicker::focusable(int control) const {
    return !(control == 1 && request_.mode == Mode::save_files) &&
           !(control == 2 && request_.filters.empty());
}

void FilePicker::activate(int control, bool keyboard) {
    if (control == 2) {
        filters_menu(keyboard);
    } else if (control == 3) {
        cancel();
    } else if (control >= 5) {
        choice_control(static_cast<std::size_t>(control - 5), keyboard);
    } else {
        accept();
    }
}

bool FilePicker::key(const ui::KeyEvent& event) {
    if (confirm_.key(event) || menu_.key(event)) {
        return true;
    }
    const auto command = command_for(event);
    if (!browser_.has_overlay() &&
        (command == Command::filter_names || command == Command::new_folder)) {
        focus_ = 0;
        return browser_.key(event, Browser::Clock::now());
    }
    // The browser's live filter owns text editing and Escape while it has focus.
    if (focus_ == 0 && browser_.filtering() && !browser_.has_overlay()) {
        if (event.keysym != XKB_KEY_Tab && event.keysym != XKB_KEY_ISO_Left_Tab) {
            return browser_.key(event, Browser::Clock::now());
        }
        browser_.blur_filter();
    }
    if (event.keysym == XKB_KEY_Escape) {
        if (browser_.has_overlay()) {
            browser_.key(event, Browser::Clock::now());
        } else {
            cancel();
        }
        return true;
    }
    if (browser_.has_overlay()) {
        return browser_.key(event, Browser::Clock::now());
    }
    if ((event.keysym == XKB_KEY_Tab || event.keysym == XKB_KEY_ISO_Left_Tab) &&
        !event.modifiers.ctrl && !event.modifiers.alt) {
        const int step = event.modifiers.shift || event.keysym == XKB_KEY_ISO_Left_Tab ? -1 : 1;
        std::vector<int> order{0, 1, 2};
        for (std::size_t i = 5; i < controls_.size(); ++i) {
            order.push_back(static_cast<int>(i));
        }
        order.insert(order.end(), {3, 4});
        auto index = static_cast<int>(std::ranges::find(order, focus_) - order.begin());
        do {
            const int count = static_cast<int>(order.size());
            index = (index + step + count) % count;
            focus_ = order[static_cast<std::size_t>(index)];
        } while (!focusable(focus_));
        reveal_choice_ = focus_ >= 5;
        redraw_();
        return true;
    }
    const bool enter = event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter;
    if (enter && event.modifiers.ctrl) {
        accept();
        return true;
    }
    if ((enter || (event.keysym == XKB_KEY_space && focus_ >= 2)) && focus_ != 0) {
        activate(focus_, true);
        return true;
    }
    if (focus_ == 1) {
        if (name_.key(event)) {
            error_.clear();
        }
        redraw_();
        return true;
    }
    if (focus_ == 0) {
        const bool handled = browser_.key(event, Browser::Clock::now());
        sync_selection();
        return handled;
    }
    return true;
}

void FilePicker::pointer(const ui::PointerEvent& event) {
    if (confirm_.is_open()) {
        confirm_.pointer(event);
        return;
    }
    if (menu_.pointer(event)) {
        return;
    }
    if (browser_.has_overlay()) {
        browser_.pointer(event, Browser::Clock::now());
        return;
    }
    using Kind = ui::PointerEvent::Kind;
    if (event.kind == Kind::scroll && choices_viewport_.contains(event.x, event.y)) {
        const float delta = event.wheel_y != 0 ? event.wheel_y * 40 : event.scroll_y;
        choices_scroll_ = std::clamp(choices_scroll_ + delta, 0.0F, choices_scroll_max_);
        redraw_();
        return;
    }
    const bool footer = footer_.contains(event.x, event.y);
    if (event.kind == Kind::press && event.button == ui::PointerButton::left) {
        if (!footer) {
            focus_ = 0;
            browser_pressed_ = true;
        } else {
            browser_.blur_filter();
            browser_.pointer({.kind = Kind::leave}, Browser::Clock::now());
            for (std::size_t i = 1; i < controls_.size(); ++i) {
                if (!controls_[i].contains(event.x, event.y) ||
                    (i >= 5 && !choices_viewport_.contains(event.x, event.y))) {
                    continue;
                }
                focus_ = static_cast<int>(i);
                if (i == 1) {
                    name_.click(event.x, false);
                }
                if (i >= 2) {
                    activate(static_cast<int>(i), false);
                }
                redraw_();
                return;
            }
        }
    }
    if (!footer || browser_pressed_ || event.kind == Kind::leave) {
        browser_.pointer(event, Browser::Clock::now());
        sync_selection();
    }
    if (event.kind == Kind::release || event.kind == Kind::leave) {
        browser_pressed_ = false;
    }
}

void FilePicker::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                      ui::IconSet& icons) {
    // Choices occupy their own wrapping rows, keeping every application option visible
    // without squeezing the filename or the primary actions.
    const float available = std::max(1.0F, bounds.width - 24.0F);
    std::vector<std::string> labels;
    std::vector<ui::Rect> choices;
    float x = 0;
    float choice_height = 0;
    for (const auto& choice : request_.choices) {
        std::string label = choice.label;
        if (!choice.options.empty()) {
            const auto selected =
                std::ranges::find(choice.options, choice.selected, &core::PickerOption::id);
            label += ": " + (selected == choice.options.end() ? choice.selected : selected->label);
        }
        const float width = std::min(
            available,
            std::clamp(text_width(renderer, label, 13) + (choice.options.empty() ? 44.0F : 28.0F),
                       100.0F, 320.0F));
        if (x > 0 && x + width > available) {
            x = 0;
            choice_height += 40;
        }
        choices.push_back({x, choice_height, width, 32});
        labels.push_back(std::move(label));
        x += width + 8;
    }
    if (!choices.empty()) {
        choice_height += 40;
    }
    const bool has_name = request_.mode != Mode::save_files;
    const bool has_filter = !request_.filters.empty();
    const float visible_choices = std::min(choice_height, bounds.height < 480 ? 40.0F : 80.0F);
    choices_scroll_max_ = choice_height - visible_choices;
    choices_scroll_ = std::clamp(choices_scroll_, 0.0F, choices_scroll_max_);
    if (reveal_choice_ && focus_ >= 5) {
        const auto& rect = choices.at(static_cast<std::size_t>(focus_ - 5));
        choices_scroll_ = std::clamp(choices_scroll_, rect.bottom() - visible_choices, rect.y);
        choices_scroll_ = std::clamp(choices_scroll_, 0.0F, choices_scroll_max_);
        reveal_choice_ = false;
    }
    const float height =
        78 + (has_name ? 40.0F : 0.0F) + (has_filter ? 40.0F : 0.0F) + visible_choices;
    ui::Rect content = bounds;
    content.height = std::max(0.0F, content.height - height);
    browser_.draw(renderer, content, palette, icons, Browser::Clock::now());
    footer_ = {bounds.x, content.bottom(), bounds.width, height};
    renderer.fill_background(footer_, palette.surface_container);
    std::ranges::fill(controls_, ui::Rect{});
    float y = footer_.y + 10.0F;
    if (has_name) {
        renderer.draw_text(name_label(), bounds.x + 12, y + 7, 13, palette.on_surface);
        controls_[1] = {bounds.x + 64, y, std::max(1.0F, bounds.width - 76), 32};
        name_.draw(renderer, controls_[1], 14,
                   {.background = palette.surface,
                    .border = focus_ == 1 ? palette.primary : palette.outline_variant,
                    .text = palette.on_surface,
                    .selection = palette.primary_container,
                    .caret = palette.on_surface});
        y += 40;
    }
    const auto button = [&](std::size_t i, const std::string& text, ui::Rect rect, bool primary,
                            bool checkbox = false, bool checked = false) {
        controls_[i] = rect;
        renderer.draw_box(rect,
                          {.fill = primary ? palette.primary : palette.surface_container_highest,
                           .radius = {},
                           .border_width = focus_ == static_cast<int>(i) ? 2.0F : 0.0F,
                           .border = palette.primary});
        if (checkbox) {
            renderer.draw_box({rect.x + 10, rect.y + 9, 14, 14},
                              {.fill = palette.surface,
                               .radius = {},
                               .border_width = 1,
                               .border = palette.on_surface_variant});
            if (checked) {
                renderer.fill_rect({rect.x + 13, rect.y + 12, 8, 8}, palette.primary);
            }
        }
        const float inset = checkbox ? 32.0F : 10.0F;
        renderer.draw_text(text, rect.x + inset, rect.y + 8, 13,
                           primary ? palette.on_primary : palette.on_surface,
                           rect.width - inset - 10);
    };
    if (has_filter) {
        button(2, request_.filters[request_.filter].label, {bounds.x + 12, y, available, 32},
               false);
        y += 40;
    }
    choices_viewport_ = {bounds.x + 12, y, available, visible_choices};
    renderer.push_clip(choices_viewport_);
    for (std::size_t i = 0; i < choices.size(); ++i) {
        auto rect = choices[i];
        rect.x += bounds.x + 12;
        rect.y += y - choices_scroll_;
        button(5 + i, labels[i], rect, false, request_.choices[i].options.empty(),
               request_.choices[i].selected == "true");
    }
    renderer.pop_clip();
    if (choices_scroll_max_ > 0) {
        const float thumb = visible_choices * visible_choices / choice_height;
        renderer.fill_rect(
            {bounds.right() - 6,
             y + (visible_choices - thumb) * choices_scroll_ / choices_scroll_max_, 3, thumb},
            palette.outline);
    }
    y += visible_choices;
    const float right = bounds.right() - 12;
    const float action_width = std::min(108.0F, (available - 8) / 2);
    button(4,
           request_.accept_label.empty() ? (request_.mode == Mode::open ? "Select" : "Save")
                                         : request_.accept_label,
           {right - action_width, y, action_width, 32}, true);
    button(3, "Cancel", {right - 2 * action_width - 8, y, action_width, 32}, false);
    renderer.draw_text(error_.empty() ? "Ctrl+Enter selects · Esc cancels" : error_, bounds.x + 12,
                       footer_.bottom() - 27, 13,
                       error_.empty() ? palette.on_surface_variant : palette.error,
                       bounds.width - 24);
    menu_.draw(renderer, bounds, palette);
    confirm_.draw(renderer, bounds, palette);
}
}  // namespace rummage::views
