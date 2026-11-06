#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/views/column_view.hpp"
#include "nordend/views/details_view.hpp"
#include "nordend/views/grid_view.hpp"
#include "nordend/views/info_pane.hpp"
#include "nordend/views/thumbnails.hpp"

namespace nordend::views {

// The three ways of showing a folder.
enum class ViewMode : std::uint8_t { columns, grid, details };

// A folder opened, as the history keeps it: where the cursor was when it was left.
struct HistoryStep {
    std::filesystem::path folder;
    std::string cursor;
};

// One place being browsed: the column view, the grid and the details of it, which of them is
// shown, and the folders opened in it (Alt+Left, Alt+Right). A tab is one (ROADMAP 9.1); the
// browser shows one pane at a time and keeps what the window has once (the sidebar, the info
// pane, the status bar, Quick Look, the dialogs, the transfers, undo). The views draw into the
// browser's info pane and take their thumbnails from its cache.
struct Pane {
    Pane(core::EventLoop& loop, core::WorkerPool& pool, fs::DirectoryWatcher& watcher,
         const std::shared_ptr<const fs::MimeDatabase>& mime, const std::function<void()>& redraw,
         const std::function<void(ui::CursorShape)>& set_cursor, Thumbnails* thumbnails,
         InfoPane* info_pane);

    ColumnView columns;  // before grid: grid folders may report to it
    GridView grid;
    DetailsView details;
    ViewMode mode{ViewMode::columns};
    std::vector<HistoryStep> history;
    std::size_t here{0};  // where history is now
};

}  // namespace nordend::views
