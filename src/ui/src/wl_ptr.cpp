#include "nordend/ui/wl_ptr.hpp"

namespace nordend::ui {

void destroy(wl_display* display) noexcept { wl_display_disconnect(display); }

void destroy(wl_registry* registry) noexcept { wl_registry_destroy(registry); }

void destroy(wl_compositor* compositor) noexcept { wl_compositor_destroy(compositor); }

void destroy(wl_shm* shm) noexcept { wl_shm_destroy(shm); }

void destroy(wl_shm_pool* pool) noexcept { wl_shm_pool_destroy(pool); }

void destroy(wl_buffer* buffer) noexcept { wl_buffer_destroy(buffer); }

void destroy(wl_surface* surface) noexcept { wl_surface_destroy(surface); }

void destroy(xdg_wm_base* wm_base) noexcept { xdg_wm_base_destroy(wm_base); }

void destroy(xdg_surface* surface) noexcept { xdg_surface_destroy(surface); }

void destroy(xdg_toplevel* toplevel) noexcept { xdg_toplevel_destroy(toplevel); }

void destroy(zxdg_decoration_manager_v1* manager) noexcept {
    zxdg_decoration_manager_v1_destroy(manager);
}

void destroy(zxdg_toplevel_decoration_v1* decoration) noexcept {
    zxdg_toplevel_decoration_v1_destroy(decoration);
}

}  // namespace nordend::ui
