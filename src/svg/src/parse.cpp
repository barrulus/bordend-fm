#include <charconv>
#include <cmath>

#include "internal.hpp"

namespace nordend::svg {
double number(std::string_view text, double fallback) {
    const auto first = text.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) {
        return fallback;
    }
    text.remove_prefix(first);
    text = text.substr(0, text.find_last_not_of(" \t\n\r") + 1);
    if (text.starts_with('+')) {
        text.remove_prefix(1);
    }
    double value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
            std::isfinite(value));
    return value;
}

Colour colour(std::string_view text) {
    if (text == "none") {
        return {};
    }
    if (text == "red") {
        return {1, 0, 0, 1};
    }
    if (text == "white") {
        return {1, 1, 1, 1};
    }
    if (text == "blue") {
        return {0, 0, 1, 1};
    }
    if (text == "lime") {
        return {0, 1, 0, 1};
    }
    if (text.starts_with('#') && (text.size() == 7 || text.size() == 4)) {
        unsigned value{};
        const auto parsed = std::from_chars(text.data() + 1, text.data() + text.size(), value, 16);
        require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size());
        if (text.size() == 4) {
            return {static_cast<double>((value >> 8U) & 15U) / 15,
                    static_cast<double>((value >> 4U) & 15U) / 15,
                    static_cast<double>(value & 15U) / 15, 1};
        }
        return {static_cast<double>((value >> 16U) & 255U) / 255,
                static_cast<double>((value >> 8U) & 255U) / 255,
                static_cast<double>(value & 255U) / 255, 1};
    }
    return {0, 0, 0, 1};
}
}  // namespace nordend::svg
