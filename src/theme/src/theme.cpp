#include "nordend/theme/theme.hpp"

#include <string>
#include <utility>

#include "nordend/core/log.hpp"

namespace nordend::theme {
namespace {

void log_problems(const PaletteResult& result) {
    for (const std::string& problem : result.problems) {
        log::warn("theme: {}", problem);
    }
}

}  // namespace

Theme::Theme(std::filesystem::path path) : path_{std::move(path)} {
    const PaletteResult result = load_palette(path_);
    log_problems(result);
    palette_ = result.palette;
    log::info("theme: {}", result.from_file ? path_.string() : "default palette");
}

void Theme::watch(core::EventLoop& loop) {
    if (path_.empty()) {
        return;
    }
    watcher_ = std::make_unique<core::FileWatcher>(loop, path_, [this] { reload(); });
}

void Theme::reload() {
    const PaletteResult result = load_palette(path_);
    log_problems(result);
    if (result.from_file) {
        log::info("theme: reloaded {}", path_.string());
    } else if (result.problems.empty()) {
        log::info("theme: {} removed, default palette", path_.string());
    } else {
        log::warn("theme: keeping the current palette");
        return;
    }
    palette_ = result.palette;
    if (change_handler_) {
        change_handler_();
    }
}

}  // namespace nordend::theme
