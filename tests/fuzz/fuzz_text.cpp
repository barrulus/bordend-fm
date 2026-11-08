// Fuzzes the text family (ROADMAP 3b) the way the base engine runs it: the first byte picks the
// limits (small ones too, to reach the cuts), the rest is the file; read from a memfd as the
// helper reads, and decoded as the start of a longer file.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sys/mman.h>
#include <unistd.h>

#include "nordend/formats/text/read.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }
    static constexpr std::array<nordend::text::Limits, 4> kLimits{{
        {},
        {.max_bytes = 8192, .max_line_bytes = 256, .max_lines = 40, .tab_width = 4},
        {.max_bytes = 64, .max_line_bytes = 3, .max_lines = 0, .tab_width = 8},
        {.max_bytes = 7, .max_line_bytes = 1, .max_lines = 2, .tab_width = 1},
    }};
    const nordend::text::Limits& limits = kLimits.at(data[0] % kLimits.size());
    const std::span<const std::uint8_t> file{data + 1, size - 1};
    static_cast<void>(nordend::formats::text::decode(file, true, limits));
    const int fd = ::memfd_create("fuzz", MFD_CLOEXEC);
    if (fd < 0) {
        return 0;
    }
    if (::write(fd, file.data(), file.size()) == static_cast<ssize_t>(file.size())) {
        static_cast<void>(nordend::formats::text::read(fd, limits));
    }
    ::close(fd);
    return 0;
}
