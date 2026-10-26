#include <array>
#include <csignal>
#include <cstdio>
#include <exception>
#include <poll.h>
#include <stdexcept>
#include <string_view>
#include <sys/signalfd.h>
#include <unistd.h>

#include "nordend/core/log.hpp"
#include "nordend/ui/display.hpp"
#include "nordend/ui/egl_context.hpp"
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

// Temporary loop for ROADMAP 1.1: Wayland events + signals. ROADMAP 2.3 replaces it with the
// general poll loop (worker results, inotify, timers). Returns the process exit code.
int run_event_loop(const ui::Display& display, const ui::Window& window) {
    const SignalFd signals;
    wl_display* wl = display.native();

    while (!window.close_requested()) {
        // Standard libwayland read protocol: dispatch what is queued, flush, then wait.
        while (wl_display_prepare_read(wl) != 0) {
            if (wl_display_dispatch_pending(wl) < 0) {
                log::error("lost the connection to the compositor");
                return 1;
            }
        }
        wl_display_flush(wl);

        std::array fds{pollfd{.fd = display.fd(), .events = POLLIN, .revents = 0},
                       pollfd{.fd = signals.fd(), .events = POLLIN, .revents = 0}};
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
    }
    log::info("window closed");
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
        window.set_draw_handler(&nordend::app::draw_test_scene);
        return run_event_loop(display, window);
    } catch (const std::exception& e) {
        log::error("{}", e.what());
        return 1;
    }
}
