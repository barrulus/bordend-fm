// Stage 1 specifications, prepared before implementation and integrated after the stage 0 gate.
#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "nordend/picture/wuffs.hpp"
#include "nordend/svg/svg.hpp"
#include "svg_compare.hpp"

namespace {
auto svg(std::string_view body, std::string_view attrs = "width='64' height='64'", int size = 64) {
    const std::string source = "<svg xmlns='http://www.w3.org/2000/svg' " + std::string(attrs) +
                               ">" + std::string(body) + "</svg>";
    return nordend::svg::render(
        {reinterpret_cast<const std::uint8_t*>(source.data()), source.size()}, size);
}

std::array<unsigned, 4> pixel(const nordend::picture::Image& image, unsigned x, unsigned y) {
    REQUIRE(x < image.width);
    REQUIRE(y < image.height);
    const auto at = (static_cast<std::size_t>(y) * image.width + x) * 4;
    return {image.rgba[at], image.rgba[at + 1], image.rgba[at + 2], image.rgba[at + 3]};
}

std::vector<std::uint8_t> bytes(const std::string& name) {
    std::ifstream f{std::string(NORDEND_TEST_DATA) + "/svg/" + name, std::ios::binary};
    REQUIRE(f.good());
    return {std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
}
}  // namespace

TEST_CASE("svg stage1: committed shape and paint references") {
    for (const std::string name :
         {"solid", "shapes", "paths", "compact-path", "strokes", "css", "colours",
          "linear-gradient", "gradient-inheritance", "radial-gradient", "gradient-repeat", "uses",
          "transforms", "visibility", "aspect-ratio", "units"}) {
        CAPTURE(name);
        const auto image = nordend::svg::render(bytes(name + ".svg"), 64);
        const auto reference = nordend::picture::decode_wuffs(bytes(name + ".png"), 4096);
        const auto difference = nordend::test::svg_difference(image, reference.image);
        CAPTURE(difference.maximum);
        CAPTURE(difference.fraction);
        CHECK(difference.passes());
    }
}

TEST_CASE("svg stage1: stylesheet specificity, source order and inheritance") {
    const auto image =
        svg("<style>rect{fill:red}.a{fill:blue}#b{fill:lime}.a{fill:yellow}</style>"
            "<g color='purple' fill='currentColor'><rect width='16' height='64' class='a'/>"
            "<rect x='16' width='16' height='64' class='a' id='b'/>"
            "<rect x='32' width='16' height='64' class='a' style='fill:inherit'/>"
            "<rect x='48' width='16' height='64' fill='black' style='fill:orange'/></g>");
    CHECK(pixel(image, 8, 8) == std::array<unsigned, 4>{255, 255, 0, 255});
    CHECK(pixel(image, 24, 8) == std::array<unsigned, 4>{0, 255, 0, 255});
    CHECK(pixel(image, 40, 8) == std::array<unsigned, 4>{128, 0, 128, 255});
    CHECK(pixel(image, 56, 8) == std::array<unsigned, 4>{255, 165, 0, 255});
}

TEST_CASE("svg stage1: named colours and rgba percentages") {
    CHECK(pixel(svg("<rect width='64' height='64' fill='lightgoldenrodyellow'/>"), 0, 0) ==
          std::array<unsigned, 4>{250, 250, 210, 255});
    CHECK(pixel(svg("<rect width='64' height='64' fill='rgba(100%,0%,0%,50%)'/>"), 0, 0) ==
          std::array<unsigned, 4>{255, 0, 0, 128});
    CHECK(pixel(svg("<rect width='64' height='64' fill='currentColor'/>"), 0, 0) ==
          std::array<unsigned, 4>{0, 0, 0, 255});
    CHECK(pixel(svg("<rect width='64' height='64' fill='url(#missing) red'/>"), 0, 0) ==
          std::array<unsigned, 4>{255, 0, 0, 255});
}

TEST_CASE("svg stage1: viewBox sizing, units and preserveAspectRatio") {
    for (std::string unit :
         {"64px", "48pt", "4pc", "16.933333333mm", "1.6933333333cm", ".6666666667in"}) {
        CAPTURE(unit);
        const auto image = svg("<rect width='100%' height='100%' fill='red'/>",
                               "width='" + unit + "' height='32px'");
        CHECK(image.width == 64);
        CHECK(image.height == 32);
        CHECK(pixel(image, 63, 31) == std::array<unsigned, 4>{255, 0, 0, 255});
    }
    CHECK(svg("<rect width='64' height='32'/>", "viewBox='0 0 64 32'").height == 32);
    CHECK(svg("<rect width='64' height='32'/>", "width='100%' height='100%' viewBox='0 0 64 32'")
              .height == 32);
    for (std::string x : {"xMin", "xMid", "xMax"}) {
        for (std::string y : {"YMin", "YMid", "YMax"}) {
            for (std::string mode : {"meet", "slice"}) {
                CAPTURE(x);
                CAPTURE(y);
                CAPTURE(mode);
                const auto image =
                    svg("<rect width='16' height='16' fill='red'/>",
                        "width='64' height='32' viewBox='0 0 16 16' preserveAspectRatio='" + x + y +
                            " " + mode + "'");
                REQUIRE_FALSE(image.empty());
                const unsigned offset = x == "xMin" ? 0 : x == "xMid" ? 16 : 32;
                CHECK(pixel(image, mode == "slice" ? 0 : offset, 0) ==
                      std::array<unsigned, 4>{255, 0, 0, 255});
            }
        }
    }
    const auto stretch =
        svg("<rect width='16' height='16' fill='red'/>",
            "width='64' height='32' viewBox='0 0 16 16' preserveAspectRatio='none'");
    CHECK(pixel(stretch, 63, 31) == std::array<unsigned, 4>{255, 0, 0, 255});
}

TEST_CASE("svg stage1: references are bounded and never fetch resources") {
    CHECK(svg("<defs><g id='a'><use href='#a'/></g></defs><use href='#a'/>").empty());
    CHECK(svg("<defs><linearGradient id='a' href='#b'/><linearGradient id='b' "
              "href='#a'/></defs><rect width='64' height='64' fill='url(#a)'/>")
              .empty());
    const auto image =
        svg("<use href='file:///etc/passwd'/><image "
            "href='https://example.com/a.png'/><text>ignored</text><rect width='64' height='64' "
            "fill='red'/>");
    CHECK(pixel(image, 0, 0) == std::array<unsigned, 4>{255, 0, 0, 255});
    std::string defs = "<defs><g id='a0'><rect width='1' height='1'/></g>";
    for (int i = 1; i < 16; ++i) {
        defs += "<g id='a" + std::to_string(i) + "'><use href='#a" + std::to_string(i - 1) +
                "'/><use href='#a" + std::to_string(i - 1) + "'/></g>";
    }
    CHECK(svg(defs + "</defs><use href='#a15'/>").empty());
}

TEST_CASE("svg stage1: linear gradient pixel centres and stop alpha") {
    const auto image = svg(
        "<defs><linearGradient id='g'><stop stop-color='red'/><stop offset='1' "
        "stop-color='blue'/></linearGradient></defs><rect width='64' height='64' fill='url(#g)'/>");
    CHECK(pixel(image, 0, 0) == std::array<unsigned, 4>{253, 0, 2, 255});
    CHECK(pixel(image, 31, 0) == std::array<unsigned, 4>{129, 0, 126, 255});
    CHECK(pixel(image, 63, 0) == std::array<unsigned, 4>{2, 0, 253, 255});
}

TEST_CASE("svg stage1: path grammar and segment cap") {
    CHECK(svg("<path d='M0 0Lnan 1'/>").empty());
    CHECK(svg("<path d='M0 0A1 1 0 2 0 1 1'/>").empty());
    std::string path = "<path d='M0 0";
    for (int i = 0; i < 1000001; ++i) {
        path += "l0 0";
    }
    CHECK(svg(path + "'/>").empty());
}

TEST_CASE("svg stage1: empty stroked subpaths and drawing after close are safe") {
    CHECK_FALSE(svg("<path d='M1 1 M2 2 L10 2' fill='none' stroke='red'/>").empty());
    CHECK_FALSE(svg("<path d='M1 1 L1 1 M2 2 Z M3 3 L3 3' fill='none' stroke='red' "
                    "stroke-linecap='round'/>")
                    .empty());
    CHECK_FALSE(svg("<path d='M1 1L10 1L10 10ZL12 12' fill='none' stroke='red'/>").empty());
}

TEST_CASE("svg stage1: symbol and nested SVG viewports clip overflowing content") {
    const auto image =
        svg("<defs><symbol id='s' viewBox='0 0 10 10'><rect x='-10' y='-10' width='30' height='30' "
            "fill='red'/></symbol></defs><use href='#s' x='16' y='16' width='16' height='16'/>");
    CHECK(pixel(image, 15, 16)[3] == 0);
    CHECK(pixel(image, 16, 16) == std::array<unsigned, 4>{255, 0, 0, 255});
    CHECK(pixel(image, 31, 31) == std::array<unsigned, 4>{255, 0, 0, 255});
    CHECK(pixel(image, 32, 31)[3] == 0);
    const auto nested =
        svg("<svg x='16' y='16' width='16' height='16' viewBox='0 0 10 10'><rect x='-10' y='-10' "
            "width='30' height='30'/></svg>");
    CHECK(pixel(nested, 15, 16)[3] == 0);
    CHECK(pixel(nested, 16, 16)[3] == 255);
    CHECK(pixel(nested, 32, 16)[3] == 0);
}

TEST_CASE("svg stage1: stylesheet expansion and live viewport layers are bounded") {
    std::string css = "<style>";
    for (int i = 0; i < 101; ++i) {
        css += (i ? ",.x" : ".x") + std::to_string(i);
    }
    css += "{";
    for (int i = 0; i < 1000; ++i) {
        css += "property" + std::to_string(i) + ":value;";
    }
    CHECK(svg(css + "}</style>").empty());
    std::string nested;
    for (int i = 0; i < 17; ++i) {
        nested += "<svg width='64' height='64'>";
    }
    nested += "<rect width='64' height='64'/>";
    for (int i = 0; i < 17; ++i) {
        nested += "</svg>";
    }
    CHECK(svg(nested).empty());
}

TEST_CASE("svg stage1: a closed dash without gaps retains the closing join") {
    const std::string path =
        "<path d='M16 16H48V48H16Z' fill='none' stroke='red' stroke-width='5' "
        "stroke-linecap='square' ";
    const auto solid = svg(path + "/>");
    const auto dashed = svg(path + "stroke-dasharray='1000 1'/>");
    REQUIRE_FALSE(solid.empty());
    CHECK(solid.rgba == dashed.rgba);
}

TEST_CASE("svg stage1: duplicate XML attributes and non-finite transforms are rejected") {
    CHECK(svg("<rect width='1' width='2' height='1'/>").empty());
    CHECK(svg("<rect width='64' height='64' transform='matrix(1 0 0 1 nan 0)'/>").empty());
    CHECK(svg("<rect width='64' height='64' transform='scale(1e300)'/>").empty());
}

TEST_CASE("svg stage1: dash fragments that collapse to one fixed point are safe") {
    CHECK_FALSE(svg("<path d='M7 14420 L34 14' fill='none' stroke='red' stroke-width='5' "
                    "stroke-dasharray='1 3' stroke-dashoffset='2'/>",
                    "width='64' height='64'", 16)
                    .empty());
    CHECK_FALSE(svg("<path d='M1 1L1.00000001 1' fill='none' stroke='red' stroke-width='.000001' "
                    "stroke-linecap='round'/>")
                    .empty());
}
