#pragma once

#include <memory>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

namespace nordend::ui {

// One overload per Wayland proxy type we own. Defined out of line on purpose: the
// wayland-scanner *_destroy helpers are `static inline`, so naming them in a type used across
// translation units would give that type internal linkage (an ODR hazard).
void destroy(wl_display* display) noexcept;
void destroy(wl_registry* registry) noexcept;
void destroy(wl_compositor* compositor) noexcept;
void destroy(wl_surface* surface) noexcept;
void destroy(wl_callback* callback) noexcept;
void destroy(wl_seat* seat) noexcept;
void destroy(wl_keyboard* keyboard) noexcept;
void destroy(wl_egl_window* window) noexcept;
void destroy(xdg_wm_base* wm_base) noexcept;
void destroy(xdg_surface* surface) noexcept;
void destroy(xdg_toplevel* toplevel) noexcept;
void destroy(zxdg_decoration_manager_v1* manager) noexcept;
void destroy(zxdg_toplevel_decoration_v1* decoration) noexcept;
void destroy(wp_viewporter* viewporter) noexcept;
void destroy(wp_viewport* viewport) noexcept;
void destroy(wp_fractional_scale_manager_v1* manager) noexcept;
void destroy(wp_fractional_scale_v1* scale) noexcept;

struct WlDeleter {
    template <class T>
    void operator()(T* proxy) const noexcept {
        destroy(proxy);
    }
};

// Owning pointer for a Wayland proxy. Destruction order between proxies matters: declare
// members so that the reverse declaration order is the protocol's teardown order.
template <class T>
using WlPtr = std::unique_ptr<T, WlDeleter>;

}  // namespace nordend::ui
