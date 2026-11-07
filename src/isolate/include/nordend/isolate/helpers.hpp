#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

#include "nordend/image/load.hpp"

// Nordend's preview helpers (ROADMAP 2b): the parsers of files that come from outside run in
// separate nordend-preview processes, started when a picture is first asked for, at most one
// per worker, each closed (and waited for, no zombie) after a while without work. A helper that
// stops, or takes too long, is killed: the picture is "unavailable" and Nordend carries on. Nothing
// is ever parsed here instead.
namespace nordend::isolate {

struct HelperOptions {
    std::filesystem::path program;            // nordend-preview
    std::vector<std::string> args;            // extra arguments (tests)
    std::size_t max_helpers{8};               // one for each worker: no picture waits for a helper
    std::chrono::milliseconds timeout{3000};  // for one picture
    std::chrono::milliseconds idle{30'000};   // a helper without work is closed after this
};

class Helpers {
public:
    explicit Helpers(HelperOptions options);
    ~Helpers();
    Helpers(const Helpers&) = delete;
    Helpers& operator=(const Helpers&) = delete;
    Helpers(Helpers&&) = delete;
    Helpers& operator=(Helpers&&) = delete;

    // The picture of request.path, made by a helper. Blocks while every helper is busy.
    // Thread-safe: called from the workers.
    [[nodiscard]] image::Loaded load(const image::LoadRequest& request);

    // Helpers started and not yet known to be gone (tests).
    [[nodiscard]] std::size_t running() const;

private:
    struct Helper {
        pid_t pid{-1};
        int socket{-1};
        std::chrono::steady_clock::time_point idle_since;
    };
    enum class Outcome : std::uint8_t { done, again, failed };

    [[nodiscard]] std::optional<Helper> take();
    void give_back(Helper helper);
    int discard(Helper helper);  // its wait status
    void close_idle();           // the reaper thread: closes and waits for helpers idle too long
    [[nodiscard]] std::optional<Helper> spawn() const;
    Outcome ask(Helper helper, int file, const image::LoadRequest& request,
                image::Loaded& out) const;

    HelperOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable freed_;
    std::vector<Helper> idle_;
    std::size_t count_{0};  // idle and busy
    std::condition_variable idle_changed_;
    bool stopping_{false};
    std::thread reaper_;  // declared last: started once the rest is ready
};

// Where nordend-preview is: next to the running program (the build tree), else in
// ../libexec/nordend/ (installed). Empty when neither exists.
[[nodiscard]] std::filesystem::path default_program();

}  // namespace nordend::isolate
