#pragma once

#include <filesystem>
#include <functional>
#include <memory>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/file_watcher.hpp"
#include "nordend/theme/palette.hpp"

namespace nordend::theme {

// The current palette: loaded from colors.toml, reloaded when the file changes (Noctalia rewrites
// it on every theme change). A missing file means the default palette; a file that does not parse
// keeps the palette in use, so a half-written or hand-broken file never flashes the default
// colors. Problems are logged.
class Theme {
public:
    using ChangeHandler = std::function<void()>;

    explicit Theme(std::filesystem::path path);

    // Starts following the file. The theme must not outlive the loop.
    void watch(core::EventLoop& loop);

    // Called after the palette changed through a reload.
    void set_change_handler(ChangeHandler handler) { change_handler_ = std::move(handler); }

    [[nodiscard]] const Palette& palette() const noexcept { return palette_; }

    // Reads the file again; public so the reload rules can be tested without inotify.
    void reload();

private:
    std::filesystem::path path_;
    Palette palette_;
    ChangeHandler change_handler_;
    std::unique_ptr<core::FileWatcher> watcher_;
};

}  // namespace nordend::theme
