#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/file_watcher.hpp"
#include "nordend/theme/desktop.hpp"
#include "nordend/theme/gtk.hpp"
#include "nordend/theme/palette.hpp"

namespace nordend::theme {

// The current palette, from the first of: colors.toml (an override with every role; a file that
// does not parse keeps the palette in use, so a half-written or hand-broken file never flashes
// other colours), the user's GTK colours (gtk.css, as GTK apps follow them), the desktop's look
// (set_desktop: light or dark, accent), the default palette. Each source is followed live.
// Problems are logged.
class Theme {
public:
    using ChangeHandler = std::function<void()>;

    explicit Theme(std::filesystem::path path,
                   std::vector<std::filesystem::path> gtk_css = default_gtk_css_paths());

    // Starts following the files. The theme must not outlive the loop.
    void watch(core::EventLoop& loop);

    // What the desktop says about its look; nullopt when it says nothing (no portal). Used while
    // there is no colors.toml.
    void set_desktop(std::optional<Desktop> desktop);

    // Called after the palette changed through a reload or the desktop.
    void set_change_handler(ChangeHandler handler) { change_handler_ = std::move(handler); }

    [[nodiscard]] const Palette& palette() const noexcept { return palette_; }

    // The accent the palette follows: colors.toml's primary, else the desktop's own accent;
    // nullopt with the default palette or a desktop that gives none (ROADMAP 9.2).
    [[nodiscard]] std::optional<Color> accent() const;

    // Reads the file again; public so the reload rules can be tested without inotify.
    void reload();

    // Reads the GTK colours again; public for the same reason.
    void reload_gtk();

private:
    // The palette when colors.toml is missing.
    [[nodiscard]] Palette fallback() const;
    [[nodiscard]] std::optional<Palette> from_gtk() const;
    void changed();
    // Follows every stylesheet the GTK colours came from, and the gtk.css that could appear.
    void watch_gtk();

    std::filesystem::path path_;
    std::vector<std::filesystem::path> gtk_css_;
    bool from_file_{false};
    GtkColors gtk_;
    std::optional<Desktop> desktop_;
    Palette palette_;
    ChangeHandler change_handler_;
    core::EventLoop* loop_{nullptr};
    std::unique_ptr<core::FileWatcher> watcher_;
    std::vector<std::unique_ptr<core::FileWatcher>> gtk_watchers_;
};

}  // namespace nordend::theme
