// nordend-snapshot: renders the test scene (or the column view, or the grid) offscreen and writes
// it as a binary PPM.
//
//   nordend-snapshot [--columns DIR [--reveal PATH] [--keys KEYS] [--thumbnails CACHE]
//                    [--sort FIELDS] [--info-expanded] [--info-width W] [--bench FRAMES]
//                    [--walk FRAMES] [--message TEXT]]
//                    <out.ppm>
//                    [logical-width logical-height scale [colors.toml]]
//
// KEYS are pressed in the column view before the snapshot: comma-separated xkb key names with
// optional Ctrl+, Shift+, Alt+ prefixes, e.g. "Down,Shift+Down,Ctrl+Down" (a single letter also
// types itself: "F2,L,a,k,e" renames to "Lake"); "Ctrl+2" switches to
// the grid (then --bench scrolls the grid), "Ctrl+3" the details (--bench scrolls them; --sort
// clicks column titles after the keys: "size", "modified,modified" for newest first); with "space"
// last, Quick Look opens (and --bench scrolls its text page, when it shows one). --thumbnails gives
// the grid a thumbnail cache directory (none by default: icons only, nothing written to ~/.cache).
// The sidebar lists the places of $HOME and $XDG_CONFIG_HOME; "Ctrl+B" hides it, "F6" focuses it.
// --message shows TEXT in the status bar as opening a file would ("!TEXT": as an error).
//
// Uses a surfaceless EGL context (EGL_MESA_platform_surfaceless) and a framebuffer object, so
// it never talks to the compositor and never opens a window: it is how rendering is checked
// without touching the user's screen.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include <xkbcommon/xkbcommon.h>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/log.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/fs/places.hpp"
#include "nordend/preview/preview.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/thumbs/cache.hpp"
#include "nordend/ui/color.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/seat.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/views/browser.hpp"
#include "test_scene.hpp"

namespace {

namespace log = nordend::log;
namespace core = nordend::core;
namespace preview = nordend::preview;
namespace ui = nordend::ui;
namespace views = nordend::views;

class SurfacelessContext {
public:
    SurfacelessContext()
        : display_{
              eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)} {
        if (display_ == EGL_NO_DISPLAY || eglInitialize(display_, nullptr, nullptr) != EGL_TRUE) {
            throw std::runtime_error{"surfaceless EGL display unavailable"};
        }
        eglBindAPI(EGL_OPENGL_ES_API);
        // clang-format off
        static constexpr std::array<EGLint, 5> kAttribs{
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE};
        // clang-format on
        context_ = eglCreateContext(display_, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, kAttribs.data());
        if (context_ == EGL_NO_CONTEXT ||
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_) != EGL_TRUE) {
            throw std::runtime_error{"cannot create a surfaceless OpenGL ES 3.0 context"};
        }
    }

    ~SurfacelessContext() {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display_, context_);
        eglTerminate(display_);
    }

    SurfacelessContext(const SurfacelessContext&) = delete;
    SurfacelessContext& operator=(const SurfacelessContext&) = delete;
    SurfacelessContext(SurfacelessContext&&) = delete;
    SurfacelessContext& operator=(SurfacelessContext&&) = delete;

private:
    EGLDisplay display_;
    EGLContext context_{EGL_NO_CONTEXT};
};

class Framebuffer {
public:
    Framebuffer(std::int32_t width, std::int32_t height) {
        glGenRenderbuffers(1, &color_);
        glBindRenderbuffer(GL_RENDERBUFFER, color_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error{"framebuffer incomplete"};
        }
    }

    ~Framebuffer() {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteRenderbuffers(1, &color_);
    }

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&&) = delete;
    Framebuffer& operator=(Framebuffer&&) = delete;

private:
    GLuint color_{0};
    GLuint fbo_{0};
};

void write_ppm(const std::string& path, std::int32_t width, std::int32_t height,
               std::span<const std::uint8_t> rgba) {
    std::ofstream out{path, std::ios::binary};
    out << "P6\n" << width << ' ' << height << "\n255\n";
    // GL rows go bottom-up; PPM rows go top-down.
    const auto row_bytes = static_cast<std::size_t>(width) * 4;
    for (std::int32_t y = height - 1; y >= 0; --y) {
        const auto row = rgba.subspan(static_cast<std::size_t>(y) * row_bytes, row_bytes);
        for (std::size_t x = 0; x < row_bytes; x += 4) {
            out.put(static_cast<char>(row[x]));
            out.put(static_cast<char>(row[x + 1]));
            out.put(static_cast<char>(row[x + 2]));
        }
    }
    if (!out) {
        throw std::runtime_error{"cannot write " + path};
    }
}

// Runs the loop until done() holds or two seconds pass.
void run_until(core::EventLoop& loop, const std::function<bool()>& done) {
    const auto check =
        loop.add_timer(std::chrono::milliseconds{1}, std::chrono::milliseconds{1}, [&] {
            if (done()) {
                loop.quit(0);
            }
        });
    const auto limit = loop.add_timer(std::chrono::seconds{2}, std::chrono::milliseconds{0},
                                      [&] { loop.quit(1); });
    loop.run();
    loop.remove(check);
    loop.remove(limit);
}

// "Down,Shift+Down,Ctrl+a" -> key events.
std::vector<ui::KeyEvent> parse_keys(std::string_view keys) {
    std::vector<ui::KeyEvent> events;
    while (!keys.empty()) {
        const std::size_t comma = keys.find(',');
        std::string_view key = keys.substr(0, comma);
        keys = comma == std::string_view::npos ? std::string_view{} : keys.substr(comma + 1);
        ui::KeyEvent event;
        for (;;) {
            if (key.starts_with("Ctrl+")) {
                event.modifiers.ctrl = true;
                key.remove_prefix(5);
            } else if (key.starts_with("Shift+")) {
                event.modifiers.shift = true;
                key.remove_prefix(6);
            } else if (key.starts_with("Alt+")) {
                event.modifiers.alt = true;
                key.remove_prefix(4);
            } else {
                break;
            }
        }
        event.keysym = xkb_keysym_from_name(std::string{key}.c_str(), XKB_KEYSYM_NO_FLAGS);
        if (key.size() == 1 && !event.modifiers.ctrl && !event.modifiers.alt) {
            event.text = std::string{key};  // a letter types itself (a name being edited)
        }
        if (event.keysym == XKB_KEY_NoSymbol) {
            throw std::runtime_error{"unknown key: " + std::string{key}};
        }
        events.push_back(std::move(event));
    }
    return events;
}

}  // namespace

int main(int argc, char** argv) {
    std::span args{argv, static_cast<std::size_t>(argc)};
    // --columns DIR: the column view instead of the test scene; --reveal PATH: with the columns
    // opened down to PATH (inside DIR); --bench N: then scroll the last column (or the grid) over N
    // frames and report the time per frame.
    std::filesystem::path columns_dir;
    std::filesystem::path reveal;
    std::string keys;
    std::filesystem::path thumbnails;
    std::string sort;
    int bench_frames = 0;
    int walk_frames = 0;  // --walk N: Down once per frame, N frames, workers' results between
    bool info_expanded = false;
    float info_width = 0.0F;  // the info pane as if dragged to this width
    std::string message;
    while (args.size() >= 2) {
        const std::string_view option{args[1]};
        if (option == "--info-expanded") {  // the info pane with "More details" open
            info_expanded = true;
            args = args.subspan(1);
            continue;
        }
        if (args.size() < 3) {
            break;
        }
        if (option == "--columns") {
            columns_dir = args[2];
        } else if (option == "--reveal") {
            reveal = args[2];
        } else if (option == "--keys") {
            keys = args[2];
        } else if (option == "--thumbnails") {
            thumbnails = args[2];
        } else if (option == "--sort") {
            sort = args[2];
        } else if (option == "--info-width") {
            info_width = std::stof(args[2]);
        } else if (option == "--message") {
            message = args[2];
        } else if (option == "--walk") {
            walk_frames = std::stoi(args[2]);
        } else if (option == "--bench") {
            bench_frames = std::stoi(args[2]);
        } else {
            break;
        }
        args = args.subspan(2);
    }
    if (args.size() != 2 && args.size() != 5 && args.size() != 6) {
        log::error(
            "usage: nordend-snapshot [--columns DIR [--reveal PATH] [--keys KEYS] [--thumbnails "
            "CACHE] [--sort FIELDS] [--info-expanded] [--info-width W] [--bench FRAMES] [--walk "
            "FRAMES] [--message TEXT]] "
            "<out.ppm> "
            "[logical-width logical-height scale [colors.toml]]");
        return 2;
    }
    try {
        const std::string out{args[1]};
        const bool sized = args.size() >= 5;
        const float width = sized ? std::stof(args[2]) : 960.0F;
        const float height = sized ? std::stof(args[3]) : 640.0F;
        const float scale = sized ? std::stof(args[4]) : 1.0F;
        // The default palette unless a colors.toml is given: snapshots stay reproducible.
        const nordend::theme::PaletteResult palette = args.size() == 6
                                                          ? nordend::theme::load_palette(args[5])
                                                          : nordend::theme::PaletteResult{};
        for (const std::string& problem : palette.problems) {
            log::warn("{}", problem);
        }
        const auto buffer_width = static_cast<std::int32_t>(std::lround(width * scale));
        const auto buffer_height = static_cast<std::int32_t>(std::lround(height * scale));

        const SurfacelessContext context;
        const Framebuffer framebuffer{buffer_width, buffer_height};
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(buffer_width) *
                                         static_cast<std::size_t>(buffer_height) * 4);
        {
            ui::FontSystem fonts;
            ui::Renderer renderer{fonts};
            ui::IconSet icons{fonts};
            core::EventLoop loop;
            core::WorkerPool pool{loop};
            nordend::fs::DirectoryWatcher watcher{loop};
            std::unique_ptr<views::Browser> columns;
            const ui::Rect bounds{0.0F, 0.0F, width, height};
            const auto frame = [&] {
                renderer.begin_frame(buffer_width, buffer_height, scale, palette.palette.surface);
                if (columns) {
                    columns->draw(renderer, bounds, palette.palette, icons,
                                  views::Browser::Clock::now());
                } else {
                    nordend::app::draw_test_scene(renderer, bounds, palette.palette);
                }
                renderer.end_frame();
            };
            if (!columns_dir.empty()) {
                columns = std::make_unique<views::Browser>(
                    loop, pool, watcher, nordend::fs::MimeDatabase::load(), [] {},
                    [](ui::CursorShape /*shape*/) {},
                    thumbnails.empty()
                        ? nullptr
                        : std::make_shared<nordend::thumbs::Cache>(thumbnails, "nordend-dev"));
                columns->set_places(
                    nordend::fs::load_places(nordend::fs::PlaceSources::from_environment()));
                if (reveal.empty()) {
                    columns->open(columns_dir);
                } else {
                    columns->reveal(columns_dir, reveal);
                }
                run_until(loop, [&] { return !columns->loading(); });
                columns->info_pane().set_expanded(info_expanded);
                if (info_width > 0.0F) {
                    columns->info_pane().set_width(info_width);
                }
                if (!message.empty()) {
                    const bool error = message.starts_with('!');
                    columns->show_message(error ? message.substr(1) : message, error);
                }
                frame();  // lays out: keys need the viewport
                for (const ui::KeyEvent& event : parse_keys(keys)) {
                    columns->key(event, views::Browser::Clock::now());
                }
                std::string_view fields{sort};
                while (!fields.empty()) {
                    const std::size_t comma = std::min(fields.find(','), fields.size());
                    const std::string_view field = fields.substr(0, comma);
                    fields.remove_prefix(std::min(comma + 1, fields.size()));
                    using Field = views::DetailsView::Field;
                    Field chosen = Field::name;
                    if (field == "size") {
                        chosen = Field::size;
                    } else if (field == "modified") {
                        chosen = Field::modified;
                    } else if (field == "type") {
                        chosen = Field::type;
                    }
                    const auto start = std::chrono::steady_clock::now();
                    columns->details().sort_by(chosen);
                    log::info("sorted by {} in {:.2f} ms", field,
                              std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - start)
                                  .count());
                }
                run_until(loop, [&] { return !columns->loading(); });
                // The first frame asks for the visible entries' types and, in the grid, their
                // thumbnails, made on workers; each frame uploads the ones that arrived.
                const auto start = std::chrono::steady_clock::now();
                frame();
                if (columns->loading()) {
                    for (int i = 0; i < 1000 && columns->loading(); ++i) {
                        run_until(loop, [&] { return !columns->loading(); });
                        frame();
                    }
                    log::info("thumbnails ready in {:.0f} ms",
                              std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - start)
                                  .count());
                }
                run_until(loop, [] { return false; });  // let sniffed types arrive (2 s)
                // Let the horizontal glide end, so the snapshot shows where it settles.
                for (int i = 0; i < 200 && columns->animating(); ++i) {
                    frame();
                }
            }
            // What --bench scrolls: Quick Look's text page, the last column, or the grid.
            ui::Scroller* bench_scroller = nullptr;
            std::size_t bench_entries = 0;
            float bench_extent = 0.0F;
            const preview::Content* const shown =
                columns ? columns->quick_look().content() : nullptr;
            const auto* const text = shown ? std::get_if<preview::Text>(shown) : nullptr;
            if (text != nullptr) {
                bench_scroller = &columns->quick_look().text_scroller();
                bench_entries = text->lines.size();
                bench_extent =
                    static_cast<float>(bench_entries) * views::QuickLook::kTextLineHeight;
            } else if (columns && columns->mode() == views::Browser::Mode::details) {
                views::DetailsView& details = columns->details();
                bench_scroller = &details.scroller();
                bench_entries = details.folder()->entries().size();
                bench_extent = static_cast<float>(bench_entries) * views::DetailsView::kRowHeight;
            } else if (columns && columns->mode() == views::Browser::Mode::grid) {
                views::GridView& grid = columns->grid();
                bench_scroller = &grid.scroller();
                bench_entries = grid.folder()->entries().size();
                const std::size_t rows = (bench_entries + grid.columns() - 1) / grid.columns();
                bench_extent = static_cast<float>(rows) * grid.cell_height();
            } else if (columns) {
                views::Column& column =
                    columns->columns().column(columns->columns().column_count() - 1);
                bench_scroller = &column.scroller();
                bench_entries = column.folder().entries().size();
                bench_extent = static_cast<float>(bench_entries) * views::Column::kRowHeight;
            }
            if (bench_scroller != nullptr && bench_frames > 0) {
                // Scroll from top to bottom at constant speed; every frame waits for the GPU.
                std::vector<double> times;
                times.reserve(static_cast<std::size_t>(bench_frames));
                for (int i = 0; i < bench_frames; ++i) {
                    bench_scroller->jump_to(bench_extent * static_cast<float>(i) /
                                            static_cast<float>(bench_frames));
                    const auto start = std::chrono::steady_clock::now();
                    frame();
                    glFinish();
                    times.push_back(std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - start)
                                        .count());
                }
                std::ranges::sort(times);
                const auto at = [&](double q) {
                    return times[static_cast<std::size_t>(q *
                                                          static_cast<double>(times.size() - 1))];
                };
                std::printf(
                    "%zu entries, %d frames at %dx%d: median %.2f ms, p95 %.2f ms, "
                    "max %.2f ms per frame\n",
                    bench_entries, bench_frames, buffer_width, buffer_height, at(0.5), at(0.95),
                    times.back());
                bench_scroller->jump_to(0.0F);
            }
            if (columns && walk_frames > 0) {
                // The cursor walks down a frame at a time, as a held key would but faster; the
                // loop runs about 1 ms between frames so that workers' results arrive. Only the
                // frames are timed.
                std::vector<double> times;
                times.reserve(static_cast<std::size_t>(walk_frames));
                ui::KeyEvent down;
                down.keysym = XKB_KEY_Down;
                for (int i = 0; i < walk_frames; ++i) {
                    columns->key(down, views::Browser::Clock::now());
                    run_until(loop, [] { return true; });
                    const auto start = std::chrono::steady_clock::now();
                    frame();
                    glFinish();
                    times.push_back(std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - start)
                                        .count());
                }
                std::ranges::sort(times);
                const auto at = [&](double q) {
                    return times[static_cast<std::size_t>(q *
                                                          static_cast<double>(times.size() - 1))];
                };
                std::printf(
                    "walked %d entries at %dx%d: median %.2f ms, p95 %.2f ms, max %.2f ms "
                    "per frame\n",
                    walk_frames, buffer_width, buffer_height, at(0.5), at(0.95), times.back());
            }
            frame();
            glReadPixels(0, 0, buffer_width, buffer_height, GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());
        }
        write_ppm(out, buffer_width, buffer_height, pixels);
        log::info("wrote {} ({}x{} at scale {})", out, buffer_width, buffer_height, scale);
        return 0;
    } catch (const std::exception& e) {
        log::error("{}", e.what());
        return 1;
    }
}
