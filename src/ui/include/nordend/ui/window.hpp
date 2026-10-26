#pragma once

#include <EGL/egl.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "nordend/ui/color.hpp"
#include "nordend/ui/display.hpp"
#include "nordend/ui/egl_context.hpp"
#include "nordend/ui/geometry.hpp"
#include "nordend/ui/renderer.hpp"
#include "nordend/ui/text.hpp"
#include "nordend/ui/wl_ptr.hpp"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

namespace nordend::ui {

struct WindowOptions {
    std::string title;
    std::string app_id;  // matched by compositor window rules
    std::int32_t width{960};
    std::int32_t height{640};
    std::int32_t min_width{480};
    std::int32_t min_height{320};
    Color background{Color::rgb(0x000000)};  // behind the content; the app passes the theme's
};

// A top-level xdg-shell window rendered with OpenGL ES, with server-side decorations when the
// compositor offers them.
//
// Frame pacing: at most one frame in flight. A frame is drawn only when something changed
// (configure, request_redraw) and, while a frame is pending, the next one waits for the
// compositor's frame callback. An idle window draws nothing.
//
// Must not outlive its Display, EglContext and FontSystem. Not copyable or movable: listeners keep
// a pointer to it.
class Window {
public:
    Window(Display& display, EglContext& egl, FontSystem& fonts, const WindowOptions& options);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    // Paints the window content each frame. bounds is the window in logical pixels.
    using DrawHandler = std::function<void(Renderer& renderer, const Rect& bounds)>;
    void set_draw_handler(DrawHandler handler);

    // Schedules a frame: drawn now if none is in flight, otherwise on the next frame callback.
    void request_redraw();

    // Color the frame is cleared to before the draw handler runs; redraws.
    void set_background(Color color) {
        background_ = color;
        request_redraw();
    }

    // Called when the user asks to close the window (title-bar button, compositor keybinding).
    void set_close_handler(std::function<void()> handler) { close_handler_ = std::move(handler); }

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

    // Output scale chosen by the compositor (1.0, 1.25, 1.5, ...); 1.0 without fractional-scale.
    [[nodiscard]] double scale() const noexcept { return static_cast<double>(scale_120_) / 120.0; }

    // Size of the rendered buffer in physical pixels.
    [[nodiscard]] std::int32_t buffer_width() const noexcept { return buffer_width_; }

    [[nodiscard]] std::int32_t buffer_height() const noexcept { return buffer_height_; }

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
    static void on_preferred_scale(void* data, wp_fractional_scale_v1* scale,
                                   std::uint32_t scale_120);
    static void on_frame_done(void* data, wl_callback* callback, std::uint32_t time_ms);

    void draw();
    // Recomputes the physical buffer size from the logical size and scale; resizes the
    // wl_egl_window if it changed.
    void update_buffer_size();

    Display& display_;
    EglContext& egl_;
    std::int32_t width_;  // logical size, from configure
    std::int32_t height_;
    std::int32_t scale_120_{120};   // scale numerator over 120
    std::int32_t buffer_width_{0};  // physical size, in pixels
    std::int32_t buffer_height_{0};
    std::int32_t pending_width_{0};  // 0 = the client chooses
    std::int32_t pending_height_{0};
    bool configured_{false};
    bool dirty_{false};
    Color background_;
    DrawHandler draw_handler_;
    std::function<void()> close_handler_;
    std::unique_ptr<Renderer> renderer_;      // needs the context current: see ~Window
    EGLSurface egl_surface_{EGL_NO_SURFACE};  // destroyed first, in ~Window

    // Reverse order = teardown order: frame callback, decoration, toplevel, xdg_surface,
    // wl_egl_window, fractional scale, viewport, surface.
    WlPtr<wl_surface> surface_;
    WlPtr<wp_viewport> viewport_;
    WlPtr<wp_fractional_scale_v1> fractional_scale_;
    WlPtr<wl_egl_window> egl_window_;
    WlPtr<xdg_surface> xdg_surface_;
    WlPtr<xdg_toplevel> toplevel_;
    WlPtr<zxdg_toplevel_decoration_v1> decoration_;
    WlPtr<wl_callback> frame_callback_;  // non-null while a frame is in flight
};

}  // namespace nordend::ui
