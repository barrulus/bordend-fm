#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/image/decode.hpp"

namespace image = nordend::image;
using image::Format;
using image::Image;

namespace {

const std::filesystem::path kData = std::filesystem::path{NORDEND_TEST_DATA} / "image";

struct Rgba {
    int r, g, b, a;
};

// The blocks fixtures (tests/data/image/make.sh): 48x32, six 16x16 blocks.
//   red    green  blue
//   white* black  yellow   (* alpha 128 where the format has alpha)
constexpr std::array<Rgba, 6> kBlocks{{{255, 0, 0, 255},
                                       {0, 255, 0, 255},
                                       {0, 0, 255, 255},
                                       {255, 255, 255, 128},
                                       {0, 0, 0, 255},
                                       {255, 255, 0, 255}}};

Rgba pixel(const Image& picture, std::size_t x, std::size_t y) {
    const std::size_t at = ((y * picture.width) + x) * 4;
    return {picture.rgba[at], picture.rgba[at + 1], picture.rgba[at + 2], picture.rgba[at + 3]};
}

// The center of each block matches within tolerance (JPEG is lossy); with alpha false the
// half-transparent block is checked as white over white (the opaque formats).
void check_blocks(const Image& picture, int tolerance, bool alpha) {
    REQUIRE(picture.width == 48);
    REQUIRE(picture.height == 32);
    for (std::size_t block = 0; block < kBlocks.size(); ++block) {
        Rgba expected = kBlocks.at(block);
        if (!alpha) {
            expected.a = 255;
        }
        const Rgba actual = pixel(picture, ((block % 3) * 16) + 8, ((block / 3) * 16) + 8);
        INFO("block " << block);
        CHECK(std::abs(actual.r - expected.r) <= tolerance);
        CHECK(std::abs(actual.g - expected.g) <= tolerance);
        CHECK(std::abs(actual.b - expected.b) <= tolerance);
        CHECK(actual.a == expected.a);
    }
}

// A width x height image whose pixel i has red = i.
Image numbered(std::uint32_t width, std::uint32_t height) {
    Image picture{.width = width, .height = height, .rgba = {}};
    picture.rgba.resize(std::size_t{width} * height * 4);
    for (std::size_t i = 0; i < std::size_t{width} * height; ++i) {
        picture.rgba[i * 4] = static_cast<std::uint8_t>(i);
        picture.rgba[(i * 4) + 3] = 255;
    }
    return picture;
}

std::vector<int> reds(const Image& picture) {
    std::vector<int> out;
    for (std::size_t i = 0; i < picture.rgba.size(); i += 4) {
        out.push_back(picture.rgba[i]);
    }
    return out;
}

}  // namespace

TEST_CASE("image: formats are recognized by their first bytes") {
    CHECK(image::decode_file(kData / "blocks.png").format == Format::png);
    CHECK(image::decode_file(kData / "blocks.jpg").format == Format::jpeg);
    CHECK(image::decode_file(kData / "blocks.gif").format == Format::gif);
    CHECK(image::decode_file(kData / "blocks.webp").format == Format::webp);
    CHECK(image::decode_file(kData / "blocks.jxl").format == Format::jxl);
    const std::string text = "just some text, not an image";
    const std::vector<std::uint8_t> bytes(text.begin(), text.end());
    CHECK(image::sniff(bytes) == Format::unknown);
    CHECK(image::sniff({}) == Format::unknown);
}

TEST_CASE("image: every format decodes to the same pixels") {
    for (const char* name : {"blocks.png", "blocks.webp", "blocks.jxl"}) {
        INFO(name);
        const image::Decoded decoded = image::decode_file(kData / name);
        REQUIRE(decoded.error.empty());
        check_blocks(decoded.image, 0, true);
    }
    const image::Decoded gif = image::decode_file(kData / "blocks.gif");
    REQUIRE(gif.error.empty());
    check_blocks(gif.image, 0, false);
    const image::Decoded jpeg = image::decode_file(kData / "blocks.jpg");
    REQUIRE(jpeg.error.empty());
    check_blocks(jpeg.image, 6, false);
    CHECK(jpeg.orientation == 1);
    // An animated WebP: its first frame.
    const image::Decoded animated = image::decode_file(kData / "blocks-anim.webp");
    REQUIRE(animated.error.empty());
    check_blocks(animated.image, 0, true);
}

TEST_CASE("image: the EXIF orientation of a JPEG turns the pixels, unless asked not to") {
    const image::Decoded turned = image::decode_file(kData / "blocks-orient6.jpg");
    REQUIRE(turned.error.empty());
    CHECK(turned.orientation == 6);
    REQUIRE(turned.image.width == 32);
    REQUIRE(turned.image.height == 48);
    // A quarter clockwise: the left column, bottom up, is now the top row.
    CHECK(pixel(turned.image, 8, 8).r > 240);  // white
    CHECK(pixel(turned.image, 8, 8).b > 240);
    CHECK(pixel(turned.image, 24, 8).r > 240);  // red
    CHECK(pixel(turned.image, 24, 8).g < 16);
    CHECK(pixel(turned.image, 24, 40).b > 240);  // blue at the bottom right
    CHECK(pixel(turned.image, 24, 40).r < 16);

    const image::Decoded stored =
        image::decode_file(kData / "blocks-orient6.jpg", {.apply_orientation = false});
    CHECK(stored.orientation == 6);
    check_blocks(stored.image, 6, false);
}

TEST_CASE("image: the eight orientations") {
    const Image picture = numbered(3, 2);  // 0 1 2 / 3 4 5
    const auto turned = [&](std::uint8_t orientation) {
        return image::apply_orientation(picture, orientation);
    };
    CHECK(reds(turned(1)) == std::vector<int>{0, 1, 2, 3, 4, 5});
    CHECK(reds(turned(2)) == std::vector<int>{2, 1, 0, 5, 4, 3});
    CHECK(reds(turned(3)) == std::vector<int>{5, 4, 3, 2, 1, 0});
    CHECK(reds(turned(4)) == std::vector<int>{3, 4, 5, 0, 1, 2});
    CHECK(reds(turned(5)) == std::vector<int>{0, 3, 1, 4, 2, 5});
    CHECK(reds(turned(6)) == std::vector<int>{3, 0, 4, 1, 5, 2});
    CHECK(reds(turned(7)) == std::vector<int>{5, 2, 4, 1, 3, 0});
    CHECK(reds(turned(8)) == std::vector<int>{2, 5, 1, 4, 0, 3});
    CHECK(turned(6).width == 2);
    CHECK(turned(6).height == 3);
    CHECK(reds(turned(9)) == reds(picture));  // not an orientation: as it is
}

TEST_CASE("image: EXIF orientation in both byte orders, and garbage") {
    // TIFF header, IFD0 at 8 with one entry: tag 0x0112, SHORT, count 1, value 8.
    const std::vector<std::uint8_t> little{'I', 'I', 42, 0, 8, 0, 0, 0, 1, 0, 0x12, 0x01,
                                           3,   0,   1,  0, 0, 0, 8, 0, 0, 0, 0,    0};
    const std::vector<std::uint8_t> big{'M', 'M', 0, 42, 0, 0, 0, 8, 0, 1, 0x01, 0x12,
                                        0,   3,   0, 0,  0, 1, 0, 3, 0, 0, 0,    0};
    CHECK(image::exif_orientation(little) == 8);
    CHECK(image::exif_orientation(big) == 3);
    std::vector<std::uint8_t> prefixed{'E', 'x', 'i', 'f', 0, 0};
    prefixed.insert(prefixed.end(), big.begin(), big.end());
    CHECK(image::exif_orientation(prefixed) == 3);

    std::vector<std::uint8_t> bad_offset = little;
    bad_offset[4] = 200;  // IFD past the end
    CHECK(image::exif_orientation(bad_offset) == 1);
    const std::vector<std::uint8_t> cut(little.begin(), little.begin() + 15);
    CHECK(image::exif_orientation(cut) == 1);
    CHECK(image::exif_orientation({}) == 1);
}

TEST_CASE("image: bad input fails with a reason, never a crash") {
    const image::Decoded truncated = image::decode_file(kData / "truncated.jpg");
    CHECK_FALSE(truncated.error.empty());
    CHECK(truncated.image.empty());

    const image::Decoded missing = image::decode_file(kData / "no-such-file.png");
    CHECK_FALSE(missing.error.empty());

    const std::vector<std::uint8_t> header_only{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    const image::Decoded png = image::decode(header_only);
    CHECK(png.format == Format::png);
    CHECK_FALSE(png.error.empty());

    for (const char* name : {"blocks.png", "blocks.jpg", "blocks.webp", "blocks.jxl"}) {
        INFO(name);
        const image::Decoded too_large =
            image::decode_file(kData / name, {.max_pixels = 1000});  // 48x32 = 1536
        CHECK_FALSE(too_large.error.empty());
        CHECK(too_large.image.empty());
    }
}
