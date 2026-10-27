#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/sort.hpp"

namespace nordend::fs {

// What changed in a DirectoryModel since the last notification. Names, not indices: a view
// looks them up in the listing (and keeps its selection across renames).
struct ModelChange {
    bool reset{false};   // the listing was replaced (open, reload after an overflow, error)
    bool loaded{false};  // the initial load just finished
    std::vector<std::string> added;
    std::vector<std::string> removed;
    std::vector<std::string> updated;                          // size, date or type changed
    std::vector<std::pair<std::string, std::string>> renamed;  // old name, new name
};

// One directory as the views see it: a sorted listing, loaded in chunks on workers and kept up
// to date by inotify while open.
//
// The watch starts before the read, so nothing that happens during the load is lost. Events are
// gathered for settle, then the changed names are stat'ed together on a worker and applied in
// one notification: a burst (copying a thousand files) is a few updates, not a thousand, and the
// loop thread does no I/O. Renames inside the directory are reported as renames.
//
// Everything runs on the loop thread. Must not outlive the loop, pool or watcher.
class DirectoryModel {
public:
    using ChangeHandler = std::function<void(const ModelChange& change)>;

    static constexpr std::chrono::milliseconds kDefaultSettle{30};

    DirectoryModel(core::EventLoop& loop, core::WorkerPool& pool, DirectoryWatcher& watcher,
                   std::shared_ptr<const MimeDatabase> mime,
                   std::chrono::milliseconds settle = kDefaultSettle);
    ~DirectoryModel();

    DirectoryModel(const DirectoryModel&) = delete;
    DirectoryModel& operator=(const DirectoryModel&) = delete;
    DirectoryModel(DirectoryModel&&) = delete;
    DirectoryModel& operator=(DirectoryModel&&) = delete;

    // Shows another directory: clears the listing (a reset change), watches and loads it.
    void open(std::filesystem::path path);

    void set_change_handler(ChangeHandler handler) { change_handler_ = std::move(handler); }

    void set_order(SortOrder order);

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    [[nodiscard]] const Listing& listing() const noexcept { return listing_; }

    [[nodiscard]] bool loading() const noexcept { return loading_; }

    // Counts the changes notified: a new value means the listing changed (or was replaced).
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

    // Why the directory cannot be shown (cannot be read, was deleted or moved); empty when fine.
    [[nodiscard]] const std::error_code& error() const noexcept { return error_; }

private:
    void start_load();
    void stop();
    void on_events(const std::vector<DirectoryEvent>& events);
    void schedule_refresh();
    void refresh();
    void apply(const std::vector<std::string>& names, std::vector<std::optional<Entry>> entries,
               const std::vector<std::pair<std::string, std::string>>& renames);
    void notify(const ModelChange& change);
    void fail(std::error_code error);

    core::EventLoop& loop_;
    core::WorkerPool& pool_;
    DirectoryWatcher& watcher_;
    ReadOptions options_;
    std::chrono::milliseconds settle_;
    ChangeHandler change_handler_;

    std::filesystem::path path_;
    Listing listing_;
    std::unordered_set<std::string> names_;  // what is in the listing
    std::error_code error_;
    bool loading_{false};
    std::uint64_t revision_{0};

    DirectoryWatcher::WatchId watch_{0};
    core::CancelToken load_;
    core::CancelToken refresh_job_;
    bool refresh_running_{false};
    core::EventLoop::SourceId settle_timer_{0};

    // Changes seen and not applied yet.
    std::unordered_set<std::string> dirty_;
    std::unordered_map<std::uint32_t, std::string> moved_from_;  // cookie -> old name
    std::vector<std::pair<std::string, std::string>> renames_;
};

}  // namespace nordend::fs
