#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "nordend/core/config.hpp"

using nordend::core::parse_config;
using nordend::core::set_config_value;
using nordend::core::toml_string;

namespace {

// The example's shape: comments above and beside, an unknown key, sections in this order.
constexpr std::string_view kFile = R"(# My Nordend settings
# (hand written)

[font]
# the interface
family = "Inter"   # was "sans-serif"
monospace = 'JetBrains Mono'
weight_hint = 3

[pointer]
double_click_ms   =   400 # slower hands
)";

std::string set(std::string_view text, std::string_view section, std::string_view key,
                std::string_view value) {
    return set_config_value(text, section, key, value);
}

}  // namespace

TEST_CASE("config edit: only the value changes, its comment and spacing stay") {
    CHECK(set(kFile, "font", "family", toml_string("IBM Plex Sans")) ==
          std::string{kFile}.replace(std::string{kFile}.find("\"Inter\""), 7, "\"IBM Plex Sans\""));
    const std::string ms = set(kFile, "pointer", "double_click_ms", "650");
    CHECK(ms.find("double_click_ms   =   650 # slower hands\n") != std::string::npos);
    CHECK(ms.size() == kFile.size());
    // A literal string is replaced whole.
    const std::string mono = set(kFile, "font", "monospace", toml_string("Iosevka"));
    CHECK(mono.find("monospace = \"Iosevka\"\n") != std::string::npos);
    CHECK(mono.find("JetBrains") == std::string::npos);
    // The result reads as the new value, the unknown key still reported as before.
    const auto parsed = parse_config(ms, "config.toml");
    CHECK(parsed.config.pointer.double_click_ms == 650);
    CHECK(parsed.config.font.family == "Inter");
    REQUIRE(parsed.problems.size() == 1);
    CHECK(parsed.problems[0].find("weight_hint") != std::string::npos);
}

TEST_CASE(
    "config edit: a missing key goes at the end of its section, a missing section at the end") {
    const std::string distance = set(kFile, "pointer", "double_click_distance", "8");
    CHECK(distance.ends_with(
        "double_click_ms   =   400 # slower hands\ndouble_click_distance = 8\n"));
    const std::string font = set("[font]\nfamily = \"Inter\"\n\n[pointer]\n", "font", "monospace",
                                 toml_string("Iosevka"));
    CHECK(font == "[font]\nfamily = \"Inter\"\nmonospace = \"Iosevka\"\n\n[pointer]\n");
    // An empty section: under its header.
    CHECK(set("[pointer]\n\n[font]\nfamily = \"Inter\"\n", "pointer", "double_click_ms", "500") ==
          "[pointer]\ndouble_click_ms = 500\n\n[font]\nfamily = \"Inter\"\n");
    // No such section: added, after a blank line.
    CHECK(set("# nothing yet\n", "font", "family", toml_string("Inter")) ==
          "# nothing yet\n\n[font]\nfamily = \"Inter\"\n");
    CHECK(set("# no newline", "font", "family", toml_string("Inter")) ==
          "# no newline\n\n[font]\nfamily = \"Inter\"\n");
    CHECK(set("", "font", "family", toml_string("Inter")) == "[font]\nfamily = \"Inter\"\n");
}

TEST_CASE("config edit: dotted and quoted keys, strings with # and quotes, multi-line strings") {
    CHECK(set("font.family = \"Inter\" # top\n", "font", "family", toml_string("Lato")) ==
          "font.family = \"Lato\" # top\n");
    CHECK(set("[font]\n\"family\" = \"Inter\"\n", "font", "family", toml_string("Lato")) ==
          "[font]\n\"family\" = \"Lato\"\n");
    CHECK(set("[ font ]\nfamily=\"a#b\\\"c\"#x\n", "font", "family", toml_string("Lato")) ==
          "[ font ]\nfamily=\"Lato\"#x\n");
    CHECK(set("[font]\nfamily = \"\"\"Inter\nBold\"\"\" # two lines\nmonospace = \"m\"\n", "font",
              "family",
              toml_string("Lato")) == "[font]\nfamily = \"Lato\" # two lines\nmonospace = \"m\"\n");
    // Another section's key of the same name is left alone.
    CHECK(set("[other]\nfamily = 1\n[font]\nfamily = \"Inter\"\n", "font", "family",
              toml_string("Lato")) == "[other]\nfamily = 1\n[font]\nfamily = \"Lato\"\n");
}

TEST_CASE("toml_string escapes what TOML needs") {
    CHECK(toml_string("Inter") == "\"Inter\"");
    CHECK(toml_string("a\"b\\c") == "\"a\\\"b\\\\c\"");
    CHECK(toml_string("tab\there") == "\"tab\\there\"");
    CHECK(toml_string(std::string{"\x01"}) == "\"\\u0001\"");
    const auto parsed = parse_config(
        std::string{"[font]\nfamily = "} + toml_string("a\"b\\c") + "\n", "config.toml");
    CHECK(parsed.problems.empty());
    CHECK(parsed.config.font.family == "a\"b\\c");
}

TEST_CASE("config_problem: the key's own problem, readable") {
    using nordend::core::config_problem;
    const auto slow = parse_config("[pointer]\ndouble_click_ms = 5\n", "config.toml");
    CHECK(config_problem(slow, "pointer", "double_click_ms") ==
          "must be between 100 and 2000, got 5");
    CHECK(config_problem(slow, "font", "family").empty());
    const auto empty = parse_config("[font]\nfamily = \"\"\n", "config.toml");
    CHECK(config_problem(empty, "font", "family") == "must not be empty");
    // Another key's problem is not this one's.
    const auto other = parse_config("[font]\nweight = 3\nfamily = \"Inter\"\n", "config.toml");
    CHECK(config_problem(other, "font", "family").empty());
    // A file that no longer parses.
    const auto broken = parse_config("[font\n", "config.toml");
    CHECK_FALSE(config_problem(broken, "font", "family").empty());
}
