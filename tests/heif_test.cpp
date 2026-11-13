#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include "rummage/formats/image/decode.hpp"
#include "rummage/formats/image/load.hpp"
#include "rummage/isolate/helpers.hpp"
#include "rummage/picture/load.hpp"
#include "rummage/thumbs/cache.hpp"
#include "temp_dir.hpp"

namespace image = rummage::formats::image;
namespace picture = rummage::picture;
namespace isolate = rummage::isolate;
namespace thumbs = rummage::thumbs;
namespace fs = std::filesystem;

namespace {
const fs::path kData = fs::path{RUMMAGE_TEST_DATA} / "image";

std::vector<std::uint8_t> read(const fs::path& path) {
    std::ifstream file{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

std::array<int, 4> pixel(const picture::Image& image, std::size_t x, std::size_t y) {
    const auto at = (y * image.width + x) * 4;
    return {image.rgba.at(at), image.rgba.at(at + 1), image.rgba.at(at + 2), image.rgba.at(at + 3)};
}

isolate::HelperOptions helper_options() {
    isolate::HelperOptions options;
    const char* packaged = std::getenv("RUMMAGE_HEIF_HELPER");
    options.program = packaged ? packaged : RUMMAGE_PREVIEW_HELPER;
    options.max_helpers = 1;
    return options;
}

constexpr std::array kFixtures{"blocks.heic",
                               "blocks-rotated.heic",
                               "blocks-mirrored.heic",
                               "premultiplied.heic",
                               "crop.heic",
                               "grid.heic",
                               "p3.heic",
                               "p3-nclx.heic",
                               "pq.heic",
                               "hlg.heic"};
}  // namespace

TEST_CASE("HEIF: still brands are detected without claiming video or other BMFF formats") {
    auto bytes = read(kData / "blocks.heic");
    REQUIRE(bytes.size() > 16);
    for (const std::string_view brand : {"heic", "heix", "heim", "heis", "mif1"}) {
        std::copy(brand.begin(), brand.end(), bytes.begin() + 8);
        CHECK(image::sniff(std::span{bytes}.first(16)) == image::Format::heif);
    }
    for (const std::string_view brand : {"avif", "avis", "hevc", "hevs", "msf1", "crx ", "mp42"}) {
        std::copy(brand.begin(), brand.end(), bytes.begin() + 8);
        CHECK(image::sniff(bytes) == image::Format::unknown);
    }
    CHECK(image::sniff(std::span{bytes}.first(8)) == image::Format::unknown);
}

TEST_CASE("HEIF: primary image, straight alpha and container transformations") {
    const auto base = image::decode_file(kData / "blocks.heic");
    INFO(base.error);
    REQUIRE(base.error.empty());
    REQUIRE(base.image.width == 48);
    REQUIRE(base.image.height == 32);
    CHECK(pixel(base.image, 8, 8) == std::array{255, 0, 0, 255});
    CHECK(pixel(base.image, 8, 24) == std::array{255, 255, 255, 128});
    for (const bool orient : {true, false}) {
        const auto rotated =
            image::decode_file(kData / "blocks-rotated.heic", {.apply_orientation = orient});
        INFO(rotated.error);
        REQUIRE(rotated.error.empty());
        CHECK(rotated.orientation == 1);  // already upright, including through load()
        CHECK(rotated.image.width == 32);
        CHECK(rotated.image.height == 48);
        CHECK(rotated.image.rgba == picture::apply_orientation(base.image, 6).rgba);
    }
    const auto mirrored = image::decode_file(kData / "blocks-mirrored.heic");
    REQUIRE(mirrored.error.empty());
    CHECK(mirrored.image.rgba == picture::apply_orientation(base.image, 2).rgba);
    const auto premultiplied = image::decode_file(kData / "premultiplied.heic");
    REQUIRE(premultiplied.error.empty());
    CHECK(pixel(premultiplied.image, 8, 8) == std::array{255, 0, 0, 128});
    CHECK(pixel(premultiplied.image, 24, 8) == std::array{0, 0, 0, 0});
    const auto crop = image::decode_file(kData / "crop.heic");
    REQUIRE(crop.error.empty());
    CHECK(crop.image.width == 47);
    CHECK(crop.image.height == 31);
    CHECK(pixel(crop.image, 46, 30) == std::array{0, 0, 255, 255});
}

TEST_CASE("HEIF: large tiled primary image is assembled and cropped before scaling") {
    const auto grid = image::decode_file(kData / "grid.heic");
    INFO(grid.error);
    REQUIRE(grid.error.empty());
    REQUIRE(grid.image.width == 1537);
    REQUIRE(grid.image.height == 1025);
    CHECK(pixel(grid.image, 100, 100) == std::array{255, 0, 0, 255});
    CHECK(pixel(grid.image, 100, 1024) == std::array{255, 255, 0, 255});
    CHECK(pixel(grid.image, 1536, 1024) == std::array{0, 255, 0, 255});
    const auto small =
        image::load({.path = kData / "grid.heic", .max_width = 128, .max_height = 128});
    CHECK(small.failure == picture::Failure::none);
    CHECK(small.full_width == 1537);
    CHECK(small.full_height == 1025);
    CHECK(small.image.width == 128);
    CHECK(small.image.height == 85);
}

TEST_CASE("HEIF: Display P3 ICC becomes sRGB while retaining alpha") {
    for (const char* name : {"p3.heic", "p3-nclx.heic"}) {
        const auto p3 = image::decode_file(kData / name);
        INFO(p3.error);
        REQUIRE(p3.error.empty());
        REQUIRE(p3.image.width == 32);
        const auto value = pixel(p3.image, 16, 16);
        // Independent P3 -> XYZ D65 -> sRGB matrix/transfer reference, rounded to 8 bits.
        CHECK(std::abs(value[0] - 193) <= 2);
        CHECK(std::abs(value[1] - 95) <= 2);
        CHECK(std::abs(value[2] - 47) <= 2);
        CHECK(value[3] == 128);
    }
}

TEST_CASE("HEIF: PQ and HLG produce finite monotonic SDR pixels") {
    for (const char* name : {"pq.heic", "hlg.heic"}) {
        const auto hdr = image::decode_file(kData / name);
        INFO(std::string{name}, " ", hdr.error);
        REQUIRE(hdr.error.empty());
        REQUIRE(hdr.image.width == 64);
        REQUIRE(hdr.image.height == 32);
        int previous = -1;
        for (std::size_t x : {8U, 24U, 40U, 56U}) {
            const auto value = pixel(hdr.image, x, 16);
            CHECK(value[0] >= previous);
            CHECK(std::abs(value[0] - value[1]) <= 2);
            CHECK(std::abs(value[0] - value[2]) <= 2);
            CHECK(value[3] == 255);
            previous = value[0];
        }
        CHECK(pixel(hdr.image, 8, 16)[0] <= 2);
        CHECK(pixel(hdr.image, 56, 16)[0] >= 253);
        // PQ 100 nits / 203-nit white -> sRGB 186. HLG at this code -> about 139.
        CHECK(std::abs(pixel(hdr.image, 24, 16)[0] -
                       (std::string_view{name} == "pq.heic" ? 186 : 139)) <= 3);
    }
}

TEST_CASE("HEIF: an unshipped codec reports unsupported without stopping the helper") {
    const auto path = kData / "unsupported-codec.heif";
    const auto decoded = image::decode_file(path);
    CHECK(decoded.problem == image::Problem::unknown_format);
    CHECK(decoded.image.empty());
    CHECK_FALSE(decoded.error.empty());
    isolate::Helpers helpers{helper_options()};
    const auto loaded = helpers.load({.path = path, .max_width = 128, .max_height = 128});
    CHECK(loaded.failure == picture::Failure::unknown_format);
    CHECK_FALSE(loaded.detail.empty());
    CHECK(helpers.load({.path = kData / "blocks.heic", .max_width = 128, .max_height = 128})
              .failure == picture::Failure::none);
}

TEST_CASE("HEIF: pixel limits, truncation and malformed boxes fail cleanly") {
    for (const char* name : kFixtures) {
        const auto large = image::decode_file(kData / name, {.max_pixels = 100});
        INFO(std::string{name}, " ", large.error);
        CHECK(large.problem == image::Problem::too_large);
        CHECK(large.image.empty());
    }
    CHECK(image::decode_file(kData / "blocks.heic", {.max_pixels = 0}).problem ==
          image::Problem::too_large);
    const auto original = read(kData / "blocks.heic");
    for (std::size_t length : {16U, 32U, 64U, 128U}) {
        const auto cut = image::decode(std::span{original}.first(length));
        CHECK_FALSE(cut.error.empty());
        CHECK(cut.image.empty());
    }
    auto corrupt = original;
    // An ftyp box extending beyond the file.
    std::fill_n(corrupt.begin(), 4, 0x7f);
    CHECK_FALSE(image::decode(corrupt).error.empty());
    CHECK(image::decode(corrupt).image.empty());
}

TEST_CASE("HEIF: actual sandbox helper decodes every fixture and survives malformed input") {
    isolate::Helpers helpers{helper_options()};
    for (const char* name : kFixtures) {
        for (const std::uint32_t side : {128U, 2048U}) {
            const picture::LoadRequest request{
                .path = kData / name, .max_width = side, .max_height = side};
            const auto there = helpers.load(request);
            const auto here = image::load(request);
            INFO(std::string{name}, " ", side, " ", there.detail);
            REQUIRE(there.failure == picture::Failure::none);
            CHECK(there.full_width == here.full_width);
            CHECK(there.full_height == here.full_height);
            CHECK(there.image.width == here.image.width);
            CHECK(there.image.height == here.image.height);
            CHECK(there.image.rgba == here.image.rgba);
        }
    }
    rummage::test::TempDir temp;
    const auto broken = temp.path / "broken.heic";
    const auto bytes = read(kData / "blocks.heic");
    std::ofstream{broken, std::ios::binary}.write(reinterpret_cast<const char*>(bytes.data()), 128);
    const auto bad = helpers.load({.path = broken, .max_width = 128, .max_height = 128});
    CHECK(bad.failure == picture::Failure::damaged);
    CHECK_FALSE(bad.detail.empty());
    CHECK(helpers.load({.path = kData / "blocks.heic", .max_width = 128, .max_height = 128})
              .failure == picture::Failure::none);
    CHECK(helpers.running() == 1);
}

TEST_CASE("HEIF: thumbnail pipeline uses the helper then reads its cached PNG") {
    auto helpers = std::make_shared<isolate::Helpers>(helper_options());
    picture::set_loader(
        [helpers](const picture::LoadRequest& request) { return helpers->load(request); });

    struct Restore {
        ~Restore() {
            picture::set_loader(
                [](const picture::LoadRequest& request) { return image::load(request); });
        }
    } restore;

    rummage::test::TempDir temp;
    const thumbs::Cache cache{temp.path / "thumbnails", "rummage-heif-test"};
    const auto path = kData / "grid.heic";
    const auto made = cache.get(path, thumbs::Size::normal);
    INFO(made.error);
    REQUIRE(made.source == thumbs::Source::decoded);
    CHECK(made.image.width == 128);
    CHECK(made.image.height == 85);
    const auto cached = cache.get(path, thumbs::Size::normal);
    CHECK(cached.source == thumbs::Source::cache);
    CHECK(cached.image.rgba == made.image.rgba);
}

// Optional private/downloaded phone corpus: every HEIC/HEIF in RUMMAGE_HEIF_SAMPLES.
// Kept out of the source tree to avoid redistributing other people's photos.
TEST_CASE("HEIF: external phone corpus through the sandbox" *
          doctest::skip(std::getenv("RUMMAGE_HEIF_SAMPLES") == nullptr)) {
    const char* folder = std::getenv("RUMMAGE_HEIF_SAMPLES");
    REQUIRE(folder != nullptr);
    isolate::Helpers helpers{helper_options()};
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (!entry.is_regular_file() || image::sniff(read(entry.path())) != image::Format::heif) {
            continue;
        }
        const auto loaded =
            helpers.load({.path = entry.path(), .max_width = 2048, .max_height = 2048});
        INFO(entry.path().string(), " ", loaded.detail);
        CHECK(loaded.failure == picture::Failure::none);
        CHECK_FALSE(loaded.image.empty());
        ++count;
    }
    CHECK(count > 0);
}
