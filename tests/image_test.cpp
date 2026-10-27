#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/image/decode.hpp"
#include "nordend/image/photo.hpp"
#include "nordend/image/png.hpp"
#include "nordend/image/preview.hpp"
#include "nordend/image/scale.hpp"

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

TEST_CASE("image: scale_to_fit keeps proportions, averages, and never enlarges") {
    Image gray{.width = 300, .height = 200, .rgba = {}};
    gray.rgba.resize(std::size_t{300} * 200 * 4);
    for (std::size_t i = 0; i < gray.rgba.size(); i += 4) {
        const bool white = (i / 4) % 2 == 0;  // alternate black and white columns
        gray.rgba[i] = gray.rgba[i + 1] = gray.rgba[i + 2] = white ? 255 : 0;
        gray.rgba[i + 3] = 255;
    }
    const Image small = image::scale_to_fit(gray, 128, 128);
    CHECK(small.width == 128);
    CHECK(small.height == 85);
    // Stripes average to mid gray, through the halving and the area filter alike.
    CHECK(std::abs(pixel(small, 64, 40).r - 128) <= 2);
    CHECK(pixel(small, 64, 40).a == 255);
    const Image tiny = image::scale_to_fit(gray, 7, 1000);  // ratio 1:42.9, through halvings
    CHECK(tiny.width == 7);
    CHECK(tiny.height == 5);
    CHECK(std::abs(pixel(tiny, 3, 2).r - 128) <= 2);

    const Image same = image::scale_to_fit(gray, 1000, 1000);
    CHECK(same.width == 300);
    CHECK(same.height == 200);
}

TEST_CASE("image: transparent pixels do not darken what they are averaged with") {
    // Red, and transparent black: the average is half-transparent red, not dark red.
    Image half{.width = 2, .height = 1, .rgba = {255, 0, 0, 255, 0, 0, 0, 0}};
    const Image one = image::scale_to_fit(half, 1, 1);
    REQUIRE(one.width == 1);
    CHECK(pixel(one, 0, 0).r == 255);
    CHECK(std::abs(pixel(one, 0, 0).a - 128) <= 1);
}

TEST_CASE("image: PNG written with text chunks reads back the same") {
    const image::Decoded blocks = image::decode_file(kData / "blocks.png");
    REQUIRE(blocks.error.empty());
    const image::PngText text{{"Thumb::URI", "file:///a%20b.jpg"}, {"Thumb::MTime", "1700000000"}};
    const std::vector<std::uint8_t> png = image::encode_png(blocks.image, text);
    REQUIRE_FALSE(png.empty());
    CHECK(image::png_text(png) == text);
    const image::Decoded back = image::decode(png);
    REQUIRE(back.error.empty());
    CHECK(back.image.rgba == blocks.image.rgba);  // lossless, alpha included
    CHECK(image::png_text(std::vector<std::uint8_t>{1, 2, 3}).empty());
}

TEST_CASE("image: an embedded EXIF thumbnail is found, with the main image's orientation") {
    const image::EmbeddedPreview preview =
        image::embedded_preview(kData / "with-thumbnail.jpg", 16);
    CHECK(preview.error.empty());
    REQUIRE_FALSE(preview.data.empty());
    CHECK_FALSE(preview.tiff);
    CHECK(preview.width == 192);
    CHECK(preview.height == 128);
    CHECK(preview.orientation == 6);
    const image::Decoded decoded = image::decode_preview(preview);
    REQUIRE(decoded.error.empty());
    REQUIRE(decoded.image.width == 192);
    CHECK(pixel(decoded.image, 32, 32).r > 240);  // red block, top left
    CHECK(pixel(decoded.image, 32, 32).g < 16);

    // None in a plain file, and an error only when the file cannot be read.
    const image::EmbeddedPreview none = image::embedded_preview(kData / "blocks.png", 16);
    CHECK(none.data.empty());
    CHECK(none.error.empty());
    const image::EmbeddedPreview missing = image::embedded_preview(kData / "nothing.jpg", 16);
    CHECK(missing.data.empty());
    CHECK_FALSE(missing.error.empty());
}

TEST_CASE("image: an uncompressed TIFF preview is read, strip by strip") {
    const image::EmbeddedPreview preview = image::embedded_preview(kData / "tiff-preview.tif", 16);
    CHECK(preview.error.empty());
    REQUIRE_FALSE(preview.data.empty());
    CHECK(preview.tiff);
    CHECK(preview.width == 96);
    CHECK(preview.height == 64);
    CHECK_FALSE(preview.crop);
    const image::Decoded decoded = image::decode_preview(preview);
    REQUIRE(decoded.error.empty());
    REQUIRE(decoded.image.width == 96);
    REQUIRE(decoded.image.height == 64);
    // Each block is 32x32: the centers, and the last rows (in the short last strip).
    for (std::size_t block = 0; block < kBlocks.size(); ++block) {
        const Rgba expected = kBlocks.at(block);
        const Rgba actual = pixel(decoded.image, ((block % 3) * 32) + 16, ((block / 3) * 32) + 16);
        INFO("block " << block);
        CHECK(actual.r == expected.r);
        CHECK(actual.g == expected.g);
        CHECK(actual.b == expected.b);
        CHECK(actual.a == 255);
    }
    CHECK(pixel(decoded.image, 80, 63).r == 255);  // yellow, bottom right
    CHECK(pixel(decoded.image, 80, 63).b == 0);

    // A cut TIFF fails with a reason; a crop that does not fit leaves the preview whole.
    image::EmbeddedPreview cut = preview;
    cut.data.resize(cut.data.size() / 2);
    CHECK_FALSE(image::decode_preview(cut).error.empty());
    image::EmbeddedPreview cropped = preview;
    cropped.crop = image::PreviewCrop{.x = 32, .y = 0, .width = 64, .height = 32};
    const image::Decoded part = image::decode_preview(cropped);
    REQUIRE(part.image.width == 64);
    REQUIRE(part.image.height == 32);
    CHECK(pixel(part.image, 16, 16).g == 255);  // green, now top left
    CHECK(pixel(part.image, 16, 16).r == 0);
    cropped.crop = image::PreviewCrop{.x = 64, .y = 0, .width = 64, .height = 32};
    CHECK(image::decode_preview(cropped).image.width == 96);
}

TEST_CASE("image: a photo's shot from EXIF, upright size, and its darktable rating") {
    const image::PhotoInfo shot = image::read_photo_info(kData / "shot.jpg");
    CHECK(shot.width == 32);  // 48x32 stored, orientation 6
    CHECK(shot.height == 48);
    CHECK(shot.camera == "OM-1");  // the maker's long name left out
    CHECK(shot.lens == "OLYMPUS M.12-100mm F4.0");
    CHECK(shot.exposure_time == "1/2000 s");
    CHECK(shot.aperture == "F5.6");
    CHECK(shot.iso == "200");
    CHECK(shot.focal_length == "25.0 mm");
    CHECK(shot.taken == "2026-08-31 16:34");
    CHECK(shot.rating == std::optional<int>{3});  // from shot.jpg.xmp

    const image::PhotoInfo plain = image::read_photo_info(kData / "blocks.png");
    CHECK(plain.width == 48);
    CHECK(plain.camera.empty());
    CHECK_FALSE(plain.rating);
    const image::PhotoInfo missing = image::read_photo_info(kData / "nothing.jpg");
    CHECK(missing.width == 0);
}

// Raw files of other cameras, from rawsamples.ch (not in the repository): the folder named by
// NORDEND_RAW_SAMPLES, when it is set. The test needs the files these checks name.
namespace {
const std::filesystem::path kRawSamples = [] {
    const char* folder = std::getenv("NORDEND_RAW_SAMPLES");
    return std::filesystem::path{folder != nullptr ? folder : ""};
}();
}  // namespace

TEST_CASE("image: raw samples of other cameras" *
          doctest::skip(kRawSamples.empty() || !std::filesystem::is_directory(kRawSamples))) {
    for (const auto& entry : std::filesystem::directory_iterator{kRawSamples}) {
        INFO(entry.path().filename().string());
        for (const std::uint32_t side : {128U, 256U, 1024U, 2880U}) {
            INFO("side " << side);
            const image::EmbeddedPreview preview = image::embedded_preview(entry.path(), side);
            CHECK(preview.error.empty());
            REQUIRE_FALSE(preview.data.empty());
            const image::Decoded decoded = image::decode_preview(preview);
            REQUIRE(decoded.error.empty());
            CHECK(decoded.image.width == preview.width);
            CHECK(decoded.image.height == preview.height);
        }
    }
    const auto preview = [&](const char* name, std::uint32_t side) {
        return image::embedded_preview(kRawSamples / name, side);
    };
    // Only a TIFF preview in a 3FR.
    const image::EmbeddedPreview hasselblad = preview("RAW_HASSELBLAD_H3D39II.3FR", 256);
    CHECK(hasselblad.tiff);
    CHECK(hasselblad.width == 320);
    // Canon's TIFF previews are never taken, even where one would be big enough.
    CHECK_FALSE(preview("RAW_CANON_EOS_5DS.CR2", 256).tiff);
    CHECK(preview("RAW_CANON_EOS_5DS.CR2", 256).width == 8688);
    // A 4:3 EXIF thumbnail of a 3:2 photo has black bars: the next preview is taken instead.
    CHECK(preview("RAW_SONY_RX10M2.ARW", 128).width == 1616);
    CHECK(preview("RAW_NIKON_D800_L.TIFF", 128).width == 570);
    CHECK(preview("RAW_PANASONIC_DMC-GX-7_11.RW2", 128).width == 1920);
    CHECK(preview("RAW_PANASONIC_DMC-GX-7_43.RW2", 128).width == 160);
    // The GX8 crops its 4:3 preview to the aspect ratio shot; the GX7 stores it cropped.
    const image::EmbeddedPreview wide = preview("RAW_PANASONIC_GX8_169.RW2", 1024);
    REQUIRE(wide.crop);
    CHECK(wide.width == 1920);
    CHECK(wide.height == 1082);
    CHECK(wide.full_width == 5184);
    CHECK(wide.full_height == 2920);
    const image::EmbeddedPreview square = preview("RAW_PANASONIC_GX8_11.RW2", 1024);
    CHECK(square.width == 1440);
    CHECK(square.height == 1440);
    CHECK_FALSE(preview("RAW_PANASONIC_GX8_43.RW2", 1024).crop);
    CHECK_FALSE(preview("RAW_PANASONIC_DMC-GX-7_169.RW2", 1024).crop);
}
