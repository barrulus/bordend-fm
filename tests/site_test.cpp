// The site's documentation (site/docs/index.html) lists the keyboard shortcuts from data in the page; this keeps that
// data the same as views::shortcuts(), the table Nordend itself works from.
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "nordend/views/shortcuts.hpp"

namespace {

constexpr std::string_view kDataOpen = R"(<script type="application/json" id="shortcut-data">)";

std::string json_string(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out + "\"";
}

// What the page's shortcut data must be: one object per line, in the overlay's order. Lines
// with no keys of their own (shown on the line before) are left out.
std::string expected_data() {
    std::string out = "[\n";
    bool first = true;
    for (const nordend::views::Shortcut& shortcut : nordend::views::shortcuts()) {
        if (shortcut.keys.empty()) {
            continue;
        }
        if (!first) {
            out += ",\n";
        }
        first = false;
        out += "  {\"group\": " + json_string(shortcut.group) +
               ", \"keys\": " + json_string(shortcut.keys) +
               ", \"action\": " + json_string(shortcut.action) + "}";
    }
    return out + "\n]";
}

}  // namespace

TEST_CASE("the site lists the same shortcuts as Nordend") {
    std::ifstream file{NORDEND_SITE};
    REQUIRE(file.good());
    std::ostringstream read;
    read << file.rdbuf();
    const std::string page = read.str();
    const std::size_t open = page.find(kDataOpen);
    REQUIRE(open != std::string::npos);
    const std::size_t begin = page.find('[', open + kDataOpen.size());
    const std::size_t close = page.find("</script>", open);
    REQUIRE(begin != std::string::npos);
    REQUIRE(close != std::string::npos);
    const std::size_t end = page.rfind(']', close);
    REQUIRE(end != std::string::npos);
    const std::string expected = expected_data();
    INFO("the data site/docs/index.html should carry:\n", expected);
    CHECK(page.substr(begin, end + 1 - begin) == expected);
}
