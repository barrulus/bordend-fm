#include "nordend/ui/window.hpp"

#include <algorithm>
#include <iterator>

#include "nordend/core/log.hpp"
#include "shm_buffer.hpp"

namespace nordend::ui {
namespace {

// Placeholder background until the theme (ROADMAP 2.5) provides one.
constexpr std::uint32_t kBackgroundArgb = 0xFF1E1E2E;

}  // namespace

Window::Window(Display& display, const WindowOptions& options)
    : display_{display}, width_{options.width}, height_{options.height} {
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

    // Initial commit without a buffer: the compositor answers with the first configure.
    wl_surface_commit(surface_.get());
}

Window::~Window() = default;

void Window::on_surface_configure(void* data, xdg_surface* surface, std::uint32_t serial) {
    auto& self = *static_cast<Window*>(data);
    xdg_surface_ack_configure(surface, serial);
    if (self.pending_width_ > 0 && self.pending_height_ > 0) {
        self.width_ = self.pending_width_;
        self.height_ = self.pending_height_;
    }
    self.present();
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

void Window::present() {
    // Free released buffers of a stale size; reuse a released one of the current size.
    std::erase_if(buffers_, [&](const auto& buffer) {
        return !buffer->busy() && (buffer->width() != width_ || buffer->height() != height_);
    });
    auto it = std::ranges::find_if(buffers_, [](const auto& buffer) { return !buffer->busy(); });
    if (it == buffers_.end()) {
        buffers_.push_back(
            std::make_unique<ShmBuffer>(display_.shm(), width_, height_, kBackgroundArgb));
        it = std::prev(buffers_.end());
    }

    ShmBuffer& buffer = **it;
    wl_surface_attach(surface_.get(), buffer.native(), 0, 0);
    wl_surface_damage_buffer(surface_.get(), 0, 0, width_, height_);
    wl_surface_commit(surface_.get());
    buffer.mark_busy();
    log::trace("presented {}x{} ({} buffers)", width_, height_, buffers_.size());
}

}  // namespace nordend::ui
