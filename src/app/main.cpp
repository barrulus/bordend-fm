#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/signalfd.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include "nordend/core/config.hpp"
#include "nordend/core/event_loop.hpp"
#include "nordend/core/log.hpp"
#include "nordend/core/memory.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/platform/desktop_entry.hpp"
#include "nordend/platform/opener.hpp"
#include "nordend/theme/theme.hpp"
#include "nordend/thumbs/cache.hpp"
#include "nordend/ui/activation.hpp"
#include "nordend/ui/clipboard.hpp"
#include "nordend/ui/display.hpp"
#include "nordend/ui/egl_context.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/ui/window.hpp"
#include "nordend/views/browser.hpp"
#include "test_scene.hpp"

namespace {

namespace core = nordend::core;
namespace fs = nordend::fs;
namespace log = nordend::log;
namespace platform = nordend::platform;
namespace theme = nordend::theme;
namespace thumbs = nordend::thumbs;
namespace ui = nordend::ui;
namespace views = nordend::views;

// SIGINT/SIGTERM delivered as a readable fd, so Ctrl+C ends the loop and runs every
// destructor instead of killing the process mid-flight; SIGUSR1 asks for the memory report.
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
        sigaddset(&mask, SIGUSR1);
        // Writing to a program that stopped reading the clipboard: EPIPE, not death. Started
        // applications get an empty mask (Launcher).
        sigaddset(&mask, SIGPIPE);
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
    core::give_back_large_blocks();  // before any thread: decoded photos go back to the system
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
        // Set once the browser exists: what it holds, for the memory report.
        std::function<std::vector<std::string>()> memory_lines;
        loop.add_fd(signals.fd(), POLLIN, [&](short /*revents*/) {
            signalfd_siginfo info{};
            if (read(signals.fd(), &info, sizeof info) != sizeof info) {
                return;
            }
            if (info.ssi_signo == SIGPIPE) {
                return;  // a clipboard reader went away: the write already failed with EPIPE
            }
            if (info.ssi_signo == SIGUSR1) {
                // A development report (ROADMAP 6.3b): pkill -USR1 nordend.
                for (const std::string& line : core::describe(core::read_memory_report())) {
                    log::info("memory: {}", line);
                }
                for (const std::string& line :
                     memory_lines ? memory_lines() : std::vector<std::string>{}) {
                    log::info("memory: {}", line);
                }
                return;
            }
            log::info("signal received, closing");
            loop.quit(0);
        });

        ui::Display display;
        ui::EglContext egl{display};
        ui::FontSystem fonts{config.font.family, config.font.monospace};
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
        ui::Activation activation{display, loop};
        ui::Clipboard clipboard{display, loop};  // before the browser that uses it
        // The column view and the grid (ROADMAP 4.2, 4.4), unless the renderer test scene was
        // asked for.
        std::unique_ptr<views::Browser> columns;
        // Files open with their default applications (ROADMAP 6.1).
        std::unique_ptr<platform::Opener> opener;
        if (!test_scene) {
            const std::shared_ptr<const fs::MimeDatabase> mime = fs::MimeDatabase::load();
            columns = std::make_unique<views::Browser>(
                loop, pool, watcher, mime, [&] { window.request_redraw(); },
                [&](ui::CursorShape shape) { seat.set_cursor(shape); },
                std::make_shared<thumbs::Cache>(thumbs::default_root(),
                                                "nordend-" NORDEND_VERSION));
            opener = std::make_unique<platform::Opener>(
                loop, pool, mime, platform::Environment::from_environment(),
                [&](std::string message, bool error) {
                    columns->show_message(std::move(message), error);
                },
                [&](std::function<void(std::string)> done) {
                    activation.request(display.seat(), seat.input_serial(), window.surface(),
                                       std::move(done));
                });
            columns->set_open_handler(
                [&](const std::filesystem::path& directory, std::vector<fs::Entry> files) {
                    opener->open(directory, std::move(files));
                });
            columns->set_open_with_handlers(
                [&](const std::filesystem::path& directory, fs::Entry entry,
                    std::function<void(std::vector<views::Browser::AppChoice>)> done) {
                    opener->choices(
                        directory, std::move(entry),
                        [done = std::move(done)](std::vector<platform::AppChoice> found) {
                            std::vector<views::Browser::AppChoice> apps;
                            apps.reserve(found.size());
                            for (platform::AppChoice& app : found) {
                                apps.push_back({.id = std::move(app.id),
                                                .name = std::move(app.name),
                                                .is_default = app.is_default});
                            }
                            done(std::move(apps));
                        });
                },
                [&](const std::filesystem::path& directory, std::vector<fs::Entry> files,
                    std::string app_id) {
                    opener->open(directory, std::move(files), std::move(app_id));
                });
            if (clipboard.available()) {
                columns->set_system_clipboard({
                    .offer =
                        [&](std::vector<views::SystemClipboard::Format> formats) {
                            clipboard.offer(std::move(formats), seat.input_serial());
                        },
                    .clear = [&] { clipboard.clear(seat.input_serial()); },
                    .owned = [&] { return clipboard.owned(); },
                    .generation = [&] { return clipboard.generation(); },
                    .types = [&] { return clipboard.types(); },
                    .read =
                        [&](const std::string& type, views::SystemClipboard::Done done) {
                            clipboard.read(type, std::move(done));
                        },
                });
                clipboard.set_lost_handler([&] { columns->clipboard_lost(); });
                // Drag and drop (ROADMAP 7.1) on the same wl_data_device.
                columns->set_drag_and_drop({
                    .start =
                        [&](std::vector<views::DragAndDrop::Format> formats) {
                            wl_surface* const icon = window.make_drag_icon(
                                views::Browser::kDragIconWidth, views::Browser::kDragIconHeight,
                                [&](ui::Renderer& renderer, const ui::Rect& bounds) {
                                    columns->draw_drag_icon(renderer, bounds, theme.palette(),
                                                            icons);
                                });
                            clipboard.start_drag(std::move(formats), window.surface(), icon,
                                                 seat.input_serial());
                        },
                    .cancel = [&] { clipboard.cancel_drag(); },
                    .types = [&] { return clipboard.drop_types(); },
                    .source_actions = [&] { return clipboard.drop_source_actions(); },
                    .action = [&] { return clipboard.drop_action(); },
                    .accept =
                        [&](const std::optional<std::string>& type, std::uint32_t actions,
                            std::uint32_t preferred) {
                            clipboard.accept_drop(type, actions, preferred);
                        },
                    .read =
                        [&](const std::string& type, views::DragAndDrop::Done done) {
                            clipboard.read_drop(type, std::move(done));
                        },
                    .finish = [&] { clipboard.finish_drop(); },
                    .modifiers = [&] { return seat.modifiers(); },
                });
                clipboard.set_drop_handler(
                    [&](const ui::DropEvent& event) { columns->drop_event(event); });
                clipboard.set_drag_end_handler([&](bool /*dropped*/) {
                    window.drop_drag_icon();
                    columns->drag_ended();
                });
            }
            columns->open(start_folder(start_dir));
            memory_lines = [&columns] { return columns->memory_lines(); };
        }
        // The sidebar's places, kept current as things are mounted and bookmarked (ROADMAP 5.9).
        std::unique_ptr<fs::PlacesWatcher> places;
        if (columns) {
            places =
                std::make_unique<fs::PlacesWatcher>(loop, fs::PlaceSources::from_environment(),
                                                    [&] { columns->set_places(places->places()); });
            columns->set_places(places->places());
            columns->set_places_reload([&places] { places->reload(); });
        }

        std::string last_key = "Press a key (Ctrl+Q quits)";
        seat.set_key_handler([&](const ui::KeyEvent& event) {
            if (event.modifiers.ctrl && (event.keysym == XKB_KEY_q || event.keysym == XKB_KEY_Q)) {
                log::info("quit requested");
                loop.quit(0);
                return;
            }
            if (columns) {
                columns->key(event, views::Browser::Clock::now());
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
            if (columns) {
                columns->pointer(event, views::Browser::Clock::now());
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
            if (columns) {
                columns->draw(renderer, bounds, palette, icons, views::Browser::Clock::now());
                if (columns->animating()) {
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
