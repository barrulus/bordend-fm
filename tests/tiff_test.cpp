#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/formats/image/decode.hpp"
#include "nordend/formats/image/load.hpp"
#include "nordend/picture/wuffs.hpp"
#include "temp_dir.hpp"

namespace fs = std::filesystem;
namespace image = nordend::formats::image;
namespace picture = nordend::picture;
using nordend::test::TempDir;

namespace {

const fs::path kTiff = fs::path{NORDEND_TEST_DATA} / "image" / "tiff";

std::vector<std::uint8_t> read_bytes(const fs::path& path) {
    std::ifstream in{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{in}, {}};
}

void write_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out{path, std::ios::binary};
    out.write(reinterpret_cast<const char*>(bytes.data()),  // NOLINT: bytes to chars
              static_cast<std::streamsize>(bytes.size()));
}

// The largest difference of any channel between two pictures of the same size (the colour of
// fully transparent pixels does not count); 256 when they differ in size.
int largest_difference(const picture::Image& a, const picture::Image& b) {
    if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size()) {
        return 256;
    }
    int largest = 0;
    for (std::size_t i = 0; i < a.rgba.size(); ++i) {
        const std::size_t alpha = i - (i % 4) + 3;
        if (a.rgba[alpha] == 0 && b.rgba[alpha] == 0) {
            continue;
        }
        largest = std::max(largest, std::abs(int{a.rgba[i]} - int{b.rgba[i]}));
    }
    return largest;
}

}  // namespace

// Every fixture against ImageMagick's own decoding of it (libtiff inside): tests/data/image/
// tiff/make.sh writes each NAME.tif with its NAME.png.
TEST_CASE("tiff: every kind of TIFF decodes exactly as libtiff does") {
    int checked = 0;
    for (const auto& entry : fs::directory_iterator{kTiff}) {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".tif" || name.starts_with("broken")) {
            continue;
        }
        INFO(name);
        const image::Decoded ours = image::decode_file(entry.path(), {.apply_orientation = false});
        REQUIRE(ours.error.empty());
        CHECK(ours.format == image::Format::tiff);
        fs::path reference = entry.path();
        reference.replace_extension(".png");
        std::string error;
        const auto theirs = picture::decode_png(read_bytes(reference), error);
        REQUIRE(theirs.has_value());
        CHECK(largest_difference(ours.image, theirs.value_or(picture::Image{})) == 0);
        ++checked;
    }
    CHECK(checked == 36);
    // The first page of two; the Orientation tag reported, turned when asked.
    CHECK(image::decode_file(kTiff / "two-pages.tif").image.width == 37);
    const image::Decoded turned = image::decode_file(kTiff / "orient6.tif");
    CHECK(turned.orientation == 6);
    CHECK(turned.image.width == 29);
}

TEST_CASE("tiff: plain TIFFs are shown without exiv2, camera raws keep their previews") {
    const std::uint64_t before = image::embedded_preview_reads();
    const image::Loaded big = image::load(
        {.path = kTiff / "rgb16-zip-predictor.tif", .max_width = 1024, .max_height = 1024});
    REQUIRE(big.failure == image::Failure::none);
    CHECK_FALSE(big.from_preview);
    CHECK(big.full_width == 37);
    const image::Loaded turned =
        image::load({.path = kTiff / "orient6.tif", .max_width = 1024, .max_height = 1024});
    CHECK(turned.image.width == 29);  // upright
    CHECK(turned.full_width == 29);
    CHECK(image::embedded_preview_reads() == before);
}

TEST_CASE("tiff: broken TIFFs are damaged, never a crash") {
    const TempDir dir;
    const std::vector<std::uint8_t> good = read_bytes(kTiff / "rgb8-none.tif");
    const auto load = [&](const std::vector<std::uint8_t>& bytes) {
        write_bytes(dir.path / "test.tif", bytes);
        return image::load({.path = dir.path / "test.tif", .max_width = 64, .max_height = 64});
    };
    REQUIRE(load(good).failure == image::Failure::none);
    CHECK(image::load({.path = kTiff / "broken-truncated.tif", .max_width = 64, .max_height = 64})
              .failure == image::Failure::damaged);
    // Found by the fuzzer: a TileWidth slipped into a TIFF of strips asked for gigabytes of tiles
    // (4.6 s); now refused at once.
    const auto start = std::chrono::steady_clock::now();
    CHECK(image::load({.path = kTiff / "broken-tile-tag.tif", .max_width = 64, .max_height = 64})
              .failure == image::Failure::damaged);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds{200});
    // Found by the fuzzer of the whole family: not a TIFF this reader shows, so its stored
    // previews are asked of exiv2, whose safe arithmetic throws std::overflow_error. A plain
    // reason, never an exception out of load().
    CHECK(image::load(
              {.path = kTiff / "broken-exiv2-overflow.tif", .max_width = 64, .max_height = 64})
              .failure != image::Failure::none);
    // Every single byte of the header and the IFD set to 0xFF in turn: never a crash, and a
    // picture or a plain reason each time.
    const std::vector<std::uint8_t> lzw = read_bytes(kTiff / "rgb8-lzw.tif");
    for (std::size_t i = 0; i < lzw.size(); ++i) {
        std::vector<std::uint8_t> bent = lzw;
        bent[i] = 0xFF;
        const image::Loaded loaded = load(bent);
        const bool shown = loaded.failure == image::Failure::none;
        CHECK(shown != loaded.image.empty());
    }
    // A size beyond the limit is refused before anything is allocated.
    std::vector<std::uint8_t> huge = good;
    const std::size_t width_entry = [&] {
        // Find ImageWidth (256, SHORT or LONG) in the first IFD, little-endian.
        const std::uint32_t ifd = std::uint32_t{good[4]} | (std::uint32_t{good[5]} << 8) |
                                  (std::uint32_t{good[6]} << 16) | (std::uint32_t{good[7]} << 24);
        const std::size_t count = std::size_t{good[ifd]} | (std::size_t{good[ifd + 1]} << 8);
        for (std::size_t e = 0; e < count; ++e) {
            const std::size_t at = ifd + 2 + (e * 12);
            if ((good[at] | (good[at + 1] << 8)) == 256) {
                return at;
            }
        }
        return std::size_t{0};
    }();
    REQUIRE(width_entry != 0);
    huge[width_entry + 2] = 4;  // LONG
    huge[width_entry + 4] = 1;  // count 1
    huge[width_entry + 8] = 0x00;
    huge[width_entry + 9] = 0x00;
    huge[width_entry + 10] = 0x00;
    huge[width_entry + 11] = 0x01;  // 0x01000000: 16 777 216 wide, past the 2^20 limit
    CHECK(load(huge).failure == image::Failure::damaged);
}
