#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "nordend/core/event_loop.hpp"

namespace nordend::core {

// Order in which queued work is picked: what is on screen first.
enum class Priority : std::uint8_t { visible, nearby, prefetch };

// Shared cancellation flag. Cheap to copy; cancel() from any thread. Work should poll
// cancelled() between expensive steps; a cancelled job's result is never delivered.
class CancelToken {
public:
    CancelToken() : flag_{std::make_shared<std::atomic<bool>>(false)} {}

    void cancel() const noexcept { flag_->store(true, std::memory_order_relaxed); }

    [[nodiscard]] bool cancelled() const noexcept { return flag_->load(std::memory_order_relaxed); }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

// Fixed pool of worker threads for blocking work (directory reads, decoding, thumbnails).
//
// submit() queues work with a priority; a worker runs it, and its result is handed to done() on
// the EventLoop thread. done() is not called if the job was cancelled, or if the pool was
// destroyed, before delivery. Queued jobs are dropped on destruction; running ones finish (they
// should check their token) and the destructor joins them.
//
// The value returned by work is moved to done(); it must be copyable (it travels inside a
// std::function), so wrap move-only results in a shared_ptr.
//
// Must not outlive the EventLoop. submit() and the destructor run on the loop thread.
class WorkerPool {
public:
    // threads == 0: half the hardware threads, at least 2.
    explicit WorkerPool(EventLoop& loop, unsigned threads = 0);
    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    WorkerPool(WorkerPool&&) = delete;
    WorkerPool& operator=(WorkerPool&&) = delete;

    template <class Work, class Done>
    CancelToken submit(Priority priority, Work work, Done done) {
        CancelToken token;
        enqueue(priority, token,
                [this, token, alive = alive_, work = std::move(work),
                 done = std::move(done)]() mutable {
                    if (token.cancelled()) {
                        return;
                    }
                    auto result = work(token);
                    loop_.post([token, alive = std::move(alive), done = std::move(done),
                                result = std::move(result)]() mutable {
                        if (alive->load() && !token.cancelled()) {
                            done(std::move(result));
                        }
                    });
                });
        return token;
    }

    [[nodiscard]] std::size_t thread_count() const noexcept { return threads_.size(); }

private:
    struct Job {
        CancelToken token;
        std::function<void()> run;
    };

    void enqueue(Priority priority, CancelToken token, std::function<void()> run);
    void worker_main();

    EventLoop& loop_;
    // Shared with posted deliveries so they can tell the pool (and its owner) are gone.
    std::shared_ptr<std::atomic<bool>> alive_{std::make_shared<std::atomic<bool>>(true)};

    std::mutex mutex_;
    std::condition_variable wake_;
    std::array<std::deque<Job>, 3> queues_;  // indexed by Priority
    bool stopping_{false};
    std::vector<std::thread> threads_;
};

}  // namespace nordend::core
