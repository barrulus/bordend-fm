#include "nordend/ui/window.hpp"

#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdlib>
#include <format>
#include <stdexcept>

#include "nordend/core/log.hpp"

namespace nordend::ui {
namespace {

// Placeholder background (#1E1E2E) until the theme (ROADMAP 2.5) provides one.
constexpr GLfloat kBackgroundRed = 0x1E / 255.0F;
constexpr GLfloat kBackgroundGreen = 0x1E / 255.0F;
constexpr GLfloat kBackgroundBlue = 0x2E / 255.0F;

// Temporary test pattern (NORDEND_SCALE_TEST=1) until the primitives of ROADMAP 1.4 exist:
// alternating 1-physical-pixel vertical lines on the left. Crisp lines mean the buffer maps 1:1
// onto screen pixels; a blurred or flat grey band means the compositor is rescaling it.
constexpr std::int32_t kScaleTestBandWidth = 400;

void draw_scale_test_pattern(std::int32_t buffer_width, std::int32_t buffer_height) {
    glEnable(GL_SCISSOR_TEST);
    glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
    const std::int32_t band = std::min(buffer_width, kScaleTestBandWidth);
    for (std::int32_t x = 0; x < band; x += 2) {
        glScissor(x, 0, 1, buffer_height);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);
}

// fractional-scale-v1 sends scales as numerator over 120.
constexpr std::int32_t kScaleDenominator = 120;

// Physical size of a logical length, rounded half away from zero as the protocol asks.
std::int32_t to_physical(std::int32_t logical, std::int32_t scale_120) {
    return ((logical * scale_120) + (kScaleDenominator / 2)) / kScaleDenominator;
}

}  // namespace

Window::Window(Display& display, EglContext& egl, const WindowOptions& options)
    : display_{display},
      egl_{egl},
      width_{options.width},
      height_{options.height},
      scale_test_{std::getenv("NORDEND_SCALE_TEST") != nullptr} {
    surface_.reset(wl_compositor_create_surface(display_.compositor()));

    static constexpr xdg_surface_listener kSurfaceListener{
        .configure = &Window::on_surface_configure,
    };
    xdg_surface_.reset(xdg_wm_base_get_xdg_surface(display_.wm_base(), surface_.get()));
    xdg_surface_add_listener(xdg_surface_.get(), &kSurfaceListener, this);

    static constexpr xdg_toplevel_listener kToplevelListener{
        .configure = &Window::on_toplevel_configure,
        .close = &Window::on_toplevel_close,
        .configure_bounds = &Window::on_toplevel_configure_bounds,
        .wm_capabilities = &Window::on_toplevel_wm_capabilities,
    };
    toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));
    xdg_toplevel_add_listener(toplevel_.get(), &kToplevelListener, this);
    xdg_toplevel_set_title(toplevel_.get(), options.title.c_str());
    xdg_toplevel_set_app_id(toplevel_.get(), options.app_id.c_str());
    xdg_toplevel_set_min_size(toplevel_.get(), options.min_width, options.min_height);

    if (auto* manager = display_.decoration_manager()) {
        static constexpr zxdg_toplevel_decoration_v1_listener kDecorationListener{
            .configure = &Window::on_decoration_configure,
        };
        decoration_.reset(
            zxdg_decoration_manager_v1_get_toplevel_decoration(manager, toplevel_.get()));
        zxdg_toplevel_decoration_v1_add_listener(decoration_.get(), &kDecorationListener, this);
        zxdg_toplevel_decoration_v1_set_mode(decoration_.get(),
                                             ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }

    if (display_.viewporter() != nullptr && display_.fractional_scale_manager() != nullptr) {
        static constexpr wp_fractional_scale_v1_listener kScaleListener{
            .preferred_scale = &Window::on_preferred_scale,
        };
        viewport_.reset(wp_viewporter_get_viewport(display_.viewporter(), surface_.get()));
        fractional_scale_.reset(wp_fractional_scale_manager_v1_get_fractional_scale(
            display_.fractional_scale_manager(), surface_.get()));
        wp_fractional_scale_v1_add_listener(fractional_scale_.get(), &kScaleListener, this);
    }

    buffer_width_ = to_physical(width_, scale_120_);
    buffer_height_ = to_physical(height_, scale_120_);
    egl_window_.reset(wl_egl_window_create(surface_.get(), buffer_width_, buffer_height_));
    egl_surface_ =
        eglCreatePlatformWindowSurface(egl_.display(), egl_.config(), egl_window_.get(), nullptr);
    if (egl_surface_ == EGL_NO_SURFACE) {
        throw std::runtime_error{
            std::format("eglCreatePlatformWindowSurface failed (0x{:04x})", eglGetError())};
    }
    eglMakeCurrent(egl_.display(), egl_surface_, egl_surface_, egl_.context());
    // Never block in eglSwapBuffers: frame callbacks pace us, the event loop stays responsive.
    eglSwapInterval(egl_.display(), 0);
    // glGetString returns GLubyte*; the cast to char* is the documented way to print it.
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));  // NOLINT
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));    // NOLINT
    log::info("GL: {} | {}", renderer, version);

    // Initial commit without a buffer: the compositor answers with the first configure.
    wl_surface_commit(surface_.get());
}

Window::~Window() {
    eglMakeCurrent(egl_.display(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_.display(), egl_surface_);
}

void Window::request_redraw() {
    dirty_ = true;
    if (configured_ && !frame_callback_) {
        draw();
    }
}

void Window::draw() {
    dirty_ = false;

    // Ask for the next frame callback before the swap commits this frame.
    static constexpr wl_callback_listener kFrameListener{.done = &Window::on_frame_done};
    frame_callback_.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback_.get(), &kFrameListener, this);

    // The buffer is in physical pixels; the viewport maps it back onto the logical size.
    // Both are double-buffered state applied by the commit inside eglSwapBuffers.
    if (viewport_) {
        wp_viewport_set_destination(viewport_.get(), width_, height_);
    }

    eglMakeCurrent(egl_.display(), egl_surface_, egl_surface_, egl_.context());
    glViewport(0, 0, buffer_width_, buffer_height_);
    glClearColor(kBackgroundRed, kBackgroundGreen, kBackgroundBlue, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    if (scale_test_) {
        draw_scale_test_pattern(buffer_width_, buffer_height_);
    }
    if (eglSwapBuffers(egl_.display(), egl_surface_) != EGL_TRUE) {
        log::error("eglSwapBuffers failed (0x{:04x})", eglGetError());
    }
    log::trace("frame {}x{} logical, {}x{} buffer", width_, height_, buffer_width_, buffer_height_);
}

void Window::update_buffer_size() {
    const std::int32_t width = to_physical(width_, scale_120_);
    const std::int32_t height = to_physical(height_, scale_120_);
    if (width != buffer_width_ || height != buffer_height_) {
        buffer_width_ = width;
        buffer_height_ = height;
        wl_egl_window_resize(egl_window_.get(), buffer_width_, buffer_height_, 0, 0);
    }
}

void Window::on_surface_configure(void* data, xdg_surface* surface, std::uint32_t serial) {
    auto& self = *static_cast<Window*>(data);
    xdg_surface_ack_configure(surface, serial);
    self.configured_ = true;
    if (self.pending_width_ > 0 && self.pending_height_ > 0 &&
        (self.pending_width_ != self.width_ || self.pending_height_ != self.height_)) {
        self.width_ = self.pending_width_;
        self.height_ = self.pending_height_;
        self.update_buffer_size();
    }
    // Every configure needs a commit to apply its ack; with a frame in flight the commit
    // happens on the next frame callback, which keeps interactive resizing at display rate.
    self.request_redraw();
}

void Window::on_toplevel_configure(void* data, xdg_toplevel* /*toplevel*/, std::int32_t width,
                                   std::int32_t height, wl_array* /*states*/) {
    auto& self = *static_cast<Window*>(data);
    self.pending_width_ = width;
    self.pending_height_ = height;
}

void Window::on_toplevel_close(void* data, xdg_toplevel* /*toplevel*/) {
    static_cast<Window*>(data)->close_requested_ = true;
}

void Window::on_toplevel_configure_bounds(void* /*data*/, xdg_toplevel* /*toplevel*/,
                                          std::int32_t width, std::int32_t height) {
    log::debug("configure bounds {}x{}", width, height);
}

void Window::on_toplevel_wm_capabilities(void* /*data*/, xdg_toplevel* /*toplevel*/,
                                         wl_array* /*capabilities*/) {}

void Window::on_decoration_configure(void* /*data*/, zxdg_toplevel_decoration_v1* /*decoration*/,
                                     std::uint32_t mode) {
    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
        log::debug("decorations: server-side");
    } else {
        log::warn("compositor asked for client-side decorations: the window will have none");
    }
}

void Window::on_preferred_scale(void* data, wp_fractional_scale_v1* /*scale*/,
                                std::uint32_t scale_120) {
    auto& self = *static_cast<Window*>(data);
    const auto scale = static_cast<std::int32_t>(scale_120);
    if (scale == self.scale_120_) {
        return;
    }
    log::debug("preferred scale {:.3f}", static_cast<double>(scale) / kScaleDenominator);
    self.scale_120_ = scale;
    self.update_buffer_size();
    self.request_redraw();
}

void Window::on_frame_done(void* data, wl_callback* /*callback*/, std::uint32_t /*time_ms*/) {
    auto& self = *static_cast<Window*>(data);
    self.frame_callback_.reset();
    if (self.dirty_) {
        self.draw();
    }
}

}  // namespace nordend::ui
