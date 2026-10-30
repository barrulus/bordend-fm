#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/ui/display.hpp"
#include "nordend/ui/wl_ptr.hpp"

namespace nordend::ui {

// The Wayland clipboard (wl_data_device's selection): offering data in several types, and reading
// what another program offers. Data goes through pipes the loop watches: a slow or stuck reader
// or writer never blocks the window. Writes to a reader that went away fail with EPIPE: the app
// blocks SIGPIPE.
//
// Loop thread only. Must not outlive the Display or the loop. Not copyable or movable: listeners
// keep a pointer to it.
class Clipboard {
public:
    // A read the other program has not finished by then is given up.
    static constexpr std::chrono::milliseconds kReadTimeout{3000};
    static constexpr std::size_t kMaxRead = 64U << 20U;

    using Format = std::pair<std::string, std::string>;  // type, bytes
    using Done = std::function<void(std::optional<std::string> data)>;

    Clipboard(Display& display, core::EventLoop& loop);
    ~Clipboard();

    Clipboard(const Clipboard&) = delete;
    Clipboard& operator=(const Clipboard&) = delete;
    Clipboard(Clipboard&&) = delete;
    Clipboard& operator=(Clipboard&&) = delete;

    // The compositor has a clipboard for this seat (wl_data_device_manager).
    [[nodiscard]] bool available() const noexcept { return device_ != nullptr; }

    // Takes the clipboard with formats. serial: the key or button press that asked (the
    // compositor ignores requests without a recent one).
    void offer(std::vector<Format> formats, std::uint32_t serial);

    // Empties the clipboard, whoever's it was.
    void clear(std::uint32_t serial);

    // Nordend's offer is the one on the clipboard.
    [[nodiscard]] bool owned() const noexcept { return source_ != nullptr; }

    // Changes whenever the clipboard gets other contents (anyone's): a later look can tell
    // whether it still holds what was read.
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

    // The types on the clipboard now (Nordend's own too); empty when it holds nothing.
    [[nodiscard]] std::vector<std::string> types() const;

    // Reads type from what is on the clipboard; done gets nullopt when there is nothing, the
    // program failed, or after kReadTimeout. Never at once: on a later iteration of the loop.
    void read(const std::string& type, Done done);

    // Nordend's offer was replaced: another program copied something (or Nordend did again).
    void set_lost_handler(std::function<void()> lost) { lost_ = std::move(lost); }

private:
    struct Writer {
        int fd{-1};
        std::string data;
        std::size_t written{0};
        core::EventLoop::SourceId source{0};
    };

    struct Reader {
        int fd{-1};
        std::string data;
        Done done;
        core::EventLoop::SourceId source{0};
        core::EventLoop::SourceId timer{0};
    };

    static void on_data_offer(void* data, wl_data_device* device, wl_data_offer* offer);
    static void on_enter(void* data, wl_data_device* device, std::uint32_t serial,
                         wl_surface* surface, wl_fixed_t x, wl_fixed_t y, wl_data_offer* offer);
    static void on_leave(void* data, wl_data_device* device);
    static void on_motion(void* data, wl_data_device* device, std::uint32_t time, wl_fixed_t x,
                          wl_fixed_t y);
    static void on_drop(void* data, wl_data_device* device);
    static void on_selection(void* data, wl_data_device* device, wl_data_offer* offer);
    static void on_offer_type(void* data, wl_data_offer* offer, const char* type);
    static void on_source_send(void* data, wl_data_source* source, const char* type, int fd);
    static void on_source_cancelled(void* data, wl_data_source* source);

    void forget_offer(wl_data_offer* offer);
    void write_some(std::list<Writer>::iterator writer);
    void finish_read(std::list<Reader>::iterator reader, std::optional<std::string> data);

    Display& display_;
    core::EventLoop& loop_;
    WlPtr<wl_data_device> device_;
    WlPtr<wl_data_source> source_;  // Nordend's offer, while it is on the clipboard
    std::vector<Format> formats_;   // what it offers
    // Every offer the compositor introduced and not yet destroyed, with its types.
    std::map<wl_data_offer*, std::vector<std::string>> offers_;
    wl_data_offer* selection_{nullptr};  // the clipboard's, one of offers_
    wl_data_offer* dragged_{nullptr};    // a drag over the window (not taken yet)
    std::uint64_t generation_{0};
    std::list<Writer> writers_;  // lists: callbacks keep iterators
    std::list<Reader> readers_;
    std::function<void()> lost_;
};

}  // namespace nordend::ui
