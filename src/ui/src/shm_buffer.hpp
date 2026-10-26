#pragma once

#include <cstddef>
#include <cstdint>
#include <wayland-client.h>

#include "nordend/ui/wl_ptr.hpp"

namespace nordend::ui {

// A wl_shm buffer filled with one color. Temporary: it only exists so the window has
// content before the EGL renderer (ROADMAP 1.2) replaces it.
// Not copyable or movable: the release listener keeps a pointer to this object.
class ShmBuffer {
public:
    // Throws std::runtime_error if the shared memory cannot be created.
    ShmBuffer(wl_shm* shm, std::int32_t width, std::int32_t height, std::uint32_t argb);
    ~ShmBuffer();

    ShmBuffer(const ShmBuffer&) = delete;
    ShmBuffer& operator=(const ShmBuffer&) = delete;
    ShmBuffer(ShmBuffer&&) = delete;
    ShmBuffer& operator=(ShmBuffer&&) = delete;

    [[nodiscard]] wl_buffer* native() const noexcept { return buffer_.get(); }

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

    // True from attach until the compositor sends wl_buffer.release.
    [[nodiscard]] bool busy() const noexcept { return busy_; }

    void mark_busy() noexcept { busy_ = true; }

private:
    static constexpr std::int32_t kBytesPerPixel = 4;  // WL_SHM_FORMAT_ARGB8888

    static void on_release(void* data, wl_buffer* buffer);

    std::int32_t width_;
    std::int32_t height_;
    std::size_t size_{static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) *
                      static_cast<std::size_t>(kBytesPerPixel)};
    void* pixels_{nullptr};
    bool busy_{false};
    WlPtr<wl_buffer> buffer_;
};

}  // namespace nordend::ui
