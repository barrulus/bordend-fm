#include "shm_buffer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

namespace nordend::ui {
namespace {

// Closes the memfd once the pool holds its own reference.
class UniqueFd {
public:
    explicit UniqueFd(int fd) noexcept : fd_{fd} {}

    ~UniqueFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&&) = delete;
    UniqueFd& operator=(UniqueFd&&) = delete;

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_;
};

[[noreturn]] void throw_errno(const char* what) {
    throw std::runtime_error{std::string{what} + ": " + std::strerror(errno)};
}

}  // namespace

ShmBuffer::ShmBuffer(wl_shm* shm, std::int32_t width, std::int32_t height, std::uint32_t argb)
    : width_{width}, height_{height} {
    const UniqueFd fd{::memfd_create("nordend-shm", MFD_CLOEXEC)};
    if (fd.get() < 0) {
        throw_errno("memfd_create");
    }
    if (::ftruncate(fd.get(), static_cast<off_t>(size_)) < 0) {
        throw_errno("ftruncate");
    }
    pixels_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
    if (pixels_ == MAP_FAILED) {
        pixels_ = nullptr;
        throw_errno("mmap");
    }

    const std::span<std::uint32_t> pixels{static_cast<std::uint32_t*>(pixels_),
                                          size_ / sizeof(std::uint32_t)};
    std::ranges::fill(pixels, argb);

    const WlPtr<wl_shm_pool> pool{
        wl_shm_create_pool(shm, fd.get(), static_cast<std::int32_t>(size_))};
    buffer_.reset(wl_shm_pool_create_buffer(pool.get(), 0, width, height, width * kBytesPerPixel,
                                            WL_SHM_FORMAT_ARGB8888));

    static constexpr wl_buffer_listener kListener{.release = &ShmBuffer::on_release};
    wl_buffer_add_listener(buffer_.get(), &kListener, this);
}

ShmBuffer::~ShmBuffer() {
    buffer_.reset();
    if (pixels_ != nullptr) {
        ::munmap(pixels_, size_);
    }
}

void ShmBuffer::on_release(void* data, wl_buffer* /*buffer*/) {
    static_cast<ShmBuffer*>(data)->busy_ = false;
}

}  // namespace nordend::ui
