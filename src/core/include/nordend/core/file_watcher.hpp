#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include "nordend/core/event_loop.hpp"

namespace nordend::core {

// Calls a handler on the loop thread when one file changes: written and closed, replaced by a
// rename, or removed. It watches the parent directory through inotify, so the file may not exist
// yet and atomic replacements are seen; the directory is created when missing (unless told not
// to: another program's folder, such as GTK's, is never made by Nordend). Events are
// coalesced: the handler runs once, settle after the last one, so a writer that saves in several
// steps is read only when it is done.
//
// Must not outlive the loop. Not copyable or movable: the loop callbacks keep a pointer to it.
class FileWatcher {
public:
    using Handler = std::function<void()>;

    static constexpr std::chrono::milliseconds kDefaultSettle{50};

    enum class Directory : std::uint8_t {
        create,    // made when missing
        existing,  // must exist: std::system_error otherwise
    };

    FileWatcher(EventLoop& loop, const std::filesystem::path& file, Handler handler,
                std::chrono::milliseconds settle = kDefaultSettle,
                Directory directory = Directory::create);
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&) = delete;
    FileWatcher& operator=(FileWatcher&&) = delete;

private:
    void on_readable();

    EventLoop& loop_;
    std::string name_;  // file name inside the watched directory
    Handler handler_;
    std::chrono::milliseconds settle_;
    int fd_{-1};
    EventLoop::SourceId source_{0};
    EventLoop::SourceId timer_{0};  // 0 = no change pending
};

}  // namespace nordend::core
