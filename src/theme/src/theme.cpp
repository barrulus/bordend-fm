#include "nordend/theme/theme.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <system_error>
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

Theme::Theme(std::filesystem::path path, std::vector<std::filesystem::path> gtk_css)
    : path_{std::move(path)}, gtk_css_{std::move(gtk_css)}, gtk_{load_gtk_colors(gtk_css_)} {
    const PaletteResult result = load_palette(path_);
    log_problems(result);
    from_file_ = result.from_file;
    palette_ = from_file_ ? result.palette : fallback();
    if (from_file_) {
        log::info("theme: {}", path_.string());
    } else if (!gtk_.source.empty()) {
        log::info("theme: GTK colours from {} ({} named)", gtk_.source.string(),
                  gtk_.colors.size());
    } else {
        log::info("theme: default palette");
    }
}

void Theme::set_desktop(std::optional<Desktop> desktop) {
    if (desktop == desktop_) {
        return;
    }
    desktop_ = desktop;
    if (desktop_) {
        log::info("theme: desktop is {}{}", desktop_->dark ? "dark" : "light",
                  desktop_->accent ? ", with an accent colour" : "");
    }
    if (from_file_) {
        return;  // colors.toml wins
    }
    palette_ = fallback();
    changed();
}

std::optional<Color> Theme::accent() const {
    if (from_file_) {
        return palette_.primary.with_alpha(1.0F);
    }
    if (from_gtk()) {
        if (const std::optional<Color> accent = gtk_accent(gtk_)) {
            return accent;
        }
    }
    return desktop_ ? desktop_->accent : std::nullopt;
}

std::optional<Palette> Theme::from_gtk() const {
    return gtk_palette(gtk_, desktop_ ? desktop_->dark : true);
}

Palette Theme::fallback() const {
    if (std::optional<Palette> palette = from_gtk()) {
        return *palette;
    }
    return desktop_ ? desktop_palette(*desktop_) : Palette{};
}

void Theme::changed() {
    if (change_handler_) {
        change_handler_();
    }
}

void Theme::watch(core::EventLoop& loop) {
    loop_ = &loop;
    if (!path_.empty()) {
        watcher_ = std::make_unique<core::FileWatcher>(loop, path_, [this] { reload(); });
    }
    watch_gtk();
}

void Theme::watch_gtk() {
    if (loop_ == nullptr) {
        return;
    }
    std::vector<std::filesystem::path> files = gtk_css_;
    for (const std::filesystem::path& file : gtk_.files) {
        if (std::ranges::find(files, file) == files.end()) {
            files.push_back(file);
        }
    }
    gtk_watchers_.clear();
    for (const std::filesystem::path& file : files) {
        try {
            // GTK's folders are never made by Nordend: one that does not exist is not followed.
            gtk_watchers_.push_back(std::make_unique<core::FileWatcher>(
                *loop_, file, [this] { reload_gtk(); }, core::FileWatcher::kDefaultSettle,
                core::FileWatcher::Directory::existing));
        } catch (const std::system_error&) {
            log::debug("theme: {} not followed (no such folder)", file.parent_path().string());
        }
    }
}

void Theme::reload_gtk() {
    GtkColors gtk = load_gtk_colors(gtk_css_);
    if (gtk == gtk_) {
        return;
    }
    const bool files_changed = gtk.files != gtk_.files;
    gtk_ = std::move(gtk);
    log::info("theme: GTK colours {}", gtk_.source.empty() ? "gone" : "reloaded");
    if (files_changed && loop_ != nullptr) {
        // From inside a watcher's own handler: the old watchers go once this one returns.
        loop_->post([this] { watch_gtk(); });
    }
    if (from_file_) {
        return;  // colors.toml wins
    }
    palette_ = fallback();
    changed();
}

void Theme::reload() {
    const PaletteResult result = load_palette(path_);
    log_problems(result);
    if (result.from_file) {
        log::info("theme: reloaded {}", path_.string());
    } else if (result.problems.empty()) {
        std::string_view next = "default";
        if (from_gtk()) {
            next = "GTK's";
        } else if (desktop_) {
            next = "the desktop's";
        }
        log::info("theme: {} removed, {} palette", path_.string(), next);
    } else {
        log::warn("theme: keeping the current palette");
        return;
    }
    from_file_ = result.from_file;
    palette_ = from_file_ ? result.palette : fallback();
    changed();
}

}  // namespace nordend::theme
