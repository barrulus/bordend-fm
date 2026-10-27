#include "nordend/fs/directory_model.hpp"

#include <algorithm>
#include <utility>

#include "nordend/core/log.hpp"

namespace nordend::fs {
namespace {

ModelChange reset_change() {
    ModelChange change;
    change.reset = true;
    return change;
}

ModelChange loaded_change() {
    ModelChange change;
    change.loaded = true;
    return change;
}

}  // namespace

DirectoryModel::DirectoryModel(core::EventLoop& loop, core::WorkerPool& pool,
                               DirectoryWatcher& watcher, std::shared_ptr<const MimeDatabase> mime,
                               std::chrono::milliseconds settle)
    : loop_{loop}, pool_{pool}, watcher_{watcher}, settle_{settle} {
    options_.mime = std::move(mime);
}

DirectoryModel::~DirectoryModel() { stop(); }

void DirectoryModel::open(std::filesystem::path path) {
    stop();
    path_ = std::move(path);
    listing_.clear();
    names_.clear();
    error_.clear();
    notify(reset_change());

    std::error_code error;
    watch_ = watcher_.watch(
        path_, [this](const std::vector<DirectoryEvent>& events) { on_events(events); }, error);
    if (error == std::errc::no_space_on_device) {
        // The per-user inotify watch limit: show the directory anyway, without live updates.
        log::warn("{}: cannot watch for changes (inotify watch limit reached)", path_.string());
    } else if (error) {
        fail(error);
        return;
    }
    start_load();
}

void DirectoryModel::set_order(SortOrder order) {
    listing_.set_order(order);
    notify(reset_change());
}

void DirectoryModel::start_load() {
    loading_ = true;
    load_ = load_directory(
        pool_, path_,
        [this](std::vector<Entry> chunk) {
            ModelChange change;
            change.added.reserve(chunk.size());
            for (const Entry& entry : chunk) {
                names_.insert(entry.name);
                change.added.push_back(entry.name);
            }
            listing_.add(std::move(chunk));
            notify(change);
        },
        [this](const ReadResult& result) {
            loading_ = false;
            if (result.error) {
                fail(result.error);
                return;
            }
            notify(loaded_change());
            if (!dirty_.empty()) {
                schedule_refresh();  // changes that happened while loading
            }
        },
        options_);
}

void DirectoryModel::stop() {
    load_.cancel();
    refresh_job_.cancel();
    refresh_running_ = false;
    loop_.remove(settle_timer_);
    settle_timer_ = 0;
    watcher_.unwatch(watch_);
    watch_ = 0;
    loading_ = false;
    dirty_.clear();
    moved_from_.clear();
    renames_.clear();
}

void DirectoryModel::on_events(const std::vector<DirectoryEvent>& events) {
    using Kind = DirectoryEvent::Kind;
    for (const DirectoryEvent& event : events) {
        switch (event.kind) {
            case Kind::directory_gone:
                fail(std::make_error_code(std::errc::no_such_file_or_directory));
                return;
            case Kind::overflow:
                // Events were lost: read the whole directory again.
                load_.cancel();
                refresh_job_.cancel();
                refresh_running_ = false;
                loop_.remove(settle_timer_);
                settle_timer_ = 0;
                dirty_.clear();
                moved_from_.clear();
                renames_.clear();
                listing_.clear();
                names_.clear();
                notify(reset_change());
                start_load();
                return;
            case Kind::moved_from:
                moved_from_[event.cookie] = event.name;
                dirty_.insert(event.name);
                break;
            case Kind::moved_to:
                if (const auto it = moved_from_.find(event.cookie); it != moved_from_.end()) {
                    renames_.emplace_back(it->second, event.name);
                    moved_from_.erase(it);
                }
                dirty_.insert(event.name);
                break;
            case Kind::created:
            case Kind::deleted:
            case Kind::changed:
                dirty_.insert(event.name);
                break;
        }
    }
    if (!loading_) {
        schedule_refresh();
    }
}

void DirectoryModel::schedule_refresh() {
    if (settle_timer_ != 0 || refresh_running_) {
        return;  // already coming; a running refresh reschedules when it lands
    }
    settle_timer_ = loop_.add_timer(settle_, std::chrono::milliseconds{0}, [this] {
        settle_timer_ = 0;
        refresh();
    });
}

void DirectoryModel::refresh() {
    if (dirty_.empty()) {
        return;
    }
    std::vector<std::string> names(dirty_.begin(), dirty_.end());
    std::vector<std::pair<std::string, std::string>> renames = std::move(renames_);
    dirty_.clear();
    renames_.clear();
    moved_from_.clear();  // a move away whose pair never came: just a removal
    refresh_running_ = true;
    refresh_job_ = pool_.submit(
        core::Priority::visible,
        [path = path_, names, options = options_](const core::CancelToken& /*token*/) {
            return read_entries(path, names, options);
        },
        [this, names, renames = std::move(renames)](std::vector<std::optional<Entry>> entries) {
            refresh_running_ = false;
            apply(names, std::move(entries), renames);
            if (!dirty_.empty()) {
                schedule_refresh();
            }
        });
}

void DirectoryModel::apply(const std::vector<std::string>& names,
                           std::vector<std::optional<Entry>> entries,
                           const std::vector<std::pair<std::string, std::string>>& renames) {
    ModelChange change;
    std::unordered_set<std::string> gone;
    std::vector<Entry> present;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::string& name = names[i];
        if (std::optional<Entry>& entry = entries[i]; entry.has_value()) {
            (names_.contains(name) ? change.updated : change.added).push_back(name);
            names_.insert(name);
            present.push_back(std::move(entry.value()));
        } else if (names_.erase(name) > 0) {
            change.removed.push_back(name);
            gone.insert(name);
        }
    }
    // A rename is a removal of the old name and an addition of the new one in the same batch.
    for (const auto& [from, to] : renames) {
        const auto removed = std::ranges::find(change.removed, from);
        const auto added = std::ranges::find(change.added, to);
        if (removed != change.removed.end() && added != change.added.end()) {
            change.removed.erase(removed);
            change.added.erase(added);
            change.renamed.emplace_back(from, to);
        }
    }
    listing_.replace(gone, std::move(present));
    if (!change.added.empty() || !change.removed.empty() || !change.updated.empty() ||
        !change.renamed.empty()) {
        notify(change);
    }
}

void DirectoryModel::notify(const ModelChange& change) {
    ++revision_;
    if (change_handler_) {
        change_handler_(change);
    }
}

void DirectoryModel::fail(std::error_code error) {
    stop();
    error_ = error;
    listing_.clear();
    names_.clear();
    notify(reset_change());
}

}  // namespace nordend::fs
