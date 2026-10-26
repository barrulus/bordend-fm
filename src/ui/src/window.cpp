#include "nordend/ui/window.hpp"

#include <GLES3/gl3.h>
#include <format>
#include <stdexcept>

#include "nordend/core/log.hpp"

namespace nordend::ui {
namespace {

// Placeholder background (#1E1E2E) until the theme (ROADMAP 2.5) provides one.
constexpr GLfloat kBackgroundRed = 0x1E / 255.0F;
constexpr GLfloat kBackgroundGreen = 0x1E / 255.0F;
constexpr GLfloat kBackgroundBlue = 0x2E / 255.0F;

}  // namespace

Window::Window(Display& display, EglContext& egl, const WindowOptions& options)
    : display_{display}, egl_{egl}, width_{options.width}, height_{options.height} {
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

    egl_window_.reset(wl_egl_window_create(surface_.get(), width_, height_));
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

    eglMakeCurrent(egl_.display(), egl_surface_, egl_surface_, egl_.context());
    glViewport(0, 0, width_, height_);
    glClearColor(kBackgroundRed, kBackgroundGreen, kBackgroundBlue, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    if (eglSwapBuffers(egl_.display(), egl_surface_) != EGL_TRUE) {
        log::error("eglSwapBuffers failed (0x{:04x})", eglGetError());
    }
    log::trace("frame {}x{}", width_, height_);
}

void Window::on_surface_configure(void* data, xdg_surface* surface, std::uint32_t serial) {
    auto& self = *static_cast<Window*>(data);
    xdg_surface_ack_configure(surface, serial);
    self.configured_ = true;
    if (self.pending_width_ > 0 && self.pending_height_ > 0 &&
        (self.pending_width_ != self.width_ || self.pending_height_ != self.height_)) {
        self.width_ = self.pending_width_;
        self.height_ = self.pending_height_;
        wl_egl_window_resize(self.egl_window_.get(), self.width_, self.height_, 0, 0);
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

void Window::on_frame_done(void* data, wl_callback* /*callback*/, std::uint32_t /*time_ms*/) {
    auto& self = *static_cast<Window*>(data);
    self.frame_callback_.reset();
    if (self.dirty_) {
        self.draw();
    }
}

}  // namespace nordend::ui
