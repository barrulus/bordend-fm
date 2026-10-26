#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <wayland-client.h>

#include "nordend/ui/display.hpp"
#include "nordend/ui/wl_ptr.hpp"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

namespace nordend::ui {

class ShmBuffer;

struct WindowOptions {
    std::string title;
    std::string app_id;  // matched by compositor window rules
    std::int32_t width{960};
    std::int32_t height{640};
    std::int32_t min_width{480};
    std::int32_t min_height{320};
};

// A top-level xdg-shell window with server-side decorations when the compositor offers them.
// Must not outlive its Display. Not copyable or movable: listeners keep a pointer to it.
class Window {
public:
    Window(Display& display, const WindowOptions& options);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    // The user asked to close the window (title-bar button, compositor keybinding, ...).
    [[nodiscard]] bool close_requested() const noexcept { return close_requested_; }

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

private:
    static void on_surface_configure(void* data, xdg_surface* surface, std::uint32_t serial);
    static void on_toplevel_configure(void* data, xdg_toplevel* toplevel, std::int32_t width,
                                      std::int32_t height, wl_array* states);
    static void on_toplevel_close(void* data, xdg_toplevel* toplevel);
    static void on_toplevel_configure_bounds(void* data, xdg_toplevel* toplevel, std::int32_t width,
                                             std::int32_t height);
    static void on_toplevel_wm_capabilities(void* data, xdg_toplevel* toplevel,
                                            wl_array* capabilities);
    static void on_decoration_configure(void* data, zxdg_toplevel_decoration_v1* decoration,
                                        std::uint32_t mode);

    void present();

    Display& display_;
    std::int32_t width_;
    std::int32_t height_;
    std::int32_t pending_width_{0};  // 0 = the client chooses
    std::int32_t pending_height_{0};
    bool close_requested_{false};

    std::vector<std::unique_ptr<ShmBuffer>> buffers_;

    // Reverse order = teardown order: decoration, toplevel, xdg_surface, surface.
    WlPtr<wl_surface> surface_;
    WlPtr<xdg_surface> xdg_surface_;
    WlPtr<xdg_toplevel> toplevel_;
    WlPtr<zxdg_toplevel_decoration_v1> decoration_;
};

}  // namespace nordend::ui
