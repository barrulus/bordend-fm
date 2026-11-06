#include "nordend/views/fuzzy.hpp"

#include <cctype>
#include <cmath>
#include <string>

#include "fzy/src/match.h"  // NOLINT(misc-include-cleaner): has_match, match, match_positions

namespace nordend::views {
namespace {

// fzy widens only lowercase query characters to either case: an uppercase one would miss a
// lowercase letter. The text it lowercases itself.
std::string lowercase(std::string_view text) {
    std::string out{text};
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool usable(std::string_view query, std::string_view text) {
    return query.size() <= text.size() && text.size() <= kFuzzyMaxLength &&
           query.find('\0') == std::string_view::npos && text.find('\0') == std::string_view::npos;
}

}  // namespace

std::optional<double> fuzzy_score(std::string_view query, std::string_view text) {
    if (query.empty()) {
        return 0.0;
    }
    if (!usable(query, text)) {
        return std::nullopt;
    }
    const std::string needle = lowercase(query);
    const std::string haystack{text};
    if (has_match(needle.c_str(), haystack.c_str()) == 0) {
        return std::nullopt;
    }
    const double score = match(needle.c_str(), haystack.c_str());
    // fzy gives +infinity to a whole-text match and -infinity when it cannot tell: kept finite so
    // they still sort.
    if (std::isinf(score)) {
        return score > 0.0 ? 1e9 : -1e9;
    }
    return score;
}

std::vector<std::size_t> fuzzy_positions(std::string_view query, std::string_view text) {
    if (query.empty() || !usable(query, text)) {
        return {};
    }
    const std::string needle = lowercase(query);
    const std::string haystack{text};
    if (has_match(needle.c_str(), haystack.c_str()) == 0) {
        return {};
    }
    std::vector<std::size_t> positions(needle.size());
    match_positions(needle.c_str(), haystack.c_str(), positions.data());
    return positions;
}

}  // namespace nordend::views
