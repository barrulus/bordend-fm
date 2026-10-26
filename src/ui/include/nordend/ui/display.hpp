#pragma once

#include <cstdint>
#include <wayland-client.h>

#include "nordend/ui/wl_ptr.hpp"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

namespace nordend::ui {

// Connection to the Wayland compositor and the globals Nordend binds.
// Throws std::runtime_error if the connection fails or a required global is missing.
// Not copyable or movable: listeners keep a pointer to this object.
class Display {
public:
    Display();
    ~Display() = default;

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    Display(Display&&) = delete;
    Display& operator=(Display&&) = delete;

    [[nodiscard]] wl_display* native() const noexcept { return display_.get(); }

    [[nodiscard]] int fd() const noexcept { return wl_display_get_fd(display_.get()); }

    [[nodiscard]] wl_compositor* compositor() const noexcept { return compositor_.get(); }

    [[nodiscard]] xdg_wm_base* wm_base() const noexcept { return wm_base_.get(); }

    // Optional: nullptr when the compositor does not offer server-side decorations.
    [[nodiscard]] zxdg_decoration_manager_v1* decoration_manager() const noexcept {
        return decoration_manager_.get();
    }

    // Optional: nullptr on a compositor without input devices.
    [[nodiscard]] wl_seat* seat() const noexcept { return seat_.get(); }

    // Optional: fractional scaling needs both; without them windows render at scale 1.
    [[nodiscard]] wp_viewporter* viewporter() const noexcept { return viewporter_.get(); }

    [[nodiscard]] wp_fractional_scale_manager_v1* fractional_scale_manager() const noexcept {
        return fractional_scale_manager_.get();
    }

private:
    static void on_global(void* data, wl_registry* registry, std::uint32_t name,
                          const char* interface, std::uint32_t version);
    static void on_global_remove(void* data, wl_registry* registry, std::uint32_t name);
    static void on_ping(void* data, xdg_wm_base* wm_base, std::uint32_t serial);

    // Reverse order = teardown order: globals first, the connection last.
    WlPtr<wl_display> display_;
    WlPtr<wl_registry> registry_;
    WlPtr<wl_compositor> compositor_;
    WlPtr<xdg_wm_base> wm_base_;
    WlPtr<zxdg_decoration_manager_v1> decoration_manager_;
    WlPtr<wp_viewporter> viewporter_;
    WlPtr<wp_fractional_scale_manager_v1> fractional_scale_manager_;
    WlPtr<wl_seat> seat_;
};

}  // namespace nordend::ui
