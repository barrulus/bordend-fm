#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

// Fuzzy matching for the command palette (ROADMAP 8a), on fzy's algorithm (third_party/fzy): the
// query's characters in order anywhere in the text, scored higher when they are together, at the
// start of a word or after a slash. Case does not matter (ASCII); bytes are compared as they are,
// so any UTF-8 text works, accented letters matching only themselves.
namespace nordend::views {

// fzy's limit: longer texts (or queries) never match.
inline constexpr std::size_t kFuzzyMaxLength = 1024;

// The score of text for query, higher is better; nullopt when it does not match. An empty query
// matches everything with 0.
[[nodiscard]] std::optional<double> fuzzy_score(std::string_view query, std::string_view text);

// The byte offsets in text of the query's characters as matched (best placement), ascending;
// empty when it does not match or the query is empty.
[[nodiscard]] std::vector<std::size_t> fuzzy_positions(std::string_view query,
                                                       std::string_view text);

}  // namespace nordend::views
