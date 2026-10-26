#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/signalfd.h>
#include <system_error>
#include <unistd.h>
#include <utility>

#include "nordend/core/config.hpp"
#include "nordend/core/event_loop.hpp"
#include "nordend/core/log.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/theme/theme.hpp"
#include "nordend/ui/display.hpp"
#include "nordend/ui/egl_context.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/ui/window.hpp"
#include "nordend/views/list_view.hpp"
#include "test_scene.hpp"

namespace {

namespace core = nordend::core;
namespace fs = nordend::fs;
namespace log = nordend::log;
namespace theme = nordend::theme;
namespace ui = nordend::ui;
namespace views = nordend::views;

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

const char* button_name(ui::PointerButton button) {
    switch (button) {
        case ui::PointerButton::left:
            return "left";
        case ui::PointerButton::right:
            return "right";
        case ui::PointerButton::middle:
            return "middle";
        case ui::PointerButton::back:
            return "back";
        case ui::PointerButton::forward:
            return "forward";
        case ui::PointerButton::other:
            break;
    }
    return "other";
}

constexpr const char* kUsage =
    "usage: nordend [--config FILE] [--check-config] [--test-scene] [--version] [DIR]\n"
    "  DIR             the folder to show (default: your home folder)\n"
    "  --config FILE   read FILE instead of ~/.config/nordend/config.toml\n"
    "  --test-scene    show the renderer test scene instead of a folder\n"
    "  --check-config  report problems in the config file and exit (status 1 if any)\n"
    "  --version       print the version and exit\n";

// The folder to open: DIR as given (made absolute, ".." resolved), else $HOME, else "/".
std::filesystem::path start_folder(const std::filesystem::path& given) {
    std::filesystem::path folder = given;
    if (folder.empty()) {
        const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe): startup
        folder = home != nullptr && home[0] != '\0' ? home : "/";
    }
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::weakly_canonical(folder, error);
    return error ? folder : absolute;
}

// --check-config: every problem on stderr, a one-line verdict on stdout.
int check_config(const std::filesystem::path& path, const core::ConfigResult& result) {
    for (const std::string& problem : result.problems) {
        std::fprintf(stderr, "%s\n", problem.c_str());
    }
    if (!result.problems.empty()) {
        std::printf("%s: %zu problem(s)\n", path.c_str(), result.problems.size());
        return 1;
    }
    std::printf("%s: %s\n", path.c_str(), result.from_file ? "ok" : "not found, defaults in use");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path config_path;
    std::filesystem::path start_dir;
    bool check_only = false;
    bool test_scene = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--version") {
            std::puts("nordend " NORDEND_VERSION);
            return 0;
        }
        if (arg == "--check-config") {
            check_only = true;
        } else if (arg == "--test-scene") {
            test_scene = true;
        } else if (arg == "--config") {
            if (i + 1 == argc) {
                std::fprintf(stderr, "nordend: --config needs a file\n%s", kUsage);
                return 2;
            }
            config_path = argv[++i];
        } else if (!arg.starts_with("-") && start_dir.empty()) {
            start_dir = arg;
        } else {
            std::fprintf(stderr, "nordend: unknown option '%s'\n%s", argv[i], kUsage);
            return 2;
        }
    }
    if (config_path.empty()) {
        config_path = core::default_config_path();
    }
    const core::ConfigResult config_result = core::load_config(config_path);
    if (check_only) {
        return check_config(config_path, config_result);
    }

    log::init_from_env();
    log::info("nordend {}", NORDEND_VERSION);
    if (config_result.from_file) {
        log::info("config: {}", config_path.string());
    } else {
        log::info("config: {} not used, defaults in use", config_path.string());
    }
    for (const std::string& problem : config_result.problems) {
        log::warn("config: {}", problem);
    }
    const core::Config& config = config_result.config;

    try {
        // Before any other thread exists, so every thread inherits the blocked signals.
        const SignalFd signals;
        core::EventLoop loop;
        loop.add_fd(signals.fd(), POLLIN, [&](short /*revents*/) {
            log::info("signal received, closing");
            loop.quit(0);
        });

        ui::Display display;
        ui::EglContext egl{display};
        ui::FontSystem fonts{config.font.family};
        theme::Theme theme{theme::default_colors_path()};
        theme.watch(loop);
        ui::Window window{
            display,
            egl,
            fonts,
            {.title = "Nordend", .app_id = "nordend", .background = theme.palette().surface}};
        theme.set_change_handler([&] { window.set_background(theme.palette().surface); });
        ui::Seat seat{display};
        seat.set_multi_click(std::chrono::milliseconds{config.pointer.double_click_ms},
                             static_cast<float>(config.pointer.double_click_distance));
        display.attach(loop);
        seat.attach(loop);
        window.set_close_handler([&] {
            log::info("window closed");
            loop.quit(0);
        });

        core::WorkerPool pool{loop};
        fs::DirectoryWatcher watcher{loop};
        ui::IconSet icons{fonts};
        // The folder list (ROADMAP 4.1), unless the renderer test scene was asked for.
        std::unique_ptr<views::ListView> list;
        if (!test_scene) {
            list = std::make_unique<views::ListView>(loop, pool, watcher, fs::MimeDatabase::load(),
                                                     [&] { window.request_redraw(); });
            list->open(start_folder(start_dir));
        }

        std::string last_key = "Press a key (Ctrl+Q quits)";
        seat.set_key_handler([&](const ui::KeyEvent& event) {
            if (event.modifiers.ctrl && (event.keysym == XKB_KEY_q || event.keysym == XKB_KEY_Q)) {
                log::info("quit requested");
                loop.quit(0);
                return;
            }
            if (list) {
                list->key(event, views::ListView::Clock::now());
                return;
            }
            last_key = describe(event);
            log::debug("key: {}", last_key);
            window.request_redraw();
        });
        // Pointer test (ROADMAP 2.2): a hover/click target and the last pointer event.
        const ui::Rect target{760.0F, 32.0F, 160.0F, 48.0F};
        bool hovered = false;
        int clicks = 0;
        std::string last_pointer = "Move the pointer, click the button, scroll";
        seat.set_pointer_handler([&](const ui::PointerEvent& event) {
            if (list) {
                list->pointer(event, views::ListView::Clock::now());
                return;
            }
            using Kind = ui::PointerEvent::Kind;
            const bool inside = event.kind != Kind::leave && event.x >= target.x &&
                                event.x < target.right() && event.y >= target.y &&
                                event.y < target.bottom();
            if (inside != hovered) {
                hovered = inside;
                seat.set_cursor(inside ? ui::CursorShape::pointer : ui::CursorShape::standard);
                window.request_redraw();
            }
            switch (event.kind) {
                case Kind::press:
                    if (inside && event.button == ui::PointerButton::left) {
                        ++clicks;
                    }
                    last_pointer =
                        std::format("press {} \u00d7{} at {:.1f}, {:.1f}",
                                    button_name(event.button), event.click_count, event.x, event.y);
                    break;
                case Kind::scroll:
                    last_pointer =
                        std::format("scroll {:.2f}, {:.2f} px   wheel {:.2f}, {:.2f}{}{}",
                                    event.scroll_x, event.scroll_y, event.wheel_x, event.wheel_y,
                                    event.scroll_finger ? "   (touchpad)" : "",
                                    event.scroll_stop ? "   stop" : "");
                    break;
                case Kind::leave:
                    last_pointer = "pointer left the window";
                    break;
                default:
                    return;  // motion and enter only matter for hover
            }
            log::debug("pointer: {}", last_pointer);
            window.request_redraw();
        });

        window.set_draw_handler([&](ui::Renderer& renderer, const ui::Rect& bounds) {
            const theme::Palette& palette = theme.palette();
            if (list) {
                list->draw(renderer, bounds, palette, icons, views::ListView::Clock::now());
                if (list->animating()) {
                    window.request_redraw();  // next frame, paced by the compositor
                }
                return;
            }
            nordend::app::draw_test_scene(renderer, bounds, palette);
            renderer.draw_box(target,
                              {.fill = hovered ? palette.primary : palette.surface_container_high,
                               .radius = ui::Corners::all(10)});
            renderer.draw_text(std::format("Clicks: {}", clicks), target.x + 16.0F,
                               target.y + 14.0F, 15.0F,
                               hovered ? palette.on_primary : palette.on_surface);
            renderer.draw_text(last_pointer, 32.0F, bounds.bottom() - 56.0F, 14.0F,
                               palette.secondary);
            renderer.draw_text(last_key, 32.0F, bounds.bottom() - 32.0F, 14.0F, palette.tertiary);
        });
        return loop.run();
    } catch (const std::exception& e) {
        log::error("{}", e.what());
        return 1;
    }
}
