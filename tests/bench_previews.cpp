// Times finding a camera file's embedded preview, Nordend's own reader against the exiv2 code it
// replaces (ROADMAP 2e), and counts nothing else: run under `strace -c -f` with one run to see
// the system calls of either.
//
//   nordend-bench-previews ours|exiv2 RUNS LONG_SIDE FILE...
//
// Prints, per file, the best of RUNS (open, find, read the preview's bytes, close).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <span>
#include <string_view>
#include <unistd.h>

#include "exiv2_reference.hpp"

namespace image = nordend::formats::image;

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() < 5) {
        std::fprintf(stderr, "usage: nordend-bench-previews ours|exiv2 RUNS LONG_SIDE FILE...\n");
        return 2;
    }
    const bool exiv2 = std::string_view{args[1]} == "exiv2";
    const int runs = std::max(1, std::atoi(args[2]));
    const auto side = static_cast<std::uint32_t>(std::max(1, std::atoi(args[3])));
    for (const char* path : args.subspan(4)) {
        double best = 1e9;
        std::size_t bytes = 0;
        for (int run = 0; run < runs; ++run) {
            const auto start = std::chrono::steady_clock::now();
            const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                break;
            }
            const image::EmbeddedPreview preview =
                exiv2 ? image::reference::embedded_preview(fd, side)
                      : image::embedded_preview(fd, side);
            ::close(fd);
            best = std::min(best, std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - start)
                                      .count());
            bytes = preview.data.size();
        }
        std::printf("%-5s %8.2f ms %9zu bytes  %s\n", args[1], best, bytes, path);
    }
    return 0;
}
