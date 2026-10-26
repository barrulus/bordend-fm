#include "icon_demo.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string_view>
#include <utility>

#include "nordend/fs/icon.hpp"

namespace nordend::app {
namespace {

constexpr float kMargin = 24.0F;
constexpr float kHeaderHeight = 56.0F;
constexpr float kCellWidth = 300.0F;
constexpr float kRowHeight = 30.0F;
constexpr float kIconSize = 20.0F;
constexpr float kTextSize = 14.0F;

}  // namespace

void draw_icon_grid(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    ui::IconSet& icons, const fs::MimeDatabase& mime, const std::string& title,
                    const std::vector<fs::Entry>& entries) {
    renderer.push_clip(bounds);
    renderer.draw_text(title, kMargin, kMargin - 6.0F, 18.0F, palette.on_surface,
                       bounds.width - (2.0F * kMargin), ui::Ellipsis::middle);
    const std::string note =
        icons.available()
            ? std::format("{} entries   icons: {}", entries.size(), icons.path().string())
            : std::format("{} entries   no icon font found", entries.size());
    renderer.draw_text(note, kMargin, kMargin + 20.0F, 12.0F, palette.on_surface_variant,
                       bounds.width - (2.0F * kMargin), ui::Ellipsis::middle);

    const float top = kHeaderHeight;
    const auto columns = std::max<std::size_t>(
        1, static_cast<std::size_t>((bounds.width - (2.0F * kMargin)) / kCellWidth));
    const auto rows = std::max<std::size_t>(
        1, static_cast<std::size_t>((bounds.height - top - kMargin) / kRowHeight));
    const std::size_t shown = std::min(entries.size(), columns * rows);
    for (std::size_t i = 0; i < shown; ++i) {
        const fs::Entry& entry = entries[i];
        // Column-major, like a file manager's compact list.
        const std::size_t column = i / rows;
        const std::size_t row = i % rows;
        const float x = kMargin + (static_cast<float>(column) * kCellWidth);
        const float y = top + (static_cast<float>(row) * kRowHeight);
        const std::string_view icon = fs::icon_name(mime, entry);
        const ui::Color tint = entry.is_directory() ? palette.primary : palette.on_surface_variant;
        icons.draw(renderer, icon, x, y + 4.0F, kIconSize, tint);
        renderer.draw_text(entry.name, x + kIconSize + 10.0F, y + 5.0F, kTextSize,
                           entry.hidden() ? palette.outline : palette.on_surface,
                           kCellWidth - kIconSize - 24.0F, ui::Ellipsis::middle);
    }
    if (shown < entries.size()) {
        renderer.draw_text(std::format("… and {} more", entries.size() - shown), kMargin,
                           bounds.bottom() - kMargin, 12.0F, palette.on_surface_variant);
    }
    renderer.pop_clip();
}

IconDemo::IconDemo(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
                   std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw)
    : pool_{pool},
      mime_{std::move(mime)},
      redraw_{std::move(redraw)},
      model_{loop, pool, watcher, mime_} {
    model_.set_change_handler([this](const fs::ModelChange& change) {
        if (change.reset) {
            sniff_job_.cancel();
            sniffing_ = false;
            sniffed_.clear();
        }
        for (const std::string& name : change.removed) {
            sniffed_.erase(name);
        }
        for (const std::string& name : change.updated) {
            sniffed_.erase(name);  // the content may have changed
        }
        for (const auto& [from, to] : change.renamed) {
            sniffed_.erase(from);
        }
        rebuild();
        if (!model_.loading()) {
            sniff();
        }
    });
}

void IconDemo::rebuild() {
    shown_ = model_.listing().entries();
    for (fs::Entry& entry : shown_) {
        if (entry.mime == fs::kUnknownMime) {
            if (const auto it = sniffed_.find(entry.name); it != sniffed_.end()) {
                entry.mime = it->second;
            }
        }
    }
    redraw_();
}

void IconDemo::sniff() {
    if (sniffing_) {
        return;
    }
    std::vector<fs::Entry> pending;
    for (const fs::Entry& entry : shown_) {
        if (entry.mime == fs::kUnknownMime) {
            pending.push_back(entry);
        }
    }
    if (pending.empty()) {
        return;
    }
    sniffing_ = true;
    sniff_job_ = pool_.submit(
        core::Priority::visible,
        [directory = model_.path(), mime = mime_,
         pending = std::move(pending)](const core::CancelToken& token) {
            std::vector<std::pair<std::string, fs::MimeId>> types;
            for (const fs::Entry& entry : pending) {
                if (token.cancelled()) {
                    break;
                }
                types.emplace_back(entry.name, mime->detect(directory, entry));
            }
            return types;
        },
        [this](std::vector<std::pair<std::string, fs::MimeId>> types) {
            sniffing_ = false;
            for (auto& [name, type] : types) {
                sniffed_[std::move(name)] = type;
            }
            rebuild();
            sniff();  // entries that arrived meanwhile
        });
}

void IconDemo::draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    ui::IconSet& icons) const {
    std::string title = model_.path().string();
    if (model_.error()) {
        title += "   (" + model_.error().message() + ")";
    } else if (model_.loading()) {
        title += "   (loading)";
    }
    draw_icon_grid(renderer, bounds, palette, icons, *mime_, title, shown_);
}

}  // namespace nordend::app
