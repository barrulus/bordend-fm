#include <array>
#include <csignal>
#include <cstdio>
#include <exception>
#include <format>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/signalfd.h>
#include <unistd.h>
#include <utility>

#include "nordend/core/log.hpp"
#include "nordend/ui/display.hpp"
#include "nordend/ui/egl_context.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/ui/window.hpp"
#include "test_scene.hpp"

namespace {

namespace log = nordend::log;
namespace ui = nordend::ui;

// SIGINT/SIGTERM delivered as a readable fd, so Ctrl+C ends the loop and runs every
// destructor instead of killing the process mid-flight.
class SignalFd {
public:
    SignalFd() : mask_{block_signals()}, fd_{signalfd(-1, &mask_, SFD_CLOEXEC | SFD_NONBLOCK)} {
        if (fd_ < 0) {
            throw std::runtime_error{"signalfd failed"};
        }
    }

    ~SignalFd() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    SignalFd(const SignalFd&) = delete;
    SignalFd& operator=(const SignalFd&) = delete;
    SignalFd(SignalFd&&) = delete;
    SignalFd& operator=(SignalFd&&) = delete;

    [[nodiscard]] int fd() const noexcept { return fd_; }

private:
    static sigset_t block_signals() {
        sigset_t mask{};
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        sigprocmask(SIG_BLOCK, &mask, nullptr);
        return mask;
    }

    sigset_t mask_;
    int fd_;
};

// "Ctrl+Shift+a  text 'A'  (repeat)": what the test scene shows for the last key.
std::string describe(const ui::KeyEvent& event) {
    std::string out;
    const ui::Modifiers& mods = event.modifiers;
    for (const auto& [active, name] :
         {std::pair{mods.ctrl, "Ctrl+"}, std::pair{mods.alt, "Alt+"},
          std::pair{mods.shift, "Shift+"}, std::pair{mods.super, "Super+"}}) {
        if (active) {
            out += name;
        }
    }
    std::array<char, 64> name{};
    xkb_keysym_get_name(event.keysym, name.data(), name.size());
    out += name.data();
    if (!event.text.empty()) {
        out += std::format("   text \u201c{}\u201d", event.text);
    }
    if (event.repeat) {
        out += "   (repeat)";
    }
    return out;
}

// Temporary loop: Wayland events, key repeat and signals. ROADMAP 2.3 replaces it with the
// general poll loop (worker results, inotify, timers). Returns the process exit code.
int run_event_loop(const ui::Display& display, ui::Seat& seat, const ui::Window& window,
                   const bool& quit) {
    const SignalFd signals;
    wl_display* wl = display.native();

    while (!window.close_requested() && !quit) {
        // Standard libwayland read protocol: dispatch what is queued, flush, then wait.
        while (wl_display_prepare_read(wl) != 0) {
            if (wl_display_dispatch_pending(wl) < 0) {
                log::error("lost the connection to the compositor");
                return 1;
            }
        }
        wl_display_flush(wl);

        std::array fds{pollfd{.fd = display.fd(), .events = POLLIN, .revents = 0},
                       pollfd{.fd = signals.fd(), .events = POLLIN, .revents = 0},
                       pollfd{.fd = seat.repeat_fd(), .events = POLLIN, .revents = 0}};
        if (poll(fds.data(), fds.size(), -1) < 0) {
            wl_display_cancel_read(wl);
            continue;  // EINTR
        }

        if ((fds[0].revents & POLLIN) != 0) {
            wl_display_read_events(wl);
        } else {
            wl_display_cancel_read(wl);
        }
        if (wl_display_dispatch_pending(wl) < 0) {
            log::error("lost the connection to the compositor");
            return 1;
        }

        if ((fds[1].revents & POLLIN) != 0) {
            log::info("signal received, closing");
            return 0;
        }
        if ((fds[2].revents & POLLIN) != 0) {
            seat.dispatch_repeat();
        }
    }
    log::info("{}", quit ? "quit requested" : "window closed");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--version") {
            std::puts("nordend " NORDEND_VERSION);
            return 0;
        }
    }

    log::init_from_env();
    log::info("nordend {}", NORDEND_VERSION);

    try {
        ui::Display display;
        ui::EglContext egl{display};
        ui::FontSystem fonts;
        ui::Window window{display, egl, fonts, {.title = "Nordend", .app_id = "nordend"}};
        ui::Seat seat{display};

        bool quit = false;
        std::string last_key = "Press a key (Ctrl+Q quits)";
        seat.set_key_handler([&](const ui::KeyEvent& event) {
            if (event.modifiers.ctrl && (event.keysym == XKB_KEY_q || event.keysym == XKB_KEY_Q)) {
                quit = true;
                return;
            }
            last_key = describe(event);
            log::debug("key: {}", last_key);
            window.request_redraw();
        });
        window.set_draw_handler([&](ui::Renderer& renderer, const ui::Rect& bounds) {
            nordend::app::draw_test_scene(renderer, bounds);
            renderer.draw_text(last_key, 32.0F, bounds.bottom() - 32.0F, 14.0F,
                               ui::Color::rgb(0xF9E2AF));
        });
        return run_event_loop(display, seat, window, quit);
    } catch (const std::exception& e) {
        log::error("{}", e.what());
        return 1;
    }
}
