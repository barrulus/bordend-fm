#include "nordend/svg/svg.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "nordend/picture/wuffs.hpp"
#include "svg_compare.hpp"

namespace {
nordend::picture::Image render(std::string_view source, int pixels = 64) {
    return nordend::svg::render(
        {reinterpret_cast<const std::uint8_t*>(source.data()), source.size()}, pixels);
}
}  // namespace

TEST_CASE("svg: invalid input and dimensions fail cleanly") {
    CHECK(render("").empty());
    CHECK(render("not SVG").empty());
    CHECK(render("<svg/>").empty());
    CHECK(render("<svg width='nan' height='1'/>").empty());
    CHECK(render("<svg width='1e999' height='1'/>").empty());
    CHECK(render("<svg width='64' height='64'/>", 0).empty());
    CHECK(render("<svg width='64' height='64'/>", 1000000).empty());
    CHECK(render(std::string(8U * 1024U * 1024U + 1U, ' ')).empty());
    CHECK(render("<svg width='64' height='64'><rect></svg>").empty());
    CHECK(render("<svg width='64' height='64'/><svg/>").empty());
}

TEST_CASE("svg: rectangle retains aspect ratio and straight alpha") {
    const auto image = render(
        "<svg xmlns='http://www.w3.org/2000/svg' width='20' height='10'>"
        "<rect width='20' height='10' fill='#ff0000' opacity='.5'/></svg>",
        80);
    REQUIRE_FALSE(image.empty());
    CHECK(image.width == 80);
    CHECK(image.height == 40);
    REQUIRE(image.rgba.size() == 80 * 40 * 4);
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        CHECK(image.rgba[i] == 255);
        CHECK(image.rgba[i + 1] == 0);
        CHECK(image.rgba[i + 2] == 0);
        CHECK(image.rgba[i + 3] == 128);
    }
}

TEST_CASE("svg: XML nesting and element limits reject before rendering") {
    std::string source = "<svg width='1' height='1'>";
    for (int i = 0; i < 257; ++i) {
        source += "<g>";
    }
    for (int i = 0; i < 257; ++i) {
        source += "</g>";
    }
    source += "</svg>";
    CHECK(render(source).empty());
    source = "<svg width='1' height='1'>";
    for (int i = 0; i < 100001; ++i) {
        source += "<g/>";
    }
    CHECK(render(source + "</svg>").empty());
}

TEST_CASE("svg: XML declarations, comments, namespaces and entities") {
    const auto image = render(
        "<?xml version='1.0'?><!-- icon -->"
        "<!DOCTYPE svg [<!ENTITY ignored 'red'>]>"
        "<s:svg xmlns:s='http://www.w3.org/2000/svg' width='1' height='1'>"
        "<s:rect width='1' height='1' fill='&#35;ff0000'/></s:svg>",
        1);
    REQUIRE_FALSE(image.empty());
    CHECK(image.rgba == std::vector<std::uint8_t>{255, 0, 0, 255});
    CHECK(render("<!DOCTYPE svg [<!ENTITY x SYSTEM 'file:///etc/passwd'>]>"
                 "<svg width='1' height='1'>&x;</svg>")
              .empty());
}

TEST_CASE("svg: large coordinates and raster work fail cleanly") {
    CHECK(render("<svg width='1' height='1'><rect width='1e300' height='1'/></svg>").empty());
    std::string source = "<svg width='64' height='64'>";
    for (int i = 0; i < 65; ++i) {
        source += "<rect width='64' height='64'/>";
    }
    CHECK(render(source + "</svg>", 1024).empty());
}

TEST_CASE("svg: solid rectangle matches its captured reference") {
    const auto read = [](std::string_view suffix) {
        std::ifstream input{std::string{NORDEND_TEST_DATA} + "/svg/solid." + std::string{suffix},
                            std::ios::binary};
        REQUIRE(input.good());
        return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{input},
                                         std::istreambuf_iterator<char>{}};
    };
    const auto image = nordend::svg::render(read("svg"), 64);
    const auto reference = nordend::picture::decode_wuffs(read("png"), 4096);
    REQUIRE_FALSE(reference.image.empty());
    CHECK(nordend::test::svg_difference(image, reference.image).passes());
}

TEST_CASE("svg: reference comparison tolerates transparent edges but catches opaque errors") {
    const nordend::picture::Image transparent{.width = 1, .height = 1, .rgba = {0, 0, 0, 0}};
    const nordend::picture::Image edge{.width = 1, .height = 1, .rgba = {255, 255, 0, 1}};
    const nordend::picture::Image opaque{.width = 1, .height = 1, .rgba = {255, 255, 0, 255}};
    CHECK(nordend::test::svg_difference(edge, transparent).passes());
    CHECK_FALSE(nordend::test::svg_difference(opaque, transparent).passes());
}
