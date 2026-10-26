#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_model.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/entry.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"

namespace nordend::app {

// Check of the file-type icons (ROADMAP 3.4): a directory's entries in a grid, icon and name,
// as many as fit. Not a view (no scrolling, no selection): those come in phase 4.
void draw_icon_grid(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
                    ui::IconSet& icons, const fs::MimeDatabase& mime, const std::string& title,
                    const std::vector<fs::Entry>& entries);

// nordend --icons DIR: a live DirectoryModel shown with draw_icon_grid. Entries whose type the
// name does not decide are sniffed on a worker, then redrawn: what a view will do for its
// visible rows.
class IconDemo {
public:
    IconDemo(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
             std::shared_ptr<const fs::MimeDatabase> mime, std::function<void()> redraw);

    void open(const std::filesystem::path& directory) { model_.open(directory); }

    void draw(ui::Renderer& renderer, const ui::Rect& bounds, const theme::Palette& palette,
              ui::IconSet& icons) const;

private:
    void rebuild();
    void sniff();

    core::WorkerPool& pool_;
    std::shared_ptr<const fs::MimeDatabase> mime_;
    std::function<void()> redraw_;
    fs::DirectoryModel model_;
    std::unordered_map<std::string, fs::MimeId> sniffed_;  // by name, for the open directory
    std::vector<fs::Entry> shown_;                         // listing with sniffed types
    core::CancelToken sniff_job_;
    bool sniffing_{false};
};

}  // namespace nordend::app
