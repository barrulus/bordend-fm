// nordend-snapshot: renders the test scene (or the column view) offscreen and writes it as a
// binary PPM.
//
//   nordend-snapshot [--columns DIR [--reveal PATH] [--bench FRAMES]] <out.ppm>
//                    [logical-width logical-height scale [colors.toml]]
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
#include <vector>

#include "nordend/core/event_loop.hpp"
#include "nordend/core/log.hpp"
#include "nordend/core/worker_pool.hpp"
#include "nordend/fs/directory_watcher.hpp"
#include "nordend/fs/mime.hpp"
#include "nordend/theme/palette.hpp"
#include "nordend/ui/color.hpp"
#include "nordend/ui/icons.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/views/column_view.hpp"
#include "test_scene.hpp"

namespace {

namespace log = nordend::log;
namespace core = nordend::core;
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

}  // namespace

int main(int argc, char** argv) {
    std::span args{argv, static_cast<std::size_t>(argc)};
    // --columns DIR: the column view instead of the test scene; --reveal PATH: with the columns
    // opened down to PATH (inside DIR); --bench N: then scroll the last column over N frames and
    // report the time per frame.
    std::filesystem::path columns_dir;
    std::filesystem::path reveal;
    int bench_frames = 0;
    while (args.size() >= 3) {
        const std::string_view option{args[1]};
        if (option == "--columns") {
            columns_dir = args[2];
        } else if (option == "--reveal") {
            reveal = args[2];
        } else if (option == "--bench") {
            bench_frames = std::stoi(args[2]);
        } else {
            break;
        }
        args = args.subspan(2);
    }
    if (args.size() != 2 && args.size() != 5 && args.size() != 6) {
        log::error(
            "usage: nordend-snapshot [--columns DIR [--reveal PATH] [--bench FRAMES]] <out.ppm> "
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
            std::unique_ptr<views::ColumnView> columns;
            const ui::Rect bounds{0.0F, 0.0F, width, height};
            const auto frame = [&] {
                renderer.begin_frame(buffer_width, buffer_height, scale, palette.palette.surface);
                if (columns) {
                    columns->draw(renderer, bounds, palette.palette, icons,
                                  views::ColumnView::Clock::now());
                } else {
                    nordend::app::draw_test_scene(renderer, bounds, palette.palette);
                }
                renderer.end_frame();
            };
            if (!columns_dir.empty()) {
                columns = std::make_unique<views::ColumnView>(
                    loop, pool, watcher, nordend::fs::MimeDatabase::load(), [] {},
                    [](ui::CursorShape /*shape*/) {});
                if (reveal.empty()) {
                    columns->open(columns_dir);
                } else {
                    columns->reveal(columns_dir, reveal);
                }
                run_until(loop, [&] { return !columns->loading(); });
                frame();                                // asks for the visible entries' types
                run_until(loop, [] { return false; });  // let them arrive (2 s at most)
                // Let the horizontal glide end, so the snapshot shows where it settles.
                for (int i = 0; i < 200 && columns->animating(); ++i) {
                    frame();
                }
            }
            views::Column* const bench_column =
                columns ? &columns->column(columns->column_count() - 1) : nullptr;
            if (bench_column != nullptr && bench_frames > 0) {
                // Scroll the last column from top to bottom at constant speed; every frame waits
                // for the GPU.
                const float rows =
                    static_cast<float>(bench_column->model().listing().entries().size()) *
                    views::Column::kRowHeight;
                std::vector<double> times;
                times.reserve(static_cast<std::size_t>(bench_frames));
                for (int i = 0; i < bench_frames; ++i) {
                    bench_column->scroller().jump_to(rows * static_cast<float>(i) /
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
                    bench_column->model().listing().entries().size(), bench_frames, buffer_width,
                    buffer_height, at(0.5), at(0.95), times.back());
                bench_column->scroller().jump_to(0.0F);
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
