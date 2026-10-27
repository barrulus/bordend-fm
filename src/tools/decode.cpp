// Image decoding check and benchmark (ROADMAP 5.1). Not installed.
//
//   nordend-decode [--runs N] [--stored] [--ppm OUT.ppm] FILE...
//
// For each file prints the format, the size (after the EXIF orientation), the orientation, the
// time to read the file and the median time to decode it over N runs (default 5; the first run
// is not counted when N > 1). --stored leaves the pixels as stored (no EXIF turn). --ppm writes the
// last image decoded, over a mid gray where it is transparent, to look at it (magick OUT.ppm
// OUT.png).
#include "nordend/image/decode.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace image = nordend::image;
using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool write_ppm(const std::filesystem::path& path, const image::Image& picture) {
    std::ofstream out{path, std::ios::binary};
    out << "P6\n" << picture.width << ' ' << picture.height << "\n255\n";
    std::vector<char> row(std::size_t{picture.width} * 3);
    for (std::size_t y = 0; y < picture.height; ++y) {
        for (std::size_t x = 0; x < picture.width; ++x) {
            const std::size_t at = ((y * picture.width) + x) * 4;
            const unsigned alpha = picture.rgba[at + 3];
            for (std::size_t c = 0; c < 3; ++c) {
                const unsigned value =
                    ((picture.rgba[at + c] * alpha) + (128U * (255U - alpha))) / 255U;
                row[(x * 3) + c] = static_cast<char>(value);
            }
        }
        out.write(row.data(), static_cast<std::streamsize>(row.size()));
    }
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
    std::span args{argv, static_cast<std::size_t>(argc)};
    args = args.subspan(1);
    int runs = 5;
    std::filesystem::path ppm;
    image::DecodeOptions options;
    while (!args.empty()) {
        const std::string_view option{args[0]};
        if (option == "--stored") {
            options.apply_orientation = false;
            args = args.subspan(1);
            continue;
        }
        if (args.size() < 2) {
            break;
        }
        if (option == "--runs") {
            runs = std::max(1, std::atoi(args[1]));
        } else if (option == "--ppm") {
            ppm = args[1];
        } else {
            break;
        }
        args = args.subspan(2);
    }
    if (args.empty()) {
        std::fprintf(stderr,
                     "usage: nordend-decode [--runs N] [--stored] [--ppm OUT.ppm] FILE...\n");
        return 2;
    }
    int failures = 0;
    image::Image last;
    for (const char* arg : args) {
        const std::filesystem::path path{arg};
        const auto read_start = Clock::now();
        std::vector<std::uint8_t> data(std::filesystem::file_size(path));
        std::ifstream file{path, std::ios::binary};
        file.read(reinterpret_cast<char*>(data.data()),  // NOLINT: bytes to char for read
                  static_cast<std::streamsize>(data.size()));
        const double read_ms = ms_since(read_start);

        std::vector<double> times;
        image::Decoded decoded;
        for (int run = 0; run < runs; ++run) {
            const auto start = Clock::now();
            decoded = image::decode(data, options);
            times.push_back(ms_since(start));
        }
        if (!decoded.error.empty()) {
            std::printf("%s: %s (%s)\n", arg, decoded.error.c_str(),
                        std::string{image::format_name(decoded.format)}.c_str());
            ++failures;
            continue;
        }
        if (times.size() > 1) {
            times.erase(times.begin());  // the first run warms the caches up
        }
        std::ranges::sort(times);
        const double median = times[times.size() / 2];
        const double megapixels =
            static_cast<double>(decoded.image.width) * decoded.image.height / 1e6;
        std::printf(
            "%s: %s %ux%u (%.1f MP), orientation %u, %.1f MB read in %.1f ms, decoded in "
            "%.1f ms (%.0f MP/s)\n",
            arg, std::string{image::format_name(decoded.format)}.c_str(), decoded.image.width,
            decoded.image.height, megapixels, unsigned{decoded.orientation},
            static_cast<double>(data.size()) / 1e6, read_ms, median,
            megapixels / (median / 1000.0));
        last = std::move(decoded.image);
    }
    if (!ppm.empty() && !last.empty() && !write_ppm(ppm, last)) {
        std::fprintf(stderr, "cannot write %s\n", ppm.c_str());
        return 1;
    }
    return failures == 0 ? 0 : 1;
}
